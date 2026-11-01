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
* File Name       : fault.c
* Description     : PFC fault management with auto-recovery and fast trip
*
*   Two paths share one g_fault object, which is only modified in T1 context:
*   - Slow path: FAULT_DETECT_T objects (threshold + hysteresis + debounce)
*     evaluated every 100 us by FAULT_Evaluate.
*   - Fast path: the 65 kHz ISRs compare instantaneous values, switch PWM
*     off immediately and set g_faultFastTrip[]. FAULT_Evaluate converts
*     those flags into latched faults.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/22
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#define FAULT_C

//------------------------------------------------------------------------------
// Include
#include "fault.h"
#include "fault_common.h"
#include "../control/smps_control.h"
#include "../../config/board_config.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions
#define FLT_DEFAULT_MAX_RETRIES     3
#define FLT_DETECT_COUNT            12          // FLT_VAC_OVP .. FLT_OTP_LLCB

_Static_assert(FLT_COUNT == FLT_MAX_COUNT, "FAULTLOG_FLAGS_e must have FLT_MAX_COUNT entries");
_Static_assert(FLT_DETECT_COUNT == (uint16_t)FLT_FAST_IA_OCP, "slow detectors are FLT_VAC_OVP..FLT_OTP_LLCB");
_Static_assert((FLT_FAST_IL_OCP_THRESHOLD * IL_BASE) < (0.9f * BOARD_FB_IL_RANGE_A),
               "fast OCP threshold must sit inside the CT range with margin");

//------------------------------------------------------------------------------
// Public Variables
volatile FAULT_OBJ_T g_fault;
volatile bool g_faultFastTrip[FLT_FAST_ID_COUNT];

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes
static void FaultConfigSet(FAULTLOG_FLAGS_e id, FAULT_RECOVERY_MODE_e mode, uint16_t maxRetries);
static void FaultFastTripLatch(void);
//------------------------------------------------------------------------------
// Private Variables
FAULT_DETECT_T fltDetect[FLT_DETECT_COUNT];

