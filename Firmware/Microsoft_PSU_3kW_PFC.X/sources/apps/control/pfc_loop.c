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
* File Name       : pfc_loop.c
* Description     : Interleaved totem-pole PFC fast control path
*
*   Phase-A ISR (65 kHz, PWM3 trigger):
*     SOGI-PLL -> zero-crossing / polarity (slow leg, A SWAP)
*              -> current reference -> phase-A current loop -> PG3DC
*   Phase-B ISR (65 kHz, PWM4 trigger, 180 deg later):
*     follow polarity (B SWAP) -> phase-B current loop -> PG4DC
*     every PFC_VLOOP_DIVIDER calls: voltage loop (8.125 kHz)
*
*   Current reference per phase: iref = 0.5 * demand / vPk * |cos(theta)|
*   demand is constrained PI + qualified load feed-forward.
*   (PLL input lines up with cos(theta); 1/vPk and 1/vout are computed at
*   the voltage-loop rate, so the control path has no division at 65 kHz.
*   The SOGI-PLL itself still runs sqrtf, sinf/cosf and one division per
*   phase-A sample, about 1.3 us)
*   Duty feed-forward:            dff  = 1 - |vac| / vout
*   Both ISRs share priority 5, so they never preempt each other; every
*   shared variable is a single 32-bit word.
*
*   Polarity (measured voltage is the master, PLL cross-checks):
*     ENTER / EXIT = PFC_ZC_ENTER_PU / PFC_ZC_EXIT_PU * vPk (fixed angle,
*     2.82 / 4.23 deg); nominal 16 V / 24 V while the PLL is unlocked
*     enter BLANK on ANY of: |vac| < ENTER, sign(vac) != polarity,
*                            PLL locked and |cos| < PLL_WIN
*     leave BLANK on ALL of: PFC_ZC_DEBOUNCE_CNT samples |vac| > EXIT with
*                            one sign, and (PLL unlocked or sign(cos) agrees
*                            with |cos| >= PLL_WIN)
*   Zero crossing: current loops are frozen in BLANK (no update, no reset)
*   and resume from the previous half-cycle. Leaving BLANK, the duty ramps
*   from 0 with the slow leg and the synchronous rectifier pin still forced
*   low (the slow-leg body diode carries the current); once the ramp reaches
*   the closed-loop duty (frozen output) the loop takes over with a seeded
*   error history (bumpless), the slow leg is turned on and, in the same call,
*   the rectifier pin is released.
*
*   Voltage loop: the PI feedback goes through a notch centred at 2x the PLL
*   grid frequency, so the bulk 2x line ripple does not modulate the current
*   reference (3rd harmonic). Feed-forward and protection use raw Vout.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/28
* Modified by     : Chao Wang
* Modified on     : 2026/11/10
*******************************************************************************/
#define PFC_LOOP_C

//------------------------------------------------------------------------------
// Include
#include "pfc_loop.h"
#include "smps_control.h"
#include "../../drivers/llc_signal/llc_signal.h"
#include "../../config/app_config.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions
// Zero-crossing blanking window on |vac| [PU of VAC_BASE], given at vPk = 1 PU
// (325 Vpk) and scaled by the PLL amplitude at run time (fixed angle)
// TODO: tune on hardware
#define PFC_ZC_ENTER_PU         (16.0f / VAC_BASE)  // 16 V at 325 Vpk (2.82 deg): enter BLANK
#define PFC_ZC_EXIT_PU          (24.0f / VAC_BASE)  // 24 V at 325 Vpk (4.23 deg): leave BLANK
#define PFC_ZC_DEBOUNCE_CNT     3U          // Samples above EXIT, same sign (~46 us)
#define PFC_ZC_PLL_WIN          0.035f      // |cos(theta)| window, ~+-2 deg; 0 disables

// Duty soft-start after each zero crossing; 0 disables (closed loop at once)
#define PFC_ZC_SS_CYCLES        16U         // ~246 us at 65 kHz
#define PFC_ZC_SS_STEP          (PFC_DUTY_MAX / (float)((PFC_ZC_SS_CYCLES > 0U) ? PFC_ZC_SS_CYCLES : 1U))

// Synchronous-rectifier window soft start: runs from the boost hand-over,
// PFC_SYNC_SS_CYCLES calls long, on-time from PFC_SYNC_SS_START_NS up to
// PFC_SYNC_DUTY_MAX of the switching period, then the gate opens fully
#define PFC_SYNC_SS_START_NS    100.0f
#define PFC_SYNC_SS_CYCLES      PFC_ZC_SS_CYCLES
#define PFC_SYNC_DUTY_MAX       0.95f

#define PFC_DUTY_MAX            0.95f
#define PFC_DUTY_MIN            0.0f
#define PFC_VPK_MIN_PU          (80.0f / VAC_BASE)  // Floor for 1/vPk, 80 Vpk
#define PFC_VOUT_MIN_PU         (90.0f / VOUT_BASE) // Floor for 1/vout, 90 V
#define PFC_VAC_TO_VOUT_PU      (VAC_BASE / VOUT_BASE)

