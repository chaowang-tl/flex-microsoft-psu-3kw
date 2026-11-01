/*
    [2026] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip
    software and any derivatives exclusively with Microchip products.
    You are responsible for complying with 3rd party license terms
    applicable to your use of 3rd party software (including open source
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.?
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR
    THIS SOFTWARE.
*/

/*******************************************************************************
* File Name       : grid_meas.c
* Description     : Grid voltage and input current RMS (PU and physical units)
*
*   Window: exactly one line cycle, delimited by the PLL theta wrap
*   (2*pi -> 0). Forced latch after GRID_MEAS_N_MAX samples when the
*   PLL is not tracking, so the sums never grow without bound.
*
*   Phase-A ISR (65 kHz): sumV += vac^2, sumI += iin^2, n++
*                         iin = |iA| + |iB| (reconstructed inductor currents)
*   T1 (10 kHz):          rms = sqrt(sum / n) [PU], then x BASE -> V / A
*
*   Hand-off: the ISR copies the sums into the latch only while
*   measLatchReady is false; T1 reads the latch only while it is true and
*   clears it afterwards, so T1 always sees a consistent snapshot even
*   though the priority-5 ISR can preempt it.
*
*   X-capacitor discharge (RA4 high = bleeder on), T1, every state:
*   AC unplugged leaves a DC voltage on the X capacitor, so there is no zero
*   crossing. Uses the raw vac, not the PLL (the PLL may free-run).
*     zero crossing  : vac passes from above +HYST to below -HYST or back
*     start          : no crossing for APP_XCAP_NO_CROSS_MS and |vac| > ON
*     stop           : |vac| < OFF, APP_XCAP_MAX_MS elapsed, or a crossing
*   After a time-out stop, no new pulse until a crossing is seen again, so
*   one AC removal gives at most one pulse (bleeder thermal limit).
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/28
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#define GRID_MEAS_C

//------------------------------------------------------------------------------
// Include
#include "grid_meas.h"
#include "../../config/app_config.h"
#include "../../drivers/pfc_io/pfc_io.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions
#define XCAP_TICKS_PER_MS   BOARD_SLOW_TICKS_PER_MS     // T1 ticks per ms (board_config.h)
#define XCAP_NO_CROSS_TICKS (APP_XCAP_NO_CROSS_MS * XCAP_TICKS_PER_MS)
#define XCAP_MAX_TICKS      (APP_XCAP_MAX_MS * XCAP_TICKS_PER_MS)
#define XCAP_VAC_ON_PU      (APP_XCAP_VAC_ON_V / VAC_BASE)
#define XCAP_VAC_OFF_PU     (APP_XCAP_VAC_OFF_V / VAC_BASE)
#define XCAP_ZC_HYST_PU     (APP_XCAP_ZC_HYST_V / VAC_BASE)

//------------------------------------------------------------------------------
// Public Variables
volatile float g_pfcVacRmsPU;
volatile float g_pfcIinRmsPU;
volatile float g_pfcVacRms;
volatile float g_pfcIinRms;
volatile bool     g_xcapDischarging;
volatile uint32_t g_xcapEvents;
volatile uint16_t g_xcapNoCrossMs;

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes
static void GridXcapUpdate(void);
static void GridXcapSet(bool on);
//------------------------------------------------------------------------------
// Private Variables
// Running sums (phase-A ISR only)
float    measSumV;
float    measSumI;
uint32_t measCnt;
float    measThetaPrev;

// One-cycle snapshot (ISR -> T1)
volatile float    measSumVLatch;
volatile float    measSumILatch;
volatile uint32_t measCntLatch;
volatile bool     measLatchReady;

// X-capacitor discharge (T1 only)
int8_t   xcapSign;              // +1 / -1 last polarity beyond the hysteresis, 0 unknown
uint32_t xcapNoCrossTicks;      // T1 ticks since the last zero crossing
uint32_t xcapOnTicks;           // Duration of the running discharge
bool     xcapSpent;             // Pulse timed out; wait for a crossing before another

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : GRID_MEAS_Init
* Description     : Clear the RMS accumulators and outputs, X-cap bleeder off
* Purpose         : Start from a clean window
* Input           : None
* Return          : None
* Calls           : GridXcapSet
* Called by       : SMPS_SM_Init
* Note            : Must run before interrupts are enabled. The no-crossing
*                   timer starts at 0, so a DC input at power-up is detected
*                   after APP_XCAP_NO_CROSS_MS
*******************************************************************************/
void GRID_MEAS_Init(void)
{
    GridXcapSet(false);
    xcapSign         = 0;
    xcapNoCrossTicks = 0U;
    xcapOnTicks      = 0U;
    xcapSpent        = false;
    g_xcapEvents     = 0U;
    g_xcapNoCrossMs  = 0U;

    measSumV       = 0.0f;
    measSumI       = 0.0f;
    measCnt        = 0U;
    measThetaPrev  = 0.0f;

    measSumVLatch  = 0.0f;
    measSumILatch  = 0.0f;
    measCntLatch   = 0U;
    measLatchReady = false;

    g_pfcVacRmsPU    = 0.0f;
    g_pfcIinRmsPU    = 0.0f;
    g_pfcVacRms      = 0.0f;
    g_pfcIinRms      = 0.0f;
}