const FAULTLOG_FLAGS_e fltFastIdMap[FLT_FAST_ID_COUNT] =
{
    FLT_FAST_IA_OCP,
    FLT_FAST_IB_OCP,
    FLT_FAST_VOUT_OVP
};

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : FAULT_Init
* Description     : Initialize fault configuration and clear all states
* Purpose         : Set recovery policy, reset state, bind PU sources
* Input           : None
* Return          : None
* Calls           : FaultConfigSet, FAULT_DETECT_Init
* Called by       : SMPS_SM_Init
* Note            : Reserved faults default to RECOVERY_NONE (latched)
*******************************************************************************/
void FAULT_Init(void)
{
    uint16_t i;

    for (i = 0; i < FLT_MAX_COUNT; i++)
    {
        FaultConfigSet((FAULTLOG_FLAGS_e)i, FLT_RECOVERY_NONE, 0U);
        g_fault.state[i].active     = false;
        g_fault.state[i].latched    = false;
        g_fault.state[i].lockedOut  = false;
        g_fault.state[i].retryCount = 0U;
    }

    g_fault.activeFlags  = 0U;
    g_fault.latchedFlags = 0U;
    g_fault.lockedOutFlags = 0U;

    for (i = 0; i < FLT_FAST_ID_COUNT; i++)
    {
        g_faultFastTrip[i] = false;
    }

    FaultConfigSet(FLT_VAC_OVP,       FLT_RECOVERY_LIMITED,   FLT_DEFAULT_MAX_RETRIES);
    FaultConfigSet(FLT_VAC_UVP,       FLT_RECOVERY_UNLIMITED, 0U);
    FaultConfigSet(FLT_VOUT_OVP,      FLT_RECOVERY_NONE,      0U);
    FaultConfigSet(FLT_VOUT_UVP,      FLT_RECOVERY_LIMITED,   FLT_DEFAULT_MAX_RETRIES);
    FaultConfigSet(FLT_IA_OCP,        FLT_RECOVERY_LIMITED,   FLT_DEFAULT_MAX_RETRIES);
    FaultConfigSet(FLT_IB_OCP,        FLT_RECOVERY_LIMITED,   FLT_DEFAULT_MAX_RETRIES);
    FaultConfigSet(FLT_FREQ_HI,       FLT_RECOVERY_UNLIMITED, 0U);
    FaultConfigSet(FLT_FREQ_LO,       FLT_RECOVERY_UNLIMITED, 0U);
    FaultConfigSet(FLT_OTP_GAN,       FLT_RECOVERY_UNLIMITED, 0U);
    FaultConfigSet(FLT_OTP_INLET,     FLT_RECOVERY_UNLIMITED, 0U);
    FaultConfigSet(FLT_OTP_LLCA,      FLT_RECOVERY_UNLIMITED, 0U);
    FaultConfigSet(FLT_OTP_LLCB,      FLT_RECOVERY_UNLIMITED, 0U);
    FaultConfigSet(FLT_ISR_HEALTH,   FLT_RECOVERY_NONE, 0U);
    FaultConfigSet(FLT_CALIBRATION,  FLT_RECOVERY_NONE, 0U);
    FaultConfigSet(FLT_STARTUP_TIMEOUT, FLT_RECOVERY_NONE, 0U);
    FaultConfigSet(FLT_FAST_IA_OCP,   FLT_RECOVERY_NONE,      0U);
    FaultConfigSet(FLT_FAST_IB_OCP,   FLT_RECOVERY_NONE,      0U);
    FaultConfigSet(FLT_FAST_VOUT_OVP, FLT_RECOVERY_NONE,      0U);

    FAULT_DETECT_Init(&fltDetect[FLT_VAC_OVP], &g_pfcVacPeakPU,
                      FLT_VAC_OVP_THRESHOLD, FLT_VAC_OVP_HYSTERESIS,
                      FLT_CMP_GREATER_THAN, FLT_VAC_OVP,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_VAC_UVP], &g_pfcVacPeakPU,
                      FLT_VAC_UVP_THRESHOLD, FLT_VAC_UVP_HYSTERESIS,
                      FLT_CMP_LESS_THAN, FLT_VAC_UVP,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_VOUT_OVP], &g_pfcVoutPU,
                      FLT_VOUT_OVP_THRESHOLD, FLT_VOUT_OVP_HYSTERESIS,
                      FLT_CMP_GREATER_THAN, FLT_VOUT_OVP,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_VOUT_UVP], &g_pfcVoutPU,
                      FLT_VOUT_UVP_THRESHOLD, FLT_VOUT_UVP_HYSTERESIS,
                      FLT_CMP_LESS_THAN, FLT_VOUT_UVP,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_IA_OCP], &g_pfcIaFiltPU,
                      FLT_IL_OCP_THRESHOLD, FLT_IL_OCP_HYSTERESIS,
                      FLT_CMP_GREATER_THAN, FLT_IA_OCP,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_IB_OCP], &g_pfcIbFiltPU,
                      FLT_IL_OCP_THRESHOLD, FLT_IL_OCP_HYSTERESIS,
                      FLT_CMP_GREATER_THAN, FLT_IB_OCP,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_FREQ_HI], &g_pfcFreqPU,
                      FLT_FREQ_HI_THRESHOLD, FLT_FREQ_HI_HYSTERESIS,
                      FLT_CMP_GREATER_THAN, FLT_FREQ_HI,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_FREQ_LO], &g_pfcFreqPU,
                      FLT_FREQ_LO_THRESHOLD, FLT_FREQ_LO_HYSTERESIS,
                      FLT_CMP_LESS_THAN, FLT_FREQ_LO,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_OTP_GAN], &g_pfcTempGanPU,
                      FLT_OTP_THRESHOLD, FLT_OTP_HYSTERESIS,
                      FLT_CMP_GREATER_THAN, FLT_OTP_GAN,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_OTP_INLET], &g_pfcTempInletPU,
                      FLT_OTP_THRESHOLD, FLT_OTP_HYSTERESIS,
                      FLT_CMP_GREATER_THAN, FLT_OTP_INLET,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_OTP_LLCA], &g_pfcTempLlcAPU,
                      FLT_OTP_THRESHOLD, FLT_OTP_HYSTERESIS,
                      FLT_CMP_GREATER_THAN, FLT_OTP_LLCA,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);

    FAULT_DETECT_Init(&fltDetect[FLT_OTP_LLCB], &g_pfcTempLlcBPU,
                      FLT_OTP_THRESHOLD, FLT_OTP_HYSTERESIS,
                      FLT_CMP_GREATER_THAN, FLT_OTP_LLCB,
                      FLT_DEBOUNCE_SET_DEFAULT, FLT_DEBOUNCE_CLR_DEFAULT);
}

/*******************************************************************************
* Public Function : FAULT_Set
* Description     : Register an active fault condition
* Purpose         : Set fault active and latched, update bitmasks
* Input           : id - fault index from FAULTLOG_FLAGS_e
* Return          : None
* Calls           : None
* Called by       : FAULT_DETECT_Evaluate, FaultFastTripLatch
* Note            : T1 context only
*******************************************************************************/
void FAULT_Set(FAULTLOG_FLAGS_e id)
{
    if ((uint16_t)id >= FLT_MAX_COUNT)
    {
        return;
    }

    g_fault.state[id].active  = true;
    g_fault.state[id].latched = true;

    g_fault.activeFlags  |= ((uint32_t)1 << (uint16_t)id);
    g_fault.latchedFlags |= ((uint32_t)1 << (uint16_t)id);
}