// Voltage-loop load feed-forward from the LLC load (RD6), APP_PFC_LOAD_FF_ENABLE
// ff [PU of PFC_P_NOM_W input power] = gain * P_llc_out / PFC_P_NOM_W
// gain = 1 / eta_LLC only (PFC losses left to the PI), so ff slightly
// under-estimates and the PI stays positive in steady state. TODO: calibrate
#define PFC_LOAD_FF_GAIN        (1.0f / 0.97f)

#if BOARD_FAST_LOOP_HZ != 65000U
#error "SOGI_PLL_TS_SEC (sogi_pll.h) assumes 65 kHz: update it with BOARD_FAST_LOOP_HZ"
#endif
#define PFC_DEMAND_MAX_PU       1.2f        // Total power demand limit (3.6 kW)

// Voltage-loop feedback notch at 2x line frequency, tracked from the PLL
#define PFC_NOTCH_TS            ((float)PFC_VLOOP_DIVIDER / PFC_FAST_LOOP_HZ)
#define PFC_NOTCH_BW_HZ         20.0f       // -3 dB width; wider tolerates more frequency error
#define PFC_NOTCH_FN_DEFAULT_HZ 100.0f      // Used until the PLL locks
#define PFC_NOTCH_FN_MIN_HZ     88.0f       // 2 x 44 Hz
#define PFC_NOTCH_FN_MAX_HZ     134.0f      // 2 x 67 Hz
#define PFC_NOTCH_R             (1.0f - 3.14159265f * PFC_NOTCH_BW_HZ * PFC_NOTCH_TS)
#define PFC_NOTCH_TWO_PI        6.28318530f

//------------------------------------------------------------------------------
// Public Variables
volatile PFC_PWM_POLARITY_e g_pfcPolarity;
volatile float g_pfcDutyAPU;
volatile float g_pfcDutyBPU;
volatile uint32_t g_pfcZcRejectCnt;
volatile uint32_t g_pfcZcMismatchCnt;
volatile float g_pfcLoadFfPU;
volatile float g_pfcPowerDemandPU;

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions
// Per-phase duty soft-start after a zero crossing
typedef struct
{
    float dutyLim;                  // Ramped duty limit
    bool  active;                   // Ramp running, current loop frozen
    uint16_t cycles;                 // Minimum rectifier hold-off
    bool  syncActive;               // Sync-window ramp running (after hand-over)
    uint16_t syncCycles;            // Sync-window ramp step
} PFC_SS_T;

// Second-order notch, zeros on the unit circle at fn, poles at radius r
typedef struct
{
    float b0;                       // = b2
    float b1;
    float a1;
    float a2;                       // -r^2, fixed by the bandwidth
    float x1;                       // Input history
    float x2;
    float y1;                       // Output history
    float y2;
} PFC_NOTCH_T;

//------------------------------------------------------------------------------
// Private Function Prototypes
static void  PfcVoltageLoop(void);
static float PfcLoadFeedForward(float ffPrev);
static void  PfcNotchCoefUpdate(PFC_NOTCH_T *n, float fnHz);
static float PfcNotchRun(PFC_NOTCH_T *n, float in);
static void  PfcNotchPreset(PFC_NOTCH_T *n, float value);
static float PfcInductorCurrentSelect(PFC_PWM_POLARITY_e polarity, float iUp, float iDn);
static PFC_PWM_POLARITY_e PfcPolarityUpdate(float vac);
static bool  PfcZcMustEnterBlank(float vac, PFC_PWM_POLARITY_e polarity);
static PFC_PWM_POLARITY_e PfcZcLeaveBlank(float vac);
static bool  PfcPllAtZeroCrossing(void);
static PFC_PWM_POLARITY_e PfcVacSign(float vac);
static PFC_PWM_POLARITY_e PfcPllSign(void);
static void  PfcSoftStartBegin(PFC_SS_T *ss);
static float PfcDutyCompute(float vacAbs, SMPS_FLOAT_NPNZ_T const * const ctrl, PFC_SS_T *ss);
static uint32_t PfcSyncWindowCount(uint16_t step);
//------------------------------------------------------------------------------
// Private Variables
volatile float pfcIpkPerPhasePU;    // 0.5 * demand / vPk, voltage-loop rate
volatile float pfcInvVoutPU;        // VAC_BASE/VOUT_BASE / vout, voltage-loop rate
volatile float pfcZcEnterPU;        // PFC_ZC_ENTER_PU * vPk, voltage-loop rate
volatile float pfcZcExitPU;         // PFC_ZC_EXIT_PU * vPk, voltage-loop rate
PFC_PWM_POLARITY_e pfcPolarityB;    // Polarity currently applied to phase B
PFC_PWM_POLARITY_e pfcZcCandidate;  // Polarity being debounced in BLANK
uint16_t pfcZcCnt;                  // Consecutive samples of pfcZcCandidate
PFC_SS_T pfcSsA;
PFC_SS_T pfcSsB;
uint16_t pfcVloopCnt;
PFC_NOTCH_T pfcVoutNotch;

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : PFC_LOOP_Init
* Description     : Initialize the grid PLL and the fast-path state
* Purpose         : Prepare PLL and polarity logic before ADC ISRs start
* Input           : None
* Return          : None
* Calls           : SOGI_PLL_Init, PfcNotchCoefUpdate, PFC_LOOP_Reset
* Called by       : SMPS_SM_Init
* Note            : Must run before interrupts are enabled
*******************************************************************************/
void PFC_LOOP_Init(void)
{
    SOGI_PLL_Init(&g_pll, SOGI_PLL_TS_SEC, SOGI_PLL_F0_HZ,
                  SOGI_PLL_K, SOGI_PLL_FN_HZ, SOGI_PLL_ZETA);
    pfcVoutNotch.a2 = -(PFC_NOTCH_R * PFC_NOTCH_R);
    PfcNotchCoefUpdate(&pfcVoutNotch, PFC_NOTCH_FN_DEFAULT_HZ);
    PFC_LOOP_Reset();
}

