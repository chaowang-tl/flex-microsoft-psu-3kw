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
* File Name       : smps_sm.c
* Description     : PFC state machine implementation
*
*   INIT -> STANDBY -> PREPARE -> SOFTSTART -> RUN -> STOPPING -> STANDBY
*   Any state -> FAULT on FAULT_IsActive(); FAULT -> STANDBY once cleared.
*   INIT holds PWM off until the inductor-current CT zero calibration is done.
*   PREPARE waits for PLL lock and bulk pre-charge (Vout >= k * vPk).
*   SOFTSTART ramps Vref from the pre-charged Vout to the target.
*   PWM is forced off on every shutdown (not just loop disable).
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/18
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#define SMPS_SM_C

//------------------------------------------------------------------------------
// Include
#include "smps_sm.h"
#include "smps_common.h"
#include "smps_control.h"
#include "pfc_loop.h"
#include "../fault/fault.h"
#include "../meas/grid_meas.h"
#include "../../config/app_config.h"
#include "../../drivers/pfc_pwm/pfc_pwm.h"
#include "../meas/il_cal.h"
#include "../../drivers/mcc_extended/mcc_ext.h"
#include "../../drivers/pfc_io/pfc_io.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions
#define SMPS_SOFTSTART_VREF_STEP_PU       (90.0f * (BOARD_SLOW_TICK_US * 1e-6f) / VOUT_BASE)   // 90 V/s per T1 tick
#define SMPS_PRECHARGE_RATIO              0.90f       // Vout >= ratio * VAC_BASE/VOUT_BASE * vPk
#define SMPS_PREPARE_STABLE_TICKS         500U        // 50 ms
#define SMPS_RUN_ENTRY_STABLE_TICKS       50U
#define SMPS_STOPPING_TIMEOUT_TICKS       10000U      // 1 s
#define SMPS_STARTUP_TIMEOUT_TICKS (APP_PFC_STARTUP_TIMEOUT_MS * BOARD_SLOW_TICKS_PER_MS)
#define SMPS_RELAY_SETTLE_TICKS (APP_PFC_RELAY_SETTLE_MS * BOARD_SLOW_TICKS_PER_MS)
#define SMPS_PG_WINDOW_PU (APP_PFC_PG_WINDOW_V / VOUT_BASE)
#define SMPS_VAC_TO_VOUT_PU               (VAC_BASE / VOUT_BASE)

//------------------------------------------------------------------------------
// Public Variables
volatile SMPS_SM_T g_sm;

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes
static void SmpsShutdown(void);
static void SmpsRelaySet(bool close);
static bool SmpsPrechargeDone(void);
#if !APP_PLL_PWM_TEST_ENABLE
static bool SmpsCalibrationOk(void);
static bool SmpsCalibrationFailed(void);
#endif
static void SmpsFastTrip(FAULT_FAST_ID_e id);
//------------------------------------------------------------------------------
// Private Variables
volatile bool smpsLoopEnabled;
volatile bool smpsCtrlResetNeeded;
volatile float smpsVrefCmdPU = SMPS_VOUT_REF_PU_DEFAULT;
volatile bool smpsFaultClearRequest;
volatile bool smpsTripLatched;          // Set by SmpsFastTrip, cleared only on FAULT -> STANDBY
volatile uint32_t smpsIsrCntA;          // Phase-A ISR heartbeat
volatile uint32_t smpsIsrCntB;          // Phase-B ISR heartbeat
volatile bool smpsHealthArmed;          // Both ADC ISRs have run at least once: heartbeat is judged from now on
volatile bool smpsHealthTick;           // T1 saw both ISRs alive since the last watchdog clear
uint32_t smpsIsrCntAPrev;
uint32_t smpsIsrCntBPrev;

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : SMPS_SM_Init
* Description     : Initialize PWM safe state, controllers, PLL and faults
* Purpose         : Configure everything, enter INIT with outputs off
* Input           : None
* Return          : None
* Calls           : PFC_PWM_Init, SMPS_FloatControllerInit, PFC_LOOP_Init,
*                   GRID_MEAS_Init, FAULT_Init, SmpsShutdown
* Called by       : main (before TMR1_Start and global interrupt enable)
* Note            : Must be called after SYSTEM_Initialize / MCC_EXT_SystemInitialize.
*                   Peripherals outside the converter (LLC signals and link,
*                   CT calibration) are started by main, not here
*******************************************************************************/
void SMPS_SM_Init(void)
{
    PFC_PWM_Init();
    SMPS_FloatControllerInit();
    PFC_LOOP_Init();
    GRID_MEAS_Init();
    FAULT_Init();

    g_sm.state          = SMPS_STATE_INIT;
    g_sm.enableCommand   = APP_PFC_ENABLE_AT_STARTUP;
    g_sm.stateTimer      = 0U;
    g_sm.runEntryCounter = 0U;
    g_sm.startupTimer = 0UL;
    smpsHealthTick = false;
    smpsHealthArmed = false;
    smpsTripLatched = false;

    g_smpsPfcVrefTargetPU = smpsVrefCmdPU;
    SmpsShutdown();
}

