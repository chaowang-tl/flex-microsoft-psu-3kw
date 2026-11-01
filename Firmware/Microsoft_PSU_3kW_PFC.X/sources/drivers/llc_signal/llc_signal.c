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
* File Name       : llc_signal.c
* Description     : Duty-coded discrete signals to/from the LLC (RC4 out, RD6 in)
*
*   Both signals: 10 kHz, value = linear over duty 10 %..90 %, invalid
*   outside 5 %..95 % or with no edge for LLC_DUTY_TIMEOUT_MS
*   (docs/PFC_LLC_Comm_Protocol_EN.md, section 2).
*
*   RC4 (PFC -> LLC, bus voltage 0..500 V): SCCP3 dual-edge buffered PWM,
*   100 MHz, CCP3PR = 9999 (10 kHz), RA = 0, RB = high time. RB is buffered,
*   so a new duty takes effect at the next period. 0 % until the first update.
*
*   RD6 (LLC -> PFC, load 0..3.6 kW): MCC + SccpInitExt sync chain, as in
*   the LLC project (sec_pkcmc_llc _CCP1Interrupt):
*     SCCP2 rising-edge capture, ALTSYNC -> resets the SCCP1 time base
*     SCCP1 falling-edge capture -> CCP1BUF.BUFL = high time [100 MHz counts]
*   At 10 kHz a period is 10000 counts, so duty = BUFL / 10000.
*   The phase-B ISR (65 kHz, same context as the voltage loop that uses the
*   result) polls CCP1IF, reads BUFL and clears the flag; no CCP interrupt.
*   SCCP2 rising timestamps measure frequency; only 10 kHz +-1 % qualifies.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/31
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#define LLC_SIGNAL_C

//------------------------------------------------------------------------------
// Include
#include "llc_signal.h"
#include "../../common/llc_duty_def.h"
#include "../../config/board_config.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions
#define LLC_SIG_CLK_HZ          100000000.0f    // Standard peripheral clock (FOSC/2)
#define LLC_SIG_PERIOD_CNT      ((uint16_t)(LLC_SIG_CLK_HZ / LLC_DUTY_FREQ_HZ))  // 10000
#define LLC_SIG_CALL_HZ         BOARD_FAST_LOOP_HZ  // LLC_SIGNAL_LoadUpdate rate (phase-B ISR)
#define LLC_SIG_TIMEOUT_CALLS   (LLC_DUTY_TIMEOUT_MS * (LLC_SIG_CALL_HZ / 1000U))  // 2 ms = 130
#define LLC_SIG_FILT_ALPHA      0.125f          // Per capture: ~8-sample EMA, ~1.3 kHz at 10 kHz
#define LLC_SIG_VALID_CNT       8U              // Consecutive good captures before valid
#define LLC_SIG_DUTY_SPAN       (LLC_DUTY_MAX - LLC_DUTY_MIN)