/*******************************************************************************
* Public Function : PFC_LOOP_Reset
* Description     : Reset all PFC controllers and the polarity state
* Purpose         : Clean restart of the fast path (PLL is not reset)
* Input           : None
* Return          : None
* Calls           : SMPS_FloatController2P2ZReset, PFC_PWM_AllOutputsOff,
*                   PfcNotchPreset
* Called by       : PFC_LOOP_Init, SMPS_SM_TaskPhaseA (loop disabled)
* Note            : Call from the phase-A ISR or before ISRs start; phase B
*                   shares priority so it cannot run concurrently.
*                   Only place the current loops are cleared (not at zero
*                   crossings). Diagnostic counters are kept. The notch is
*                   preset to the present Vout so a restart has no step
*******************************************************************************/
void PFC_LOOP_Reset(void)
{
    PFC_PWM_AllOutputsOff();
    PFC_PWM_PhaseASyncGateOpen();
    PFC_PWM_PhaseBSyncGateOpen();

    SMPS_FloatController2P2ZReset(&g_vLoop2p2z);
    SMPS_FloatController2P2ZReset(&g_iaLoop2p2z);
    SMPS_FloatController2P2ZReset(&g_ibLoop2p2z);

    g_pfcPolarity    = PFC_PWM_POL_BLANK;
    pfcPolarityB     = PFC_PWM_POL_BLANK;
    g_smpsPfcIrefPU    = 0.0f;
    pfcIpkPerPhasePU = 0.0f;
    g_pfcLoadFfPU      = 0.0f;
    g_pfcPowerDemandPU = 0.0f;
    pfcInvVoutPU     = 0.0f;
    pfcZcEnterPU     = PFC_ZC_ENTER_PU;
    pfcZcExitPU      = PFC_ZC_EXIT_PU;
    g_pfcDutyAPU       = 0.0f;
    g_pfcDutyBPU       = 0.0f;
    g_pfcIaPU          = 0.0f;        // Not updated while the loop is off
    g_pfcIbPU          = 0.0f;
    pfcZcCandidate   = PFC_PWM_POL_BLANK;
    pfcZcCnt         = 0U;
    pfcSsA.dutyLim   = 0.0f;
    pfcSsA.active    = false;
    pfcSsA.syncActive = false;
    pfcSsA.syncCycles = 0U;
    pfcSsB.dutyLim   = 0.0f;
    pfcSsB.active    = false;
    pfcSsB.syncActive = false;
    pfcSsB.syncCycles = 0U;
    pfcVloopCnt      = 0U;
    PfcNotchPreset(&pfcVoutNotch, g_pfcVoutPU);
}

/*******************************************************************************
* Public Function : PFC_LOOP_GridUpdate
* Description     : Run the SOGI-PLL on the latest grid sample
* Purpose         : Keep phase/amplitude tracking alive in every state
* Input           : None
* Return          : None
* Calls           : SOGI_PLL_Update
* Called by       : SMPS_SM_TaskPhaseA (65 kHz)
* Note            : ~1.3 us. With APP_PLL_PWM_TEST_ENABLE g_pfcVacPU is an
*                   internal 50 Hz sine set in the phase-A ADC ISR
*******************************************************************************/
void PFC_LOOP_GridUpdate(void)
{
    SOGI_PLL_Update(&g_pll, g_pfcVacPU);
}

