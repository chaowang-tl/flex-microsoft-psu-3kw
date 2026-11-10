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
* File Name       : user_isr.c
* Description     : User interrupt service routine implementation
*
*   ISR             Rate     Prio  Work
*   _AD5CH1Interrupt 65 kHz  5     iA, vac -> PU; SMPS_SM_TaskPhaseA
*   _AD1CH1Interrupt 65 kHz  5     iB, vout -> PU; RD6 load; SMPS_SM_TaskPhaseB;
*                                  X2Cscope_Update
*   _T1Interrupt     10 kHz  1     temperatures -> PU; SMPS_SM_TaskSlow;
*                                  UART1 bytes; tick
*
*   ADC to PU: pu = (int32)(DATA - OFFSET) * k, where k = FB_SCALE / BASE *
*   ADC gain-calibration factor, folded into one float per channel by
*   USER_ISR_AdcScaleInit so each conversion costs one multiply.
*
*   Inductor-current CT offsets come from il_cal.c: while a phase is
*   calibrating its ISR feeds the raw counts to IL_CAL_Update; until then
*   the default mid-scale offset is used.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/09
* Modified by     : Chao Wang
* Modified on     : 2026/11/10
*******************************************************************************/
#define USER_ISR_C

//------------------------------------------------------------------------------
// Include
#include "user_isr.h"
#include "../apps/meas/il_cal.h"
#include "../config/board_config.h"
#include "../third_party/X2Cscope/X2Cscope.h"
#include "../drivers/mcc_extended/mcc_ext.h"
#include "../scheduler/scheduler.h"
#include "../apps/control/smps_control.h"
#include "../apps/control/smps_sm.h"
#include "../apps/comm/llc_link.h"
#include "../drivers/llc_signal/llc_signal.h"
#include "../config/app_config.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions
// ADC sensing scales and offsets: board_config.h (BOARD_FB_*)
#define FB_VAC_SCALE            BOARD_FB_VAC_SCALE
#define FB_VAC_L_OFFSET         BOARD_FB_VAC_L_OFFSET
#define FB_VAC_N_OFFSET         BOARD_FB_VAC_N_OFFSET
#define FB_VOUT_SCALE           BOARD_FB_VOUT_SCALE
#define FB_VOUT_OFFSET          BOARD_FB_VOUT_OFFSET
#define FB_IL_SCALE             BOARD_FB_IL_SCALE
#define FB_TEMP_SCALE           BOARD_FB_TEMP_SCALE
#define FB_TEMP_OFFSET          BOARD_FB_TEMP_OFFSET

#define K_VAC_PU                (FB_VAC_SCALE / VAC_BASE)
#define K_VOUT_PU               (FB_VOUT_SCALE / VOUT_BASE)
#define K_IL_PU                 (FB_IL_SCALE / IL_BASE)
#define K_TEMP_PU               (FB_TEMP_SCALE / TEMP_BASE)

#define ADC_GAIN_FRAC_SCALE     (1.0f / 262144.0f)  // g_adcxGainCoeff is 18-bit fractional

// Signed count difference (int32, so currents below offset stay negative)
#define ADC_TO_PU(data, offset, k)  ((float)((int32_t)(data) - (int32_t)(offset)) * (k))

#if APP_PLL_PWM_TEST_ENABLE
// Internal test grid: 50 Hz, 1.0 PU peak, sampled at PFC_FAST_LOOP_HZ
#define PLL_TEST_TWO_PI         6.28318530f
#define PLL_TEST_DELTA_THETA    (PLL_TEST_TWO_PI * 50.0f / PFC_FAST_LOOP_HZ)   // 0.00483322 rad
#endif