/*******************************************************************************
* Public Function : GRID_MEAS_SampleUpdate
* Description     : Accumulate squared samples and latch on each line cycle
* Purpose         : Fast part of the RMS (no sqrt / division at 65 kHz)
* Input           : None
* Return          : None
* Calls           : SOGI_PLL_GetAngle
* Called by       : SMPS_SM_TaskPhaseA (65 kHz, after the PLL update)
* Note            : If T1 has not consumed the previous snapshot, the new one
*                   is dropped (sums are still restarted)
*******************************************************************************/
void GRID_MEAS_SampleUpdate(void)
{
    float vac   = g_pfcVacPU;
    float iin   = g_pfcIaPU + g_pfcIbPU;
    float theta = SOGI_PLL_GetAngle(&g_pll);

    measSumV += vac * vac;
    measSumI += iin * iin;
    measCnt++;

    if ((theta < measThetaPrev) || (measCnt >= GRID_MEAS_N_MAX))
    {
        if (!measLatchReady)
        {
            measSumVLatch  = measSumV;
            measSumILatch  = measSumI;
            measCntLatch   = measCnt;
            measLatchReady = true;
        }

        measSumV = 0.0f;
        measSumI = 0.0f;
        measCnt  = 0U;
    }

    measThetaPrev = theta;
}

/*******************************************************************************
* Public Function : GRID_MEAS_TaskSlow
* Description     : Finish the RMS from the latched snapshot, convert to V / A
* Purpose         : Slow part of the RMS (sqrt and division); X-cap discharge
* Input           : None
* Return          : None
* Calls           : sqrtf, GridXcapUpdate
* Called by       : SMPS_SM_TaskSlow (100 us, every state)
* Note            : Outputs refresh once per line cycle (~20 ms at 50 Hz)
*******************************************************************************/
void GRID_MEAS_TaskSlow(void)
{
    if (measLatchReady)
    {
        float invCnt = 1.0f / (float)measCntLatch;

        g_pfcVacRmsPU = sqrtf(measSumVLatch * invCnt);
        g_pfcIinRmsPU = sqrtf(measSumILatch * invCnt);
        measLatchReady = false;
    }

    g_pfcVacRms = g_pfcVacRmsPU * VAC_BASE;
    g_pfcIinRms = g_pfcIinRmsPU * IL_BASE;

    GridXcapUpdate();
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation
/*******************************************************************************
* Private Function : GridXcapUpdate
* Description     : Detect AC removal and drive the X-capacitor bleeder (RA4)
* Purpose         : Bring the inlet voltage down after unplugging without a
*                   permanent bleeder loss while the grid is present
* Input           : None
* Return          : None
* Calls           : GridXcapSet
* Called by       : GRID_MEAS_TaskSlow (T1, 100 us)
* Note            : Rules in the file header. A grid dip (|vac| near 0) does
*                   not start a discharge because |vac| stays below ON
*******************************************************************************/
static void GridXcapUpdate(void)
{
#if APP_XCAP_ENABLE
    float vac    = g_pfcVacPU;
    float vacAbs = fabsf(vac);
    bool  cross  = false;

    if (vac > XCAP_ZC_HYST_PU)
    {
        cross    = (xcapSign < 0);
        xcapSign = 1;
    }
    else if (vac < -XCAP_ZC_HYST_PU)
    {
        cross    = (xcapSign > 0);
        xcapSign = -1;
    }

    if (cross)
    {
        xcapNoCrossTicks = 0U;
        xcapSpent        = false;
    }
    else if (xcapNoCrossTicks < XCAP_NO_CROSS_TICKS)
    {
        xcapNoCrossTicks++;
    }
    g_xcapNoCrossMs = (uint16_t)(xcapNoCrossTicks / XCAP_TICKS_PER_MS);

    if (g_xcapDischarging)
    {
        xcapOnTicks++;

        if (cross || (vacAbs < XCAP_VAC_OFF_PU))
        {
            GridXcapSet(false);
        }
        else if (xcapOnTicks >= XCAP_MAX_TICKS)
        {
            GridXcapSet(false);
            xcapSpent = true;
        }
    }
    else if (!xcapSpent && (xcapNoCrossTicks >= XCAP_NO_CROSS_TICKS)
             && (vacAbs > XCAP_VAC_ON_PU))
    {
        xcapOnTicks = 0U;
        g_xcapEvents++;
        GridXcapSet(true);
    }
#endif
}

/*******************************************************************************
* Private Function : GridXcapSet
* Description     : Switch the X-capacitor bleeder
* Purpose         : Single place that drives RA4
* Input           : on - true = discharge (RA4 high)
* Return          : None
* Calls           : PFC_IO_XcapSet
* Called by       : GRID_MEAS_Init, GridXcapUpdate
* Note            : RA4 direction and reset level are set by PFC_IO_Init
*******************************************************************************/
static void GridXcapSet(bool on)
{
    PFC_IO_XcapSet(on);
    g_xcapDischarging = on;
}

/******************************************************************************/
#undef GRID_MEAS_C