/*******************************************************************************
* Public Function : SMPS_SM_TaskPhaseA
* Description     : Phase-A ISR task
* Purpose         : Always run the PLL; run the phase-A path only when enabled
* Input           : None
* Return          : None
* Calls           : SmpsFastTrip, PFC_LOOP_GridUpdate, PFC_LOOP_PhaseAUpdate,
*                   PFC_LOOP_Reset, GRID_MEAS_SampleUpdate
* Called by       : _AD5CH1Interrupt (65 kHz)
* Note            : Fast OCP runs first, in every state. Controller reset
*                   is done here, in the fast context, once per shutdown request.
*                   RMS sampling runs last so it sees the fresh iA and theta
*******************************************************************************/
void SMPS_SM_TaskPhaseA(void)
{
    if ((fabsf(g_pfcIaUpPU) > FLT_FAST_IL_OCP_THRESHOLD)
        || (fabsf(g_pfcIaDnPU) > FLT_FAST_IL_OCP_THRESHOLD))
    {
        SmpsFastTrip(FLT_FAST_ID_IA_OCP);
    }

    smpsIsrCntA++;
    PFC_LOOP_GridUpdate();

    if (smpsLoopEnabled && !smpsTripLatched)
    {
        PFC_LOOP_PhaseAUpdate();
    }
    else if (smpsCtrlResetNeeded)
    {
        PFC_LOOP_Reset();
        smpsCtrlResetNeeded = false;
    }
    if (!smpsLoopEnabled)
    {
        // Keep the slow OCP source live after shutdown so recovery can occur.
        float current = fmaxf(fabsf(g_pfcIaUpPU), fabsf(g_pfcIaDnPU));
        g_pfcIaFiltPU += PFC_IL_FILT_ALPHA * (current - g_pfcIaFiltPU);
    }

    GRID_MEAS_SampleUpdate();
}

/*******************************************************************************
* Public Function : SMPS_SM_TaskPhaseB
* Description     : Phase-B ISR task
* Purpose         : Run the phase-B path and voltage loop only when enabled
* Input           : None
* Return          : None
* Calls           : SmpsFastTrip, PFC_LOOP_PhaseBUpdate
* Called by       : _AD1CH1Interrupt (65 kHz)
* Note            : Fast OCP and bulk OVP run first, in every state (vout is
*                   sampled with phase B)
*******************************************************************************/
void SMPS_SM_TaskPhaseB(void)
{
    if ((fabsf(g_pfcIbUpPU) > FLT_FAST_IL_OCP_THRESHOLD)
        || (fabsf(g_pfcIbDnPU) > FLT_FAST_IL_OCP_THRESHOLD))
    {
        SmpsFastTrip(FLT_FAST_ID_IB_OCP);
    }

    if (g_pfcVoutPU > FLT_FAST_VOUT_OVP_THRESHOLD)
    {
        SmpsFastTrip(FLT_FAST_ID_VOUT_OVP);
    }

    smpsIsrCntB++;

    if (smpsLoopEnabled && !smpsTripLatched)
    {
        PFC_LOOP_PhaseBUpdate();
    }
    else
    {
        float current = fmaxf(fabsf(g_pfcIbUpPU), fabsf(g_pfcIbDnPU));
        g_pfcIbFiltPU += PFC_IL_FILT_ALPHA * (current - g_pfcIbFiltPU);
    }
}