//------------------------------------------------------------------------------
// Public Variables

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes
static float AdcGainGet(ADC_SHARED_CORE_e core);
//------------------------------------------------------------------------------
// Private Variables
// Per-channel PU factors including ADC gain calibration (set once at init)
float kVacPU  = K_VAC_PU;       // AD2
float kVoutPU = K_VOUT_PU;      // AD3
float kIaPU   = K_IL_PU;        // AD5
float kIbPU   = K_IL_PU;        // AD1
float kTemp3PU = K_TEMP_PU;     // AD3
float kTemp4PU = K_TEMP_PU;     // AD4
#if APP_PLL_PWM_TEST_ENABLE
volatile float pllTestTheta;        // Internal test-grid phase [rad]
#endif

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : USER_ISR_AdcScaleInit
* Description     : Fold ADC gain calibration into per-channel PU factors
* Purpose         : Keep the 65 kHz conversions to a single multiply
* Input           : None
* Return          : None
* Calls           : AdcGainGet
* Called by       : main (after MCC_EXT_SystemInitialize, before interrupts)
* Note            : Uses g_adcxGainCoeff computed by mcc_ext AdcInitExt
*******************************************************************************/
void USER_ISR_AdcScaleInit(void)
{
    kIbPU    = K_IL_PU   * AdcGainGet(ADC_SHARED_CORE_1);
    kVacPU   = K_VAC_PU  * AdcGainGet(ADC_SHARED_CORE_2);
    kVoutPU  = K_VOUT_PU * AdcGainGet(ADC_SHARED_CORE_3);
    kTemp3PU = K_TEMP_PU * AdcGainGet(ADC_SHARED_CORE_3);
    kTemp4PU = K_TEMP_PU * AdcGainGet(ADC_SHARED_CORE_4);
    kIaPU    = K_IL_PU   * AdcGainGet(ADC_SHARED_CORE_5);
}

/*******************************************************************************
* Public Function : _AD5CH1Interrupt
* Description     : Phase-A sampling ISR (PWM3 trigger, 65 kHz)
* Purpose         : Convert phase-A currents and grid voltage to PU,
*                   then run the phase-A task (PLL, polarity, current loop A)
* Input           : None (hardware interrupt)
* Return          : None
* Calls           : IL_CAL_Update, SMPS_SM_TaskPhaseA
* Called by       : Hardware
* Note            : Priority 5 (MCC interrupt.c), same as phase-B ISR.
*                   Measured execution time ~4.4 us (of the 15.38 us period).
*                   TODO: confirm AD2 conversions are complete when AD5CH1
*                   finishes (otherwise trigger on the last channel or check RDY)
*******************************************************************************/
void __attribute__ ( ( __interrupt__ ) ) _AD5CH1Interrupt ( void )
{
    if (!g_userIsrReady) { IFS7bits.AD5CH1IF = 0U; return; }

#if APP_PLL_PWM_TEST_ENABLE
    pllTestTheta += PLL_TEST_DELTA_THETA;
    if (pllTestTheta > PLL_TEST_TWO_PI)
    {
        pllTestTheta -= PLL_TEST_TWO_PI;
    }
    g_pfcVacPU = sinf(pllTestTheta);
    // No power stage on the bench: floating CT readings must not trip OCP
    g_pfcIaUpPU = 0.0f;
    g_pfcIaDnPU = 0.0f;
#else
    if (g_ilCalA.active)
    {
        IL_CAL_Update(&g_ilCalA, (uint32_t)AD5CH0DATA, (uint32_t)AD5CH1DATA);
    }

    g_pfcIaUpPU = ADC_TO_PU(AD5CH0DATA, g_ilCalA.offsetUp, kIaPU);
    g_pfcIaDnPU = ADC_TO_PU(AD5CH1DATA, g_ilCalA.offsetDn, kIaPU);
    g_pfcVacPU  = ADC_TO_PU(AD2CH0DATA, FB_VAC_L_OFFSET, kVacPU)
                - ADC_TO_PU(AD2CH1DATA, FB_VAC_N_OFFSET, kVacPU);
#endif

    SMPS_SM_TaskPhaseA();

    IFS7bits.AD5CH1IF = 0U;
}