/*******************************************************************************
* Public Function : PFC_LOOP_PhaseAUpdate
* Description     : Phase-A fast path: polarity, reference and current loop
* Purpose         : Own the zero-crossing state and drive PG3 and PG5
* Input           : None
* Return          : None
* Calls           : PFC_PWM_*, PfcPolarityUpdate, PfcSoftStartBegin,
*                   PfcDutyCompute, PfcInductorCurrentSelect
* Called by       : SMPS_SM_TaskPhaseA (65 kHz, loop enabled)
* Note            : Leaving BLANK: set SWAP, start the duty ramp, then release
*                   only the boost-switch pin; the slow leg stays off. When the
*                   ramp hands over to the loop, the slow leg is turned on and
*                   the synchronous rectifier pin is released in the same call
*                   (phase B hands over on its own, the slow leg follows phase
*                   A). The current loop is frozen (not reset) across the zero
*                   crossing
*******************************************************************************/
void PFC_LOOP_PhaseAUpdate(void)
{
    float vacAbs = fabsf(g_pfcVacPU);
    PFC_PWM_POLARITY_e polarityPrev = g_pfcPolarity;
    PFC_PWM_POLARITY_e polarity = PfcPolarityUpdate(g_pfcVacPU);
    bool  rampWasActive;
    float duty;

    if (polarity != polarityPrev)
    {
        if (polarity == PFC_PWM_POL_BLANK)
        {
            g_pfcPolarity = PFC_PWM_POL_BLANK;
            PFC_PWM_AllOutputsOff();
            PFC_PWM_PhaseASyncGateOpen();
            pfcSsA.syncActive = false;
            PFC_PWM_DutyBSet(0.0f);
            g_pfcDutyBPU = 0.0f;
            PFC_PWM_DutyASet(0.0f);
            g_pfcDutyAPU = 0.0f;
        }
        else
        {
            // Slow leg stays off (left forced low by BLANK) until the boost
            // ramp hands over; its body diode carries the current meanwhile
            PFC_PWM_PhaseASwapSet(polarity);
            PfcSoftStartBegin(&pfcSsA);
            PFC_PWM_DutyASet(0.0f);
            PFC_PWM_PhaseABoostEnable(polarity);
            // Sync pin blocked until the boost ramp hands over; boost side passed
            pfcSsA.syncActive = false;
            PFC_PWM_PhaseASyncWindowSet(PfcSyncWindowCount(0U));
            PFC_PWM_PhaseASyncGatePending(polarity);
            g_pfcPolarity = polarity;
        }
    }

    g_pfcIaPU = PfcInductorCurrentSelect(polarity, g_pfcIaUpPU, g_pfcIaDnPU);
    g_pfcIaFiltPU += PFC_IL_FILT_ALPHA * (g_pfcIaPU - g_pfcIaFiltPU);

    if (polarity == PFC_PWM_POL_BLANK)
    {
        g_smpsPfcIrefPU = 0.0f;
        return;
    }

    // Shared per-phase current reference (phase B reuses it half a cycle later)
    g_smpsPfcIrefPU = pfcIpkPerPhasePU * fabsf(SOGI_PLL_GetCos(&g_pll));

    rampWasActive = pfcSsA.active;
    duty = PfcDutyCompute(vacAbs, &g_iaLoop2p2z, &pfcSsA);
    PFC_PWM_DutyASet(duty);
    g_pfcDutyAPU = duty;

    if (rampWasActive && !pfcSsA.active)
    {
        // Hand-over: slow leg on first, then rectifier pin released and sync
        // window starts at step 0
        PFC_PWM_SlowLegSet(polarity);
        PFC_PWM_PhaseASyncEnable(polarity);
        pfcSsA.syncCycles = 0U;
        pfcSsA.syncActive = true;
        PFC_PWM_PhaseASyncWindowSet(PfcSyncWindowCount(0U));
        PFC_PWM_PhaseASyncGateWindow(polarity);
    }
    else if (pfcSsA.syncActive)
    {
        if (++pfcSsA.syncCycles >= PFC_SYNC_SS_CYCLES)
        {
            pfcSsA.syncActive = false;          // Ramp done: sync fully released
            PFC_PWM_PhaseASyncGateOpen();
        }
        else
        {
            PFC_PWM_PhaseASyncWindowSet(PfcSyncWindowCount(pfcSsA.syncCycles));
        }
    }
}