/*******************************************************************************
* Public Function : SMPS_SM_TaskSlow
* Description     : Slow-task PFC state machine
* Purpose         : Fault evaluation, startup sequencing, soft-start, stop
* Input           : None
* Return          : None
* Calls           : FAULT_*, SMPS_FloatSoftStartRampToTarget, SmpsShutdown,
*                   SmpsPrechargeDone, SmpsRelaySet, GRID_MEAS_TaskSlow,
*                   IL_CAL_Done
* Called by       : _T1Interrupt (100 us)
* Note            : Also publishes PLL amplitude/frequency for fault detection
*                   and the grid voltage / input current RMS. ISR heartbeat:
*                   at 65 kHz each ADC ISR runs ~6.5 times per tick. Judging
*                   starts once both counts have moved (smpsHealthArmed, so
*                   the first ticks before the ADC ISRs start are not a
*                   fault); after that, if either count has not moved the
*                   converter is shut down, FLT_ISR_HEALTH is latched and the
*                   watchdog is no longer served (SMPS_SM_HealthConsume).
*                   INIT is left only once the heartbeat has been armed.
*                   Calibration status: SmpsCalibrationOk / SmpsCalibrationFailed
*******************************************************************************/
void SMPS_SM_TaskSlow(void)
{
    uint32_t cntA = smpsIsrCntA;
    uint32_t cntB = smpsIsrCntB;
    bool adcIsrAlive = (cntA != smpsIsrCntAPrev) && (cntB != smpsIsrCntBPrev);

    if (adcIsrAlive)
    {
        smpsHealthArmed = true;
        if (g_fault.state[FLT_ISR_HEALTH].active) { FAULT_Clear(FLT_ISR_HEALTH); }
    }
    else if (smpsHealthArmed)
    {
        // Heartbeat lost after the ISRs had been running: latch and stop
        smpsTripLatched = true;
        FAULT_Set(FLT_ISR_HEALTH);
        SmpsShutdown();
        g_sm.state = SMPS_STATE_FAULT;
        g_sm.stateTimer = 0U;
    }
    // Watchdog token: only while both ISRs run and no health fault is latched
    smpsHealthTick = adcIsrAlive && !g_fault.state[FLT_ISR_HEALTH].latched;
    smpsIsrCntAPrev = cntA;
    smpsIsrCntBPrev = cntB;

    if (g_sm.state == SMPS_STATE_FAULT && !g_sm.enableCommand
        && g_fault.state[FLT_STARTUP_TIMEOUT].active)
    {
        FAULT_Clear(FLT_STARTUP_TIMEOUT);
    }

    g_pfcVacPeakPU = SOGI_PLL_GetAmplitude(&g_pll);
    g_pfcFreqPU    = SOGI_PLL_GetFrequency(&g_pll) * (1.0f / FREQ_BASE);
    GRID_MEAS_TaskSlow();

#if APP_PLL_PWM_TEST_ENABLE
    // All control/fault inputs are synthetic; disconnected analog calibration
    // is not a startup condition. ISR heartbeat and trip latches remain active.
    bool calibrationOk = true;
#else
    // Both calibrations run once at power-up; a rejected result latches
    // FLT_CALIBRATION again after every clear, so the board needs a reset
    bool calibrationOk = SmpsCalibrationOk();
    if (SmpsCalibrationFailed())
    {
        FAULT_Set(FLT_CALIBRATION);
    }
    else if (calibrationOk && g_fault.state[FLT_CALIBRATION].active)
    {
        FAULT_Clear(FLT_CALIBRATION);
    }
#endif

    if (smpsFaultClearRequest)
    {
        smpsFaultClearRequest = false;
        if ((g_sm.state == SMPS_STATE_FAULT || g_sm.state == SMPS_STATE_STANDBY)
            && !smpsLoopEnabled)
        {
            (void)FAULT_ClearAll();
        }
    }

    FAULT_Evaluate(g_sm.state == SMPS_STATE_RUN);

    if (FAULT_IsActive() && (g_sm.state != SMPS_STATE_FAULT))
    {
        SmpsShutdown();
        g_sm.state = SMPS_STATE_FAULT;
        g_sm.stateTimer = 0U;
    }

    switch (g_sm.state)
    {
        //----------------------------------------------------------------------
        case SMPS_STATE_INIT:
        {
            SmpsShutdown();
            if (calibrationOk && smpsHealthArmed)
            {
                g_sm.state = SMPS_STATE_STANDBY;
            }
            break;
        }

        //----------------------------------------------------------------------
        case SMPS_STATE_STANDBY:
        {
            if (g_sm.enableCommand && calibrationOk)
            {
                g_sm.state = SMPS_STATE_PREPARE;
                g_sm.stateTimer = 0U;
                g_sm.startupTimer = 0UL;
            }
            break;
        }

        //----------------------------------------------------------------------
        case SMPS_STATE_PREPARE:
        {
            if (!g_sm.enableCommand)
            {
                SmpsShutdown();
                g_sm.state = SMPS_STATE_STANDBY;
                g_sm.stateTimer = 0U;
                break;
            }

            if (++g_sm.startupTimer >= SMPS_STARTUP_TIMEOUT_TICKS)
            {
                FAULT_Set(FLT_STARTUP_TIMEOUT);
                SmpsShutdown();
                g_sm.state = SMPS_STATE_FAULT;
                break;
            }
            if (SOGI_PLL_IsLocked(&g_pll) && SmpsPrechargeDone() && calibrationOk)
            {
                g_sm.stateTimer++;
                if (g_sm.stateTimer == SMPS_PREPARE_STABLE_TICKS)
                {
                    SmpsRelaySet(true);
                }
            }
            else
            {
                g_sm.stateTimer = 0U;
                SmpsRelaySet(false);
            }

            if ((g_sm.stateTimer >= SMPS_PREPARE_STABLE_TICKS + SMPS_RELAY_SETTLE_TICKS)
                && !smpsTripLatched && calibrationOk)
            {
                SmpsRelaySet(true);
                g_smpsPfcVrefPU       = g_pfcVoutPU;
                g_smpsPfcVrefTargetPU = smpsVrefCmdPU;
                g_sm.state           = SMPS_STATE_SOFTSTART;
                g_sm.startupTimer = 0UL;
                g_sm.stateTimer      = 0U;
                g_sm.runEntryCounter = 0U;
                smpsLoopEnabled      = true;
            }
            else
            {
                // Vout is still rising: keep the controllers and the Vout notch
                // preset to the present state, so SOFTSTART starts from the
                // real bus voltage instead of the value seen at the end of INIT
                smpsCtrlResetNeeded = true;
            }
            break;
        }

        //----------------------------------------------------------------------
        case SMPS_STATE_SOFTSTART:
        {
            bool refReady;

            if (!g_sm.enableCommand || !SOGI_PLL_IsLocked(&g_pll))
            {
                SmpsShutdown();
                g_sm.state = SMPS_STATE_STOPPING;
                g_sm.stateTimer = 0U;
                break;
            }

            refReady = SMPS_FloatSoftStartRampToTarget(&g_smpsPfcVrefPU,
                                                        g_smpsPfcVrefTargetPU,
                                                        SMPS_SOFTSTART_VREF_STEP_PU);
            if (++g_sm.startupTimer >= SMPS_STARTUP_TIMEOUT_TICKS)
            {
                FAULT_Set(FLT_STARTUP_TIMEOUT);
                SmpsShutdown();
                g_sm.state = SMPS_STATE_FAULT;
                break;
            }
            if (refReady && isfinite(g_pfcVoutPU)
                && fabsf(g_pfcVoutPU - g_smpsPfcVrefTargetPU) <= SMPS_PG_WINDOW_PU)
            {
                g_sm.runEntryCounter++;
                if (g_sm.runEntryCounter >= SMPS_RUN_ENTRY_STABLE_TICKS)
                {
                    g_sm.state = SMPS_STATE_RUN;
                    g_sm.stateTimer = 0U;
                }
            }
            else
            {
                g_sm.runEntryCounter = 0U;
            }
            break;
        }

        //----------------------------------------------------------------------
        case SMPS_STATE_RUN:
        {
            if (!g_sm.enableCommand || !SOGI_PLL_IsLocked(&g_pll))
            {
                SmpsShutdown();
                g_sm.state = SMPS_STATE_STOPPING;
                g_sm.stateTimer = 0U;
                break;
            }

            if (fabsf(g_smpsPfcVrefPU - g_smpsPfcVrefTargetPU) > SMPS_SOFTSTART_VREF_STEP_PU)
            {
                g_sm.state = SMPS_STATE_SOFTSTART;
                g_sm.runEntryCounter = 0U;
                g_sm.startupTimer = 0UL;
            }
            break;
        }

        //----------------------------------------------------------------------
        case SMPS_STATE_STOPPING:
        {
            SmpsShutdown();
            g_sm.stateTimer++;

            if (g_sm.stateTimer >= SMPS_STOPPING_TIMEOUT_TICKS)
            {
                g_sm.state = SMPS_STATE_STANDBY;
                g_sm.stateTimer = 0U;
            }
            break;
        }

        //----------------------------------------------------------------------
        case SMPS_STATE_FAULT:
        {
            SmpsShutdown();

            // Wait for the CT zero calibration too: an early fault (e.g. VAC_UVP
            // before the PLL settles) must not skip INIT's calibration wait
            if (!FAULT_IsActive() && calibrationOk)
            {
                smpsTripLatched = false;
                g_sm.state = SMPS_STATE_STANDBY;
                g_sm.stateTimer = 0U;
            }
            break;
        }

        //----------------------------------------------------------------------
        default:
        {
            SmpsShutdown();
            g_sm.state = SMPS_STATE_FAULT;
            break;
        }
    }
}