/*******************************************************************************
* Public Function : _AD1CH1Interrupt
* Description     : Phase-B sampling ISR (PWM4 trigger, 65 kHz, 180 deg after A)
* Purpose         : Convert phase-B currents and bulk voltage to PU, read the
*                   LLC load (RD6), then run the phase-B task (current loop B,
*                   decimated voltage loop) and scope sampling
* Input           : None (hardware interrupt)
* Return          : None
* Calls           : IL_CAL_Update, LLC_SIGNAL_LoadUpdate, SMPS_SM_TaskPhaseB,
*                   X2Cscope_Update
* Called by       : Hardware
* Note            : Priority 5, same as phase-A ISR (no mutual preemption).
*                   AD3CH0 (vout) is triggered by PG4TRIGA together with AD1.
*                   RD6 is read in this ISR, every state, so the load
*                   feed-forward is updated in the voltage-loop context.
*                   Measured execution time ~4.6 us (of the 15.38 us period).
*                   TODO: confirm AD3CH0 is complete when AD1CH1 finishes
*                   (otherwise trigger on the last channel or check RDY)
*******************************************************************************/
void __attribute__ ( ( __interrupt__ ) ) _AD1CH1Interrupt ( void )
{
    if (!g_userIsrReady) { IFS4bits.AD1CH1IF = 0U; return; }

#if APP_PLL_PWM_TEST_ENABLE
    g_pfcVoutPU = 1.0f;
    g_pfcIbUpPU = 0.0f;
    g_pfcIbDnPU = 0.0f;
#else
    if (g_ilCalB.active)
    {
        IL_CAL_Update(&g_ilCalB, (uint32_t)AD1CH0DATA, (uint32_t)AD1CH1DATA);
    }

    g_pfcIbUpPU = ADC_TO_PU(AD1CH0DATA, g_ilCalB.offsetUp, kIbPU);
    g_pfcIbDnPU = ADC_TO_PU(AD1CH1DATA, g_ilCalB.offsetDn, kIbPU);
    g_pfcVoutPU = ADC_TO_PU(AD3CH0DATA, FB_VOUT_OFFSET, kVoutPU);
#endif

    LLC_SIGNAL_LoadUpdate();

    SMPS_SM_TaskPhaseB();

    X2Cscope_Update();

    IFS4bits.AD1CH1IF = 0U;
}

/*******************************************************************************
* Public Function : _T1Interrupt
* Description     : Timer1 ISR - 100 us slow task
* Purpose         : Temperature sampling, fault evaluation, state machine,
*                   UART1 byte transfer, scheduler tick
* Input           : None (hardware interrupt)
* Return          : None
* Calls           : SMPS_SM_TaskSlow, LLC_LINK_ByteService
* Called by       : Hardware
* Note            : Priority 1, period = 100 us; asynchronous to the PWM.
*                   UART1 is polled here (8-byte FIFOs, ~1.15 bytes per tick).
*                   Measured pin pulse ~15 us (of the 100 us period), wall
*                   clock including preemption by the two 65 kHz ADC ISRs
*                   (~1 of each, ~9.0 us), so the T1 work itself is ~6 us
*******************************************************************************/
void __attribute__ ( ( __interrupt__ ) ) _T1Interrupt(void)
{
    if (!g_userIsrReady) { IFS1bits.T1IF = 0U; return; }

#if APP_PLL_PWM_TEST_ENABLE
    // A disconnected bench has no valid thermistors either.
    g_pfcTempGanPU   = 25.0f / TEMP_BASE;
    g_pfcTempInletPU = 25.0f / TEMP_BASE;
    g_pfcTempLlcAPU  = 25.0f / TEMP_BASE;
    g_pfcTempLlcBPU  = 25.0f / TEMP_BASE;
#else
    g_pfcTempGanPU   = ADC_TO_PU(AD3CH1DATA, FB_TEMP_OFFSET, kTemp3PU);
    g_pfcTempInletPU = ADC_TO_PU(AD4CH0DATA, FB_TEMP_OFFSET, kTemp4PU);
    g_pfcTempLlcAPU  = ADC_TO_PU(AD4CH1DATA, FB_TEMP_OFFSET, kTemp4PU);
    g_pfcTempLlcBPU  = ADC_TO_PU(AD4CH2DATA, FB_TEMP_OFFSET, kTemp4PU);
#endif

    SMPS_SM_TaskSlow();

    LLC_LINK_ByteService();

    g_schedulerTickCount++;
    IFS1bits.T1IF = 0;
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation
/*******************************************************************************
* Private Function : AdcGainGet
* Description     : Return the ADC gain-calibration factor of a shared core
* Purpose         : Convert the 18-bit fractional coefficient to float
* Input           : core - ADC shared core index
* Return          : gain factor (1.0 when not calibrated)
* Calls           : ADCx_SharedCoreGainCoefGet
* Called by       : USER_ISR_AdcScaleInit
* Note            : None
*******************************************************************************/
static float AdcGainGet(ADC_SHARED_CORE_e core)
{
    uint32_t coeff = ADCx_SharedCoreGainCoefGet((uint16_t)core);

    return (coeff == 0U) ? 1.0f : ((float)coeff * ADC_GAIN_FRAC_SCALE);
}

/******************************************************************************/
#undef USER_ISR_C