//------------------------------------------------------------------------------
// Public Variables
volatile float    g_llcLoadPowerW;
volatile bool     g_llcLoadValid;
volatile float    g_llcLoadDuty;
volatile float    g_llcLoadFreqHz;
volatile uint32_t g_llcLoadBadCnt;
volatile float    g_llcVbusDuty;

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes
static void LlcSignalLoadInvalidate(void);
//------------------------------------------------------------------------------
// Private Variables
uint16_t sigNoEdgeCalls;        // LoadUpdate calls since the last capture
uint16_t sigRisePrev;
uint16_t sigPeriodCount;
bool sigRiseSeen;
bool sigPeriodValid;
uint16_t sigGoodCnt;            // Consecutive in-range captures

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : LLC_SIGNAL_Init
* Description     : Start the RC4 PWM and arm the RD6 capture
* Purpose         : Bring both duty-coded LLC signals to a known state
* Input           : None
* Return          : None
* Calls           : None
* Called by       : main (before global interrupts are enabled)
* Note            : Must run after SYSTEM_Initialize (MCC sets the SCCP1/2/3
*                   modes and the RC4 / RD6 PPS) and before the ADC ISRs start.
*                   This module owns SCCP1/2/3 from here on.
*                   RC4 starts at 0 % so the LLC sees "invalid" until the first
*                   LLC_SIGNAL_VbusUpdate. OCAEN is set explicitly so SCCP3
*                   drives the OC3A/RC4 pin whatever the MCC CCP3CON2 value.
*                   RD6: SCCP2 (rising edge) resets the SCCP1 time base through
*                   ALTSYNC, SCCP1 captures the falling edge, so CCP1BUF.BUFL is
*                   the high time; SCCP2 is enabled first (sync source)
*******************************************************************************/
void LLC_SIGNAL_Init(void)
{
    // RC4: SCCP3 dual-edge PWM, 10 kHz, 0 %
    CCP3CON1bits.ON    = 0;
    CCP3PR             = (uint32_t)LLC_SIG_PERIOD_CNT - 1UL;
    CCP3RA             = 0UL;
    CCP3RB             = 0UL;
    CCP3CON2bits.OCAEN = 1;
    CCP3CON1bits.ON    = 1;
    g_llcVbusDuty      = 0.0f;

    // RD6: SCCP2 -> SCCP1 two-edge chain
    CCP1CON1bits.ON      = 0;
    CCP2CON1bits.ON      = 0;
    CCP2CON1bits.SYNC = 0;          // SCCP2 timer free-runs for period measurement
    CCP2PR = 0xFFFFUL;
    CCP2TMR = 0UL;
    CCP2CON1bits.ALTSYNC = 1;       // SCCP2 sync output = its capture event
    CCP1CON1bits.SYNC    = 2;       // SCCP1 time base reset by SCCP2
    CCP2CON1bits.ON      = 1;
    CCP1CON1bits.ON      = 1;

    // Drop captures taken before the loop runs
    for (uint8_t n = 0U; (n < 8U) && CCP1STATbits.ICBNE; n++)   // FIFO is 4 deep
    {
        (void)CCP1BUF;
    }
    for (uint8_t n = 0U; n < 8U && CCP2STATbits.ICBNE; n++) { (void)CCP2BUF; }
    CCP2STATbits.ICOV = 0;
    IFS1bits.CCP2IF = 0;
    sigRiseSeen = false;
    sigPeriodValid = false;
    sigPeriodCount = LLC_SIG_PERIOD_CNT;
    CCP1STATbits.ICOV = 0;
    IFS1bits.CCP1IF   = 0;

    sigNoEdgeCalls  = LLC_SIG_TIMEOUT_CALLS;
    g_llcLoadFreqHz = 0.0f;
    g_llcLoadBadCnt = 0U;
    LlcSignalLoadInvalidate();
}