/*******************************************************************************
* Public Function : FAULT_Clear
* Description     : Signal that a fault condition has disappeared
* Purpose         : Handle auto-recovery based on per-fault config
* Input           : id - fault index from FAULTLOG_FLAGS_e
* Return          : None
* Calls           : None
* Called by       : FAULT_DETECT_Evaluate
* Note            : Locked-out or RECOVERY_NONE faults stay latched
*******************************************************************************/
void FAULT_Clear(FAULTLOG_FLAGS_e id)
{
    volatile FAULT_STATE_T  *pState;
    volatile FAULT_CONFIG_T *pCfg;

    if ((uint16_t)id >= FLT_MAX_COUNT)
    {
        return;
    }

    pState = &g_fault.state[id];
    pCfg   = &g_fault.config[id];

    pState->active = false;
    g_fault.activeFlags &= ~((uint32_t)1 << (uint16_t)id);

    if (pState->lockedOut)
    {
        return;
    }

    switch (pCfg->recoveryMode)
    {
        case FLT_RECOVERY_UNLIMITED:
            if (pState->retryCount < UINT16_MAX) { pState->retryCount++; }
            pState->latched = false;
            g_fault.latchedFlags &= ~((uint32_t)1 << (uint16_t)id);
            break;

        case FLT_RECOVERY_LIMITED:
            if (pState->retryCount < UINT16_MAX) { pState->retryCount++; }
            if (pState->retryCount > pCfg->maxRetries)
            {
                pState->lockedOut = true;
                g_fault.lockedOutFlags |= (1UL << (uint16_t)id);
            }
            else
            {
                pState->latched = false;
                g_fault.latchedFlags &= ~((uint32_t)1 << (uint16_t)id);
            }
            break;

        case FLT_RECOVERY_NONE:
        default:
            break;
    }
}

/*******************************************************************************
* Public Function : FAULT_ClearAll
* Description     : Manual clear request for all non-locked-out faults
* Purpose         : Reset latched state and retry counters where permitted
* Input           : None
* Return          : true if no unrecovered, locked-out or pending fault remains
* Calls           : None
* Called by       : SMPS_SM_TaskSlow (on SMPS_SM_RequestFaultClear)
* Note            : Only recovered conditions can clear; detector history is kept.
*                   Locked-out faults remain latched; return false if any remain
*******************************************************************************/
bool FAULT_ClearAll(void)
{
    uint16_t i;
    bool allClear = true;

    for (i = 0U; i < FLT_MAX_COUNT; i++)
    {
        if (!g_fault.state[i].latched) { continue; }
        bool recovered = !g_fault.state[i].active;
        if (i < FLT_DETECT_COUNT)
        {
            float value = *fltDetect[i].pSource;
            recovered = recovered && isfinite(value)
                && ((fltDetect[i].cmpMode == FLT_CMP_GREATER_THAN)
                    ? value < fltDetect[i].threshold - fltDetect[i].hysteresis
                    : value > fltDetect[i].threshold + fltDetect[i].hysteresis);
        }
        if (i == FLT_FAST_IA_OCP)
        {
            recovered = isfinite(g_pfcIaUpPU) && isfinite(g_pfcIaDnPU)
                && fabsf(g_pfcIaUpPU) < (FLT_FAST_IL_OCP_THRESHOLD - FLT_IL_OCP_HYSTERESIS)
                && fabsf(g_pfcIaDnPU) < (FLT_FAST_IL_OCP_THRESHOLD - FLT_IL_OCP_HYSTERESIS);
        }
        else if (i == FLT_FAST_IB_OCP)
        {
            recovered = isfinite(g_pfcIbUpPU) && isfinite(g_pfcIbDnPU)
                && fabsf(g_pfcIbUpPU) < (FLT_FAST_IL_OCP_THRESHOLD - FLT_IL_OCP_HYSTERESIS)
                && fabsf(g_pfcIbDnPU) < (FLT_FAST_IL_OCP_THRESHOLD - FLT_IL_OCP_HYSTERESIS);
        }
        else if (i == FLT_FAST_VOUT_OVP)
        {
            recovered = isfinite(g_pfcVoutPU)
                && g_pfcVoutPU < (FLT_VOUT_OVP_THRESHOLD - FLT_VOUT_OVP_HYSTERESIS);
        }
        if (g_fault.state[i].lockedOut || !recovered)
        {
            allClear = false;
            continue;
        }
        g_fault.state[i].active = false;
        g_fault.state[i].latched = false;
        g_fault.state[i].retryCount = 0U;
        g_fault.activeFlags &= ~(1UL << i);
        g_fault.latchedFlags &= ~(1UL << i);
    }
    return allClear && !FAULT_IsActive();
}