/*******************************************************************************
* Public Function : PFC_LOOP_PhaseBUpdate
* Description     : Phase-B fast path and decimated voltage loop
* Purpose         : Follow phase-A polarity, run phase-B current loop on PG4
* Input           : None
* Return          : None
* Calls           : PFC_PWM_*, PfcVoltageLoop, PfcSoftStartBegin,
*                   PfcDutyCompute, PfcInductorCurrentSelect
* Called by       : SMPS_SM_TaskPhaseB (65 kHz, loop enabled)
* Note            : Phase B only touches PG4; polarity is decided by phase A.
*                   Phase-B soft-start runs independently of phase A; its
*                   synchronous rectifier pin is released at its own hand-over
*******************************************************************************/
void PFC_LOOP_PhaseBUpdate(void)
{
    PFC_PWM_POLARITY_e polarity = g_pfcPolarity;
    bool  rampWasActive;
    float duty;

    if (++pfcVloopCnt >= PFC_VLOOP_DIVIDER)
    {
        pfcVloopCnt = 0U;
        PfcVoltageLoop();
    }

    if (polarity != pfcPolarityB)
    {
        PFC_PWM_PhaseBDisable();
        PFC_PWM_DutyBSet(0.0f);
        g_pfcDutyBPU = 0.0f;

        if (polarity == PFC_PWM_POL_BLANK)
        {
            PFC_PWM_PhaseBSyncGateOpen();
            pfcSsB.syncActive = false;
        }
        else
        {
            PFC_PWM_PhaseBSwapSet(polarity);
            PfcSoftStartBegin(&pfcSsB);
            PFC_PWM_PhaseBBoostEnable(polarity);
            pfcSsB.syncActive = false;
            PFC_PWM_PhaseBSyncWindowSet(PfcSyncWindowCount(0U));
            PFC_PWM_PhaseBSyncGatePending(polarity);
        }
        pfcPolarityB = polarity;
    }

    g_pfcIbPU = PfcInductorCurrentSelect(polarity, g_pfcIbUpPU, g_pfcIbDnPU);
    g_pfcIbFiltPU += PFC_IL_FILT_ALPHA * (g_pfcIbPU - g_pfcIbFiltPU);

    if (polarity == PFC_PWM_POL_BLANK)
    {
        return;
    }

    rampWasActive = pfcSsB.active;
    duty = PfcDutyCompute(fabsf(g_pfcVacPU), &g_ibLoop2p2z, &pfcSsB);
    PFC_PWM_DutyBSet(duty);
    g_pfcDutyBPU = duty;

    if (rampWasActive && !pfcSsB.active)
    {
        // Hand-over: rectifier pin released and sync window starts at step 0
        PFC_PWM_PhaseBSyncEnable(polarity);
        pfcSsB.syncCycles = 0U;
        pfcSsB.syncActive = true;
        PFC_PWM_PhaseBSyncWindowSet(PfcSyncWindowCount(0U));
        PFC_PWM_PhaseBSyncGateWindow(polarity);
    }
    else if (pfcSsB.syncActive)
    {
        if (++pfcSsB.syncCycles >= PFC_SYNC_SS_CYCLES)
        {
            pfcSsB.syncActive = false;          // Ramp done: sync fully released
            PFC_PWM_PhaseBSyncGateOpen();
        }
        else
        {
            PFC_PWM_PhaseBSyncWindowSet(PfcSyncWindowCount(pfcSsB.syncCycles));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation
/*******************************************************************************
* Private Function : PfcVoltageLoop
* Description     : Output voltage loop and slow-rate reciprocals
* Purpose         : Compute power demand and the per-phase current amplitude
* Input           : None
* Return          : None
* Calls           : SOGI_PLL_IsLocked, SOGI_PLL_GetFrequency, PfcNotchCoefUpdate,
*                   PfcNotchRun, SMPS_FloatController2P2ZUpdate,
*                   SOGI_PLL_GetAmplitude, PfcLoadFeedForward
* Called by       : PFC_LOOP_PhaseBUpdate (every PFC_VLOOP_DIVIDER calls)
* Note            : Runs at 8.125 kHz; holds the two divisions of the fast path.
*                   The PI sees the notched Vout (2x line ripple removed); the
*                   duty feed-forward keeps the unfiltered Vout. Notch
*                   coefficients freeze while the PLL is unlocked.
*                   Power demand = PI + feed-forward, clamped to 0..1.2 PU.
*                   Applied PI contribution tracks external saturation.
*                   Also scales the zero-crossing thresholds by vPk (v_est,
*                   20 ms LPF, floored at PFC_VPK_MIN_PU); unlocked: nominal
*******************************************************************************/
static void PfcVoltageLoop(void)
{
    float vPk   = SOGI_PLL_GetAmplitude(&g_pll);
    float vout  = g_pfcVoutPU;
    bool  pllLocked = SOGI_PLL_IsLocked(&g_pll);
    float vPkZc;
    float demand;

    if (pllLocked)
    {
        PfcNotchCoefUpdate(&pfcVoutNotch, 2.0f * SOGI_PLL_GetFrequency(&g_pll));
    }
    g_pfcVoutNotchPU = PfcNotchRun(&pfcVoutNotch, vout);

    SMPS_FloatController2P2ZUpdate(&g_vLoop2p2z);

    g_pfcLoadFfPU = PfcLoadFeedForward(g_pfcLoadFfPU);
    demand = g_smpsPfcVctrlPU + g_pfcLoadFfPU;
    UPDNLMT(demand, PFC_DEMAND_MAX_PU, 0.0f);
    if (vout > g_smpsPfcVrefTargetPU + (APP_PFC_PG_WINDOW_V / VOUT_BASE))
    {
        demand = 0.0f;
    }
    SMPS_FloatController2P2ZTrackOutput(&g_vLoop2p2z, demand - g_pfcLoadFfPU);
    g_pfcPowerDemandPU = demand;

    DOWNLMT(vPk, PFC_VPK_MIN_PU);
    DOWNLMT(vout, PFC_VOUT_MIN_PU);

    pfcIpkPerPhasePU = 0.5f * demand / vPk;             // two phases share IL_BASE
    pfcInvVoutPU     = PFC_VAC_TO_VOUT_PU / vout;

    // Zero-crossing thresholds at a fixed angle; nominal values until locked
    vPkZc        = pllLocked ? vPk : 1.0f;
    pfcZcEnterPU = PFC_ZC_ENTER_PU * vPkZc;
    pfcZcExitPU  = PFC_ZC_EXIT_PU  * vPkZc;
}

/*******************************************************************************
* Private Function : PfcLoadFeedForward
* Description     : Voltage-loop feed-forward from the LLC load signal (RD6)
* Purpose         : Answer LLC load steps before the 20 Hz voltage PI does
* Input           : ffPrev - previous feed-forward [PU]
* Return          : new feed-forward [PU of PFC_P_NOM_W]
* Calls           : None
* Called by       : PfcVoltageLoop
* Note            : While RD6 is valid the feed-forward follows it directly.
*                   Invalid RD6 drops the feed-forward to zero immediately.
*                   Signed PI output and external saturation tracking allow
*                   cancellation of residual feed-forward on load removal. APP_PFC_LOAD_FF_ENABLE = 0
*                   forces 0
*******************************************************************************/
static float PfcLoadFeedForward(float ffPrev)
{
#if APP_PFC_LOAD_FF_ENABLE
    float ff;
    (void)ffPrev;

    if (g_llcLoadValid)
    {
        ff = PFC_LOAD_FF_GAIN * g_llcLoadPowerW * (1.0f / PFC_P_NOM_W);
    }
    else
    {
        ff = 0.0f;    // Do not retain power demand after an invalid load signal
    }

    UPDNLMT(ff, PFC_DEMAND_MAX_PU, 0.0f);
    return ff;
#else
    (void)ffPrev;
    return 0.0f;
#endif
}

/*******************************************************************************
* Private Function : PfcNotchCoefUpdate
* Description     : Retune the notch to a new centre frequency
* Purpose         : Follow the 2x line ripple as the grid frequency changes
* Input           : n    - notch state
*                   fnHz - centre frequency [Hz], clamped to 88..134 Hz
* Return          : None
* Calls           : cosf
* Called by       : PFC_LOOP_Init, PfcVoltageLoop
* Note            : Zeros at exp(+-j*w0) give exact rejection at fn; K sets the
*                   DC gain to 1. a2 = -r^2 depends only on the bandwidth and is
*                   set once in PFC_LOOP_Init. Voltage-loop rate only (cosf)
*******************************************************************************/
static void PfcNotchCoefUpdate(PFC_NOTCH_T *n, float fnHz)
{
    float c;
    float k;

    UPDNLMT(fnHz, PFC_NOTCH_FN_MAX_HZ, PFC_NOTCH_FN_MIN_HZ);

    c = cosf(PFC_NOTCH_TWO_PI * fnHz * PFC_NOTCH_TS);
    k = (1.0f - 2.0f * PFC_NOTCH_R * c + PFC_NOTCH_R * PFC_NOTCH_R) / (2.0f - 2.0f * c);

    n->b0 = k;
    n->b1 = -2.0f * k * c;
    n->a1 = 2.0f * PFC_NOTCH_R * c;
}

/*******************************************************************************
* Private Function : PfcNotchRun
* Description     : Filter one sample through the notch
* Purpose         : Remove the 2x line ripple from the voltage-loop feedback
* Input           : n  - notch state
*                   in - input sample
* Return          : Filtered sample
* Calls           : None
* Called by       : PfcVoltageLoop
* Note            : y = b0*(x + x2) + b1*x1 + a1*y1 + a2*y2 (b2 = b0)
*******************************************************************************/
static float PfcNotchRun(PFC_NOTCH_T *n, float in)
{
    float out = n->b0 * (in + n->x2) + n->b1 * n->x1
              + n->a1 * n->y1 + n->a2 * n->y2;

    n->x2 = n->x1;
    n->x1 = in;
    n->y2 = n->y1;
    n->y1 = out;

    return out;
}

/*******************************************************************************
* Private Function : PfcNotchPreset
* Description     : Load the notch history with a steady value
* Purpose         : Start the filter in steady state (no step transient)
* Input           : n     - notch state
*                   value - steady input/output value
* Return          : None
* Calls           : None
* Called by       : PFC_LOOP_Reset
* Note            : DC gain is 1, so a constant input maps to the same output
*******************************************************************************/
static void PfcNotchPreset(PFC_NOTCH_T *n, float value)
{
    n->x1 = value;
    n->x2 = value;
    n->y1 = value;
    n->y2 = value;
    g_pfcVoutNotchPU = value;
}

/*******************************************************************************
* Private Function : PfcInductorCurrentSelect
* Description     : Pick the switch-current sensor that sees the inductor current
* Purpose         : Reconstruct |iL| from high-/low-side switch current sensors
* Input           : polarity - grid polarity
*                   iUp      - high-side switch current [PU]
*                   iDn      - low-side switch current [PU]
* Return          : |iL| [PU], 0 in BLANK
* Calls           : fabsf
* Called by       : PFC_LOOP_PhaseAUpdate, PFC_LOOP_PhaseBUpdate
* Note            : TODO verify against schematic. Assumes the active (boost)
*                   switch is low-side for vac > 0 and high-side for vac < 0,
*                   sampled at the middle of its on-time
*******************************************************************************/
static float PfcInductorCurrentSelect(PFC_PWM_POLARITY_e polarity, float iUp, float iDn)
{
    if (polarity == PFC_PWM_POL_POS)
    {
        return fabsf(iDn);
    }

    if (polarity == PFC_PWM_POL_NEG)
    {
        return fabsf(iUp);
    }

    return 0.0f;
}

/*******************************************************************************
* Private Function : PfcPolarityUpdate
* Description     : Zero-crossing / polarity decision for the slow leg
* Purpose         : Measured voltage decides, PLL cross-checks
* Input           : vac - grid voltage [PU], signed
* Return          : new polarity (POS/NEG never change directly to the
*                   other sign, always through BLANK)
* Calls           : PfcZcLeaveBlank, PfcZcMustEnterBlank
* Called by       : PFC_LOOP_PhaseAUpdate
* Note            : Entering BLANK is single-sample (fail-safe); leaving it
*                   needs PFC_ZC_DEBOUNCE_CNT samples and PLL agreement.
*                   With the PLL unlocked the decision falls back to voltage
*                   only. Does not write g_pfcPolarity
*******************************************************************************/
static PFC_PWM_POLARITY_e PfcPolarityUpdate(float vac)
{
    PFC_PWM_POLARITY_e polarity = g_pfcPolarity;

    if (polarity == PFC_PWM_POL_BLANK)
    {
        return PfcZcLeaveBlank(vac);
    }

    if (PfcZcMustEnterBlank(vac, polarity))
    {
        pfcZcCandidate = PFC_PWM_POL_BLANK;
        pfcZcCnt       = 0U;
        return PFC_PWM_POL_BLANK;
    }

    return polarity;
}

/*******************************************************************************
* Private Function : PfcZcMustEnterBlank
* Description     : Check whether POS/NEG has to drop into BLANK
* Purpose         : OR of the three enter conditions
* Input           : vac      - grid voltage [PU], signed
*                   polarity - current polarity, POS or NEG
* Return          : true if BLANK must be entered
* Calls           : PfcVacSign, PfcPllAtZeroCrossing
* Called by       : PfcPolarityUpdate
* Note            : A sign flip outside the low band (phase jump, bad sample)
*                   is counted in g_pfcZcMismatchCnt
*******************************************************************************/
static bool PfcZcMustEnterBlank(float vac, PFC_PWM_POLARITY_e polarity)
{
    bool lowVoltage = (fabsf(vac) < pfcZcEnterPU);
    bool signFlip   = (PfcVacSign(vac) != polarity);
    bool pllAtZc    = PfcPllAtZeroCrossing();

    if (signFlip && !lowVoltage)
    {
        g_pfcZcMismatchCnt++;
    }

    return lowVoltage || signFlip || pllAtZc;
}

/*******************************************************************************
* Private Function : PfcZcLeaveBlank
* Description     : Decide whether BLANK can be left, and to which sign
* Purpose         : Three gates in order: amplitude, debounce, PLL
* Input           : vac - grid voltage [PU], signed
* Return          : BLANK, or the debounced polarity when all gates pass
* Calls           : PfcVacSign, PfcPllAtZeroCrossing, PfcPllSign,
*                   SOGI_PLL_IsLocked
* Called by       : PfcPolarityUpdate
* Note            : Updates pfcZcCandidate / pfcZcCnt; a PLL sign mismatch
*                   is counted in g_pfcZcRejectCnt. Gate 3 is skipped while
*                   the PLL is unlocked
*******************************************************************************/
static PFC_PWM_POLARITY_e PfcZcLeaveBlank(float vac)
{
    PFC_PWM_POLARITY_e vacSign = PfcVacSign(vac);

    // Gate 1: amplitude above EXIT
    if (fabsf(vac) <= pfcZcExitPU)
    {
        pfcZcCandidate = PFC_PWM_POL_BLANK;
        pfcZcCnt       = 0U;
        return PFC_PWM_POL_BLANK;
    }

    // Gate 2: same sign for PFC_ZC_DEBOUNCE_CNT consecutive samples
    if (vacSign != pfcZcCandidate)
    {
        pfcZcCandidate = vacSign;
        pfcZcCnt       = 0U;
    }

    if (pfcZcCnt < PFC_ZC_DEBOUNCE_CNT)
    {
        pfcZcCnt++;
    }

    if (pfcZcCnt < PFC_ZC_DEBOUNCE_CNT)
    {
        return PFC_PWM_POL_BLANK;
    }

    // Gate 3: PLL past the crossing and agreeing in sign (locked only)
    if (SOGI_PLL_IsLocked(&g_pll))
    {
        if (PfcPllAtZeroCrossing())
        {
            return PFC_PWM_POL_BLANK;
        }

        if (PfcPllSign() != vacSign)
        {
            g_pfcZcRejectCnt++;
            return PFC_PWM_POL_BLANK;
        }
    }

    return vacSign;
}

/*******************************************************************************
* Private Function : PfcPllAtZeroCrossing
* Description     : PLL locked and |cos(theta)| inside PFC_ZC_PLL_WIN
* Purpose         : PLL view of "at the zero crossing"
* Input           : None
* Return          : true if the PLL places theta at the crossing
* Calls           : SOGI_PLL_IsLocked, SOGI_PLL_GetCos
* Called by       : PfcZcMustEnterBlank, PfcZcLeaveBlank
* Note            : Always false when unlocked or PFC_ZC_PLL_WIN = 0
*******************************************************************************/
static bool PfcPllAtZeroCrossing(void)
{
    return SOGI_PLL_IsLocked(&g_pll)
           && (fabsf(SOGI_PLL_GetCos(&g_pll)) < PFC_ZC_PLL_WIN);
}

/*******************************************************************************
* Private Function : PfcVacSign
* Description     : Polarity of the measured grid voltage
* Purpose         : Map sign(vac) to POS / NEG
* Input           : vac - grid voltage [PU], signed
* Return          : PFC_PWM_POL_POS if vac > 0, else PFC_PWM_POL_NEG
* Calls           : None
* Called by       : PfcZcMustEnterBlank, PfcZcLeaveBlank
* Note            : vac == 0 maps to NEG; harmless, it is always inside BLANK
*******************************************************************************/
static PFC_PWM_POLARITY_e PfcVacSign(float vac)
{
    return (vac > 0.0f) ? PFC_PWM_POL_POS : PFC_PWM_POL_NEG;
}

/*******************************************************************************
* Private Function : PfcPllSign
* Description     : Polarity predicted by the PLL
* Purpose         : Map sign(cos(theta)) to POS / NEG
* Input           : None
* Return          : PFC_PWM_POL_POS if cos(theta) > 0, else PFC_PWM_POL_NEG
* Calls           : SOGI_PLL_GetCos
* Called by       : PfcZcLeaveBlank
* Note            : Only meaningful while the PLL is locked
*******************************************************************************/
static PFC_PWM_POLARITY_e PfcPllSign(void)
{
    return (SOGI_PLL_GetCos(&g_pll) > 0.0f) ? PFC_PWM_POL_POS : PFC_PWM_POL_NEG;
}

/*******************************************************************************
* Private Function : PfcSyncWindowCount
* Description     : PG1/PG2 window width (counts) of the sync-rectifier ramp
* Purpose         : On-time from PFC_SYNC_SS_START_NS to PFC_SYNC_DUTY_MAX of the
*                   period, linear in step, plus the dead time (the window starts
*                   on the edge without dead time)
* Input           : step - ramp step, 0 .. PFC_SYNC_SS_CYCLES
* Return          : window count, 0 .. g_pfcPwmPeriodCnt (PG3 period reference)
* Calls           : None
* Called by       : PFC_LOOP_PhaseAUpdate, PFC_LOOP_PhaseBUpdate
* Note            : Counts per ns are taken from g_pfcPwmPeriodCnt, the same
*                   scale as the duty write
*******************************************************************************/
static uint32_t PfcSyncWindowCount(uint16_t step)
{
    float tswNs = 1.0e9f / (float)BOARD_FAST_LOOP_HZ;
    float onNs  = PFC_SYNC_SS_START_NS
                + (PFC_SYNC_DUTY_MAX * tswNs - PFC_SYNC_SS_START_NS)
                * ((float)step / (float)PFC_SYNC_SS_CYCLES);
    float cnt   = (onNs + (float)APP_PFC_PWM_DEADTIME_NS) * (g_pfcPwmPeriodCnt / tswNs);

    if (cnt < 0.0f)               { cnt = 0.0f; }
    if (cnt > g_pfcPwmPeriodCnt)  { cnt = g_pfcPwmPeriodCnt; }
    return (uint32_t)(cnt + 0.5f);
}

/*******************************************************************************
* Private Function : PfcSoftStartBegin
* Description     : Arm the duty ramp of one phase after leaving BLANK
* Purpose         : Limit the first switching cycles after a zero crossing
* Input           : ss - phase soft-start state
* Return          : None
* Calls           : None
* Called by       : PFC_LOOP_PhaseAUpdate, PFC_LOOP_PhaseBUpdate
* Note            : The current loop stays frozen until hand-over
*******************************************************************************/
static void PfcSoftStartBegin(PFC_SS_T *ss)
{
    ss->dutyLim = 0.0f;
    ss->active  = true;
    ss->cycles = 0U;
}

/*******************************************************************************
* Private Function : PfcDutyCompute
* Description     : Duty of one phase: soft-start ramp or closed loop
* Purpose         : Coordinate the post-crossing ramp with the current loop
* Input           : vacAbs - |vac| [PU]
*                   ctrl   - phase current controller
*                   ss     - phase soft-start state
* Return          : duty (PFC_DUTY_MIN..PFC_DUTY_MAX)
* Calls           : SMPS_FloatController2P2ZErrorSeed, SMPS_FloatController2P2ZUpdate
* Called by       : PFC_LOOP_PhaseAUpdate, PFC_LOOP_PhaseBUpdate
* Note            : During the ramp the controller is frozen and the
*                   closed-loop duty is dff + frozen output. When the ramp
*                   reaches it after at least PFC_ZC_SS_CYCLES calls, seeded
*                   error/applied-output histories avoid an extra PI step. The
*                   closed-loop duty is clamped to PFC_DUTY_MAX, so hand-over
*                   happens within PFC_ZC_SS_CYCLES samples. PFC_ZC_SS_CYCLES
*                   0 or 1 hands over on the first sample
*******************************************************************************/
static float PfcDutyCompute(float vacAbs, SMPS_FLOAT_NPNZ_T const * const ctrl, PFC_SS_T *ss)
{
    float dutyFf = 1.0f - vacAbs * pfcInvVoutPU;
    float duty;

    if (ss->active)
    {
        duty = dutyFf + *ctrl->controlOutput;
        UPDNLMT(duty, PFC_DUTY_MAX, PFC_DUTY_MIN);
        ss->dutyLim += PFC_ZC_SS_STEP;
        ss->cycles++;
        if (ss->dutyLim < duty) { return ss->dutyLim; }
        if (ss->cycles < PFC_ZC_SS_CYCLES) { return duty; }
        ss->active = false;
        SMPS_FloatController2P2ZErrorSeed(ctrl);
        SMPS_FloatController2P2ZTrackOutput(ctrl, duty - dutyFf);
        return duty;   // No additional PI step on the handover sample
    }
    SMPS_FloatController2P2ZUpdate(ctrl);
    duty = dutyFf + *ctrl->controlOutput;
    UPDNLMT(duty, PFC_DUTY_MAX, PFC_DUTY_MIN);
    SMPS_FloatController2P2ZTrackOutput(ctrl, duty - dutyFf);
    return duty;
}

/******************************************************************************/
#undef PFC_LOOP_C