/*******************************************************************************
* Public Function : LLC_SIGNAL_LoadUpdate
* Description     : Read the RD6 high time and decode the LLC load power
* Purpose         : Provide g_llcLoadPowerW / g_llcLoadValid for feed-forward
* Input           : None
* Return          : None
* Calls           : LlcSignalLoadInvalidate
* Called by       : _AD1CH1Interrupt (phase-B ISR, 65 kHz), every state
* Note            : CCP1IF set -> one new falling-edge capture; BUFL is the
*                   high time in 100 MHz counts (10000 = 100 %). At 65 kHz the
*                   flag is seen about every 6.5 calls; filter and qualification
*                   count captures, not calls. Overflow or a capture backlog invalidates and flushes both FIFOs.
*                   Eight fresh in-range cycles are needed to re-qualify
*******************************************************************************/
void LLC_SIGNAL_LoadUpdate(void)
{
    uint8_t riseCount = 0U;
    uint8_t fallCount = 0U;
    uint16_t rise = 0U;
    uint16_t high = 0U;
    float duty;

    if (CCP1STATbits.ICOV || CCP2STATbits.ICOV)
    {
        for (uint8_t n = 0U; n < 8U && CCP1STATbits.ICBNE; n++) { (void)CCP1BUF; }
        for (uint8_t n = 0U; n < 8U && CCP2STATbits.ICBNE; n++) { (void)CCP2BUF; }
        CCP1STATbits.ICOV = 0;
        CCP2STATbits.ICOV = 0;
        IFS1bits.CCP1IF = 0;
        IFS1bits.CCP2IF = 0;
        sigRiseSeen = false;
        sigPeriodValid = false;
        g_llcLoadFreqHz = 0.0f;
        g_llcLoadBadCnt++;
        LlcSignalLoadInvalidate();
        return;
    }
    while (riseCount < 8U && CCP2STATbits.ICBNE)
    {
        rise = (uint16_t)CCP2BUFbits.BUFL;
        riseCount++;
    }
    while (fallCount < 8U && CCP1STATbits.ICBNE)
    {
        high = (uint16_t)CCP1BUFbits.BUFL;
        fallCount++;
    }
    IFS1bits.CCP1IF = 0;
    IFS1bits.CCP2IF = 0;
    if (riseCount > 1U || fallCount > 1U)
    {
        // Backlogged edges cannot be paired reliably; re-qualify fresh cycles.
        sigRiseSeen = false;
        sigPeriodValid = false;
        g_llcLoadFreqHz = 0.0f;
        g_llcLoadBadCnt++;
        LlcSignalLoadInvalidate();
        return;
    }
    if (riseCount != 0U)
    {
        if (sigRiseSeen)
        {
            sigPeriodCount = (uint16_t)(rise - sigRisePrev); // 16-bit timer wrap
            sigPeriodValid = sigPeriodCount != 0U;
            g_llcLoadFreqHz = sigPeriodValid ? LLC_SIG_CLK_HZ / (float)sigPeriodCount : 0.0f;
            sigPeriodValid = sigPeriodValid
                && g_llcLoadFreqHz >= LLC_DUTY_FREQ_HZ * (1.0f - LLC_DUTY_FREQ_TOL)
                && g_llcLoadFreqHz <= LLC_DUTY_FREQ_HZ * (1.0f + LLC_DUTY_FREQ_TOL);
            if (!sigPeriodValid) { LlcSignalLoadInvalidate(); }
        }
        sigRisePrev = rise;
        sigRiseSeen = true;
    }
    if (fallCount == 0U)
    {
        if (sigNoEdgeCalls < LLC_SIG_TIMEOUT_CALLS) { sigNoEdgeCalls++; }
        else
        {
            sigRiseSeen = false;
            sigPeriodValid = false;
            g_llcLoadFreqHz = 0.0f;
            LlcSignalLoadInvalidate();
        }
        return;
    }
    sigNoEdgeCalls = 0U;
    if (!sigPeriodValid)
    {
        g_llcLoadBadCnt++;
        LlcSignalLoadInvalidate();
        return;
    }
    duty = (float)high / (float)sigPeriodCount;
    if (duty < LLC_DUTY_VALID_LO || duty > LLC_DUTY_VALID_HI)
    {
        g_llcLoadBadCnt++;
        LlcSignalLoadInvalidate();
        return;
    }
    if (sigGoodCnt == 0U) { g_llcLoadDuty = duty; }
    else { g_llcLoadDuty += LLC_SIG_FILT_ALPHA * (duty - g_llcLoadDuty); }
    if (sigGoodCnt < LLC_SIG_VALID_CNT) { sigGoodCnt++; }
    if (sigGoodCnt < LLC_SIG_VALID_CNT) { return; }
    duty = (g_llcLoadDuty - LLC_DUTY_MIN) / LLC_SIG_DUTY_SPAN;
    if (duty < 0.0f) { duty = 0.0f; }
    else if (duty > 1.0f) { duty = 1.0f; }
    g_llcLoadPowerW = duty * LLC_DUTY_LOAD_FS_W;
    g_llcLoadValid = true;
}

/*******************************************************************************
* Public Function : LLC_SIGNAL_VbusUpdate
* Description     : Encode the bus voltage on the RC4 duty
* Purpose         : Tell the LLC the PFC bulk voltage
* Input           : vbusV - bulk voltage [V]
* Return          : None
* Calls           : None
* Called by       : APP_TASKS_Task1ms
* Note            : Clamped to 0..LLC_DUTY_VBUS_FS_V, i.e. duty 10 %..90 %.
*                   CCP3RB is a 32-bit buffered register: one write, picked
*                   up at the next period, so no torn update
*******************************************************************************/
void LLC_SIGNAL_VbusUpdate(float vbusV)
{
    float duty;

    if (vbusV < 0.0f)
    {
        vbusV = 0.0f;
    }
    else if (vbusV > LLC_DUTY_VBUS_FS_V)
    {
        vbusV = LLC_DUTY_VBUS_FS_V;
    }

    duty = LLC_DUTY_MIN + LLC_SIG_DUTY_SPAN * (vbusV * (1.0f / LLC_DUTY_VBUS_FS_V));

    CCP3RB = (uint32_t)(duty * (float)LLC_SIG_PERIOD_CNT + 0.5f);
    g_llcVbusDuty = duty;
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation
/*******************************************************************************
* Private Function : LlcSignalLoadInvalidate
* Description     : Mark the RD6 load signal invalid
* Purpose         : Feed-forward and telemetry fall back to zero load
* Input           : None
* Return          : None
* Calls           : None
* Called by       : LLC_SIGNAL_Init, LLC_SIGNAL_LoadUpdate
* Note            : Re-qualification needs LLC_SIG_VALID_CNT good samples
*******************************************************************************/
static void LlcSignalLoadInvalidate(void)
{
    g_llcLoadValid  = false;
    g_llcLoadPowerW = 0.0f;
    sigGoodCnt      = 0U;
}

/******************************************************************************/
#undef LLC_SIGNAL_C