/*******************************************************************************
* Public Function : FAULT_Evaluate
* Description     : Run fast-trip latching and all slow fault detectors
* Purpose         : Call every 100 us to detect/clear faults
* Input           : monitorVoutUvp - true: evaluate VOUT_UVP (RUN state only)
* Return          : None
* Calls           : FaultFastTripLatch, FAULT_DETECT_Evaluate
* Called by       : SMPS_SM_TaskSlow
* Note            : None
*******************************************************************************/
void FAULT_Evaluate(bool monitorVoutUvp)
{
    uint16_t i;
    FaultFastTripLatch();
    for (i = 0U; i < FLT_DETECT_COUNT; i++)
    {
        if (!monitorVoutUvp && i == FLT_VOUT_UVP && !fltDetect[i].detected)
        {
            // Inhibit new detections only; keep an existing trip's recovery.
            FAULT_DETECT_Reset(&fltDetect[i]);
            continue;
        }
        FAULT_DETECT_Evaluate(&fltDetect[i]);
    }
}

/*******************************************************************************
* Public Function : FAULT_IsActive
* Description     : Check whether any fault is latched or a fast trip is pending
* Purpose         : Quick check for the state machine to enter FAULT
* Input           : None
* Return          : true if any fault is latched or pending
* Calls           : None
* Called by       : SMPS_SM_TaskSlow
* Note            : None
*******************************************************************************/
bool FAULT_IsActive(void)
{
    return FAULT_GetLatchedFlags() != 0UL;
}

/*******************************************************************************
* Public Function : FAULT_GetLatchedFlags
* Description     : Return the latched fault bitmask
* Purpose         : Diagnostics / communication
* Input           : None
* Return          : 32-bit bitmask of latched faults
* Calls           : None
* Called by       : LinkFillPfcStatus (llc_link.c)
* Note            : None
*******************************************************************************/
uint32_t FAULT_GetLatchedFlags(void)
{
    uint32_t flags = g_fault.latchedFlags;
    for (uint16_t i = 0U; i < FLT_FAST_ID_COUNT; i++)
    {
        if (g_faultFastTrip[i]) { flags |= (1UL << (uint16_t)fltFastIdMap[i]); }
    }
    return flags;
}

/*******************************************************************************
* Public Function : FAULT_GetLockedOutFlags
* Description     : Return the locked-out fault bitmask
* Purpose         : Identify faults that exhausted retries and need a reset
* Input           : None
* Return          : 32-bit bitmask of locked-out faults
* Calls           : None
* Called by       : LinkFillPfcStatus (llc_link.c)
* Note            : None
*******************************************************************************/
uint32_t FAULT_GetLockedOutFlags(void)
{
    return g_fault.lockedOutFlags;
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation

/*******************************************************************************
* Private Function : FaultConfigSet
* Description     : Set the recovery policy of one fault
* Purpose         : Keep FAULT_Init readable
* Input           : id         - fault index
*                   mode       - recovery mode
*                   maxRetries - retries for FLT_RECOVERY_LIMITED
* Return          : None
* Calls           : None
* Called by       : FAULT_Init
* Note            : None
*******************************************************************************/
static void FaultConfigSet(FAULTLOG_FLAGS_e id, FAULT_RECOVERY_MODE_e mode, uint16_t maxRetries)
{
    g_fault.config[id].recoveryMode = mode;
    g_fault.config[id].maxRetries   = maxRetries;
}

/*******************************************************************************
* Private Function : FaultFastTripLatch
* Description     : Convert ISR fast-trip flags into latched faults
* Purpose         : Keep g_fault single-writer (T1 context)
* Input           : None
* Return          : None
* Calls           : FAULT_Set
* Called by       : FAULT_Evaluate
* Note            : PWM was already switched off by the ISR that set the flag
*******************************************************************************/
static void FaultFastTripLatch(void)
{
    uint16_t i;

    for (i = 0; i < FLT_FAST_ID_COUNT; i++)
    {
        if (g_faultFastTrip[i])
        {
            FAULT_Set(fltFastIdMap[i]);
            g_faultFastTrip[i] = false;
        }
    }
}

/******************************************************************************/
#undef FAULT_C