/*******************************************************************************
* Public Function : SMPS_SM_Enable
* Description     : Set the converter enable command
* Purpose         : Start or stop the PFC
* Input           : enable - true to request start, false to stop
* Return          : None
* Calls           : None
* Called by       : LinkDispatch, LinkPfcStopForBoot (llc_link.c); X2Cscope writes g_sm.enableCommand
* Note            : Takes effect on the next SMPS_SM_TaskSlow call
*******************************************************************************/
void SMPS_SM_Enable(bool enable)
{
    g_sm.enableCommand = enable;
}

/*******************************************************************************
* Public Function : SMPS_SM_SetVoutRef
* Description     : Set the output voltage reference command
* Purpose         : Change the bulk voltage setpoint
* Input           : refPU - Vout reference in PU of VOUT_BASE
* Return          : None
* Calls           : None
* Called by       : LinkDispatch (llc_link.c, SET_VOUT_REF)
* Note            : Clamped to [SMPS_VOUT_REF_MIN_PU, SMPS_VOUT_REF_MAX_PU];
*                   RUN re-enters SOFTSTART to ramp to the new target
*******************************************************************************/
void SMPS_SM_SetVoutRef(float refPU)
{
    UPDNLMT(refPU, SMPS_VOUT_REF_MAX_PU, SMPS_VOUT_REF_MIN_PU);

    smpsVrefCmdPU       = refPU;
    g_smpsPfcVrefTargetPU = refPU;
}

/*******************************************************************************
* Public Function : SMPS_SM_RequestFaultClear
* Description     : Request a one-shot clear of all non-locked-out faults
* Purpose         : Reset latched faults
* Input           : None
* Return          : None
* Calls           : None
* Called by       : LinkDispatch (llc_link.c, CLEAR_FAULT); X2Cscope writes smpsFaultClearRequest
* Note            : Executed in SMPS_SM_TaskSlow so g_fault is only touched
*                   from T1 context
*******************************************************************************/
void SMPS_SM_RequestFaultClear(void)
{
    smpsFaultClearRequest = true;
}

/*******************************************************************************
* Public Function : SMPS_SM_HealthConsume
* Description     : Take the "both ADC ISRs and T1 alive" token
* Purpose         : Serve the watchdog only while every context is running
* Input           : None
* Return          : true once per T1 tick in which both ADC ISRs ran
* Calls           : None
* Called by       : APP_TASKS_Always (main loop), before WATCHDOG_TimerClear
* Note            : T1 sets the token, the main loop clears it, so a stalled
*                   ADC ISR, T1 or main loop all end in a watchdog reset.
*                   Not atomic on purpose: single-byte accesses are atomic,
*                   and a token lost to a T1 race is set again by the next
*                   tick (100 us, far below the ~32 ms watchdog period), so
*                   the 65 kHz ISRs are never masked here
*******************************************************************************/
bool SMPS_SM_HealthConsume(void)
{
    bool healthy = smpsHealthTick;
    smpsHealthTick = false;
    return healthy;
}

/*******************************************************************************
* Public Function : SMPS_SM_IsPowerGood
* Description     : Qualify PG from measured voltage and converter interlocks
* Purpose         : Qualify PG from measured voltage and converter interlocks
* Input           : None
* Return          : true only while the converter is ready
* Calls           : FAULT_IsActive, SOGI_PLL_IsLocked
* Called by       : LinkFillPfcStatus
* Note            : User application code; preserves MCC ownership
*******************************************************************************/
bool SMPS_SM_IsPowerGood(void)
{
    return g_sm.state == SMPS_STATE_RUN && g_sm.enableCommand && smpsLoopEnabled
        && !smpsTripLatched && !FAULT_IsActive()
#if !APP_PLL_PWM_TEST_ENABLE
        && SmpsCalibrationOk()
#endif
        && SOGI_PLL_IsLocked(&g_pll) && isfinite(g_pfcVoutPU)
        && fabsf(g_pfcVoutPU - g_smpsPfcVrefTargetPU) <= SMPS_PG_WINDOW_PU;
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation
/*******************************************************************************
* Private Function : SmpsShutdown
* Description     : Force PWM off, disable the loop, open the relay, request
*                   controller reset
* Purpose         : Bring the converter to a safe idle state
* Input           : None
* Return          : None
* Calls           : PFC_PWM_AllOutputsOff, SmpsRelaySet
* Called by       : SMPS_SM_Init, SMPS_SM_TaskSlow
* Note            : Asynchronous output override is requested; the phase-A ISR then resets
*                   the controllers once (smpsCtrlResetNeeded). The relay
*                   opens so the next start pre-charges through the limiter
*******************************************************************************/
static void SmpsShutdown(void)
{
    smpsLoopEnabled = false;
    PFC_PWM_AllOutputsOff();
    SmpsRelaySet(false);
    smpsCtrlResetNeeded = true;
    g_smpsPfcVrefPU = 0.0f;
}

/*******************************************************************************
* Private Function : SmpsRelaySet
* Description     : Control the inrush bypass relay
* Purpose         : Bypass the inrush limiter after pre-charge
* Input           : close - true to close the relay
* Return          : None
* Calls           : PFC_IO_RelaySet
* Called by       : SMPS_SM_TaskSlow, SmpsShutdown
* Note            : RB2 high = relay closed; PFC_IO_Init opens it at start-up
*******************************************************************************/
static void SmpsRelaySet(bool close)
{
    PFC_IO_RelaySet(close);
}

/*******************************************************************************
* Private Function : SmpsPrechargeDone
* Description     : Check that the bulk capacitor reached the grid peak
* Purpose         : Avoid starting the boost before the passive pre-charge
* Input           : None
* Return          : true when Vout >= ratio * vPk (both in volts)
* Calls           : SOGI_PLL_GetAmplitude
* Called by       : SMPS_SM_TaskSlow
* Note            : vPk is in PU of VAC_BASE, Vout in PU of VOUT_BASE
*******************************************************************************/
static bool SmpsPrechargeDone(void)
{
    return g_pfcVoutPU >= (SMPS_PRECHARGE_RATIO * SMPS_VAC_TO_VOUT_PU
                         * SOGI_PLL_GetAmplitude(&g_pll));
}

#if !APP_PLL_PWM_TEST_ENABLE
/*******************************************************************************
* Private Function : SmpsCalibrationOk
* Description     : Check that both power-up calibrations finished and were accepted
* Purpose         : One definition of "calibration good" for startup and PG
* Input           : None
* Return          : true when the ADC gain calibration of every core succeeded and
*                   the CT zero calibration of both phases is done and accepted
* Calls           : IL_CAL_Done
* Called by       : SMPS_SM_TaskSlow, SMPS_SM_IsPowerGood
* Note            : Not compiled in bench-test mode (synthetic inputs)
*******************************************************************************/
static bool SmpsCalibrationOk(void)
{
    return IL_CAL_Done() && g_ilCalA.ok && g_ilCalB.ok
        && (g_adcCalibrationFailedMask == 0UL);
}

/*******************************************************************************
* Private Function : SmpsCalibrationFailed
* Description     : Check whether a calibration finished and was rejected
* Purpose         : Tell "failed" from "not finished yet" for FLT_CALIBRATION
* Input           : None
* Return          : true when an ADC gain core failed, or the CT calibration is
*                   done but a phase result was rejected
* Calls           : IL_CAL_Done
* Called by       : SMPS_SM_TaskSlow
* Note            : A calibration that is still running is not a failure
*******************************************************************************/
static bool SmpsCalibrationFailed(void)
{
    return (g_adcCalibrationFailedMask != 0UL)
        || (IL_CAL_Done() && (!g_ilCalA.ok || !g_ilCalB.ok));
}
#endif

/*******************************************************************************
* Private Function : SmpsFastTrip
* Description     : Instantaneous protection action from a 65 kHz ISR
* Purpose         : Stop switching within one PWM cycle, then report to T1
* Input           : id - fast fault index
* Return          : None
* Calls           : PFC_PWM_AllOutputsOff
* Called by       : SMPS_SM_TaskPhaseA, SMPS_SM_TaskPhaseB
* Note            : Only sets flags; g_fault is latched later by FAULT_Evaluate
*                   in T1 context, which also moves the state machine to FAULT.
*                   smpsTripLatched is sticky: T1 may set smpsLoopEnabled again
*                   in the same tick (PREPARE -> SOFTSTART after FAULT_Evaluate),
*                   but the ISRs keep the loop off until FAULT -> STANDBY
*******************************************************************************/
static void SmpsFastTrip(FAULT_FAST_ID_e id)
{
    smpsTripLatched = true;
    smpsLoopEnabled = false;
    PFC_PWM_AllOutputsOff();
    smpsCtrlResetNeeded = true;
    g_faultFastTrip[id] = true;
}

/******************************************************************************/
#undef SMPS_SM_C
