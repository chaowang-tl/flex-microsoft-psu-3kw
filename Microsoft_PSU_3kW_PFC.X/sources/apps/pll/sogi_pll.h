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
* File Name       : sogi_pll.h
* Description     : Reusable single-phase SOGI-PLL module
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/05/03
* Modified by     : Chao Wang
* Modified on     : 2026/10/28
*******************************************************************************/
#ifndef SOGI_PLL_H
#define SOGI_PLL_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Include
#include <xc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
// Default sampling period [s]: PLL runs in the 65 kHz phase-A ADC ISR
#ifndef SOGI_PLL_TS_SEC
#define SOGI_PLL_TS_SEC         (1.0f / 65000.0f)   // Must equal 1 / BOARD_FAST_LOOP_HZ (checked in pfc_loop.c)
#endif

// Default nominal grid frequency [Hz]
#ifndef SOGI_PLL_F0_HZ
#define SOGI_PLL_F0_HZ          50.0f
#endif

// Default SOGI damping coefficient (sqrt(2): bandwidth/selectivity tradeoff)
#ifndef SOGI_PLL_K
#define SOGI_PLL_K              1.41421356f
#endif

// Default PLL closed-loop bandwidth [Hz], typical 20-50 Hz
#ifndef SOGI_PLL_FN_HZ
#define SOGI_PLL_FN_HZ          30.0f
#endif

// Default loop damping ratio (1/sqrt(2): Butterworth-like)
#ifndef SOGI_PLL_ZETA
#define SOGI_PLL_ZETA           0.70710678f
#endif

// Default operating frequency limits [Hz]
#ifndef SOGI_PLL_F_MIN_HZ
#define SOGI_PLL_F_MIN_HZ       44.0f
#endif
#ifndef SOGI_PLL_F_MAX_HZ
#define SOGI_PLL_F_MAX_HZ       67.0f
#endif

// Filtered phase-error threshold to assert LOCKED (0.05 ~= 2.9 deg)
#ifndef SOGI_PLL_LOCK_EPS
#define SOGI_PLL_LOCK_EPS       0.05f
#endif

// Filtered phase-error threshold to de-assert LOCKED (hysteresis)
#ifndef SOGI_PLL_UNLOCK_EPS
#define SOGI_PLL_UNLOCK_EPS     0.10f
#endif

// Lock-detection LPF time constant in nominal grid cycles
#ifndef SOGI_PLL_LOCK_TAU_CYC
#define SOGI_PLL_LOCK_TAU_CYC   3u
#endif

// Consecutive in-spec duration required for LOCKED [s]
#ifndef SOGI_PLL_LOCK_HOLD_SEC
#define SOGI_PLL_LOCK_HOLD_SEC  0.050f
#endif

// SOGI output amplitude-estimator time constant [s]
#ifndef SOGI_PLL_V_EST_TAU_SEC
#define SOGI_PLL_V_EST_TAU_SEC  0.020f
#endif

// Minimum valid amplitude for normalisation and locked=true [pu peak]
#ifndef SOGI_PLL_V_MIN_PU
#define SOGI_PLL_V_MIN_PU       0.10f
#endif

// Frequency display low-pass cutoff [Hz]
#ifndef SOGI_PLL_FREQ_LPF_CUTOFF_HZ
#define SOGI_PLL_FREQ_LPF_CUTOFF_HZ  1.0f
#endif

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions
typedef struct {
    float_t  Ts;                 // Sampling period [s]
    float_t  f0_Hz;              // Nominal grid frequency [Hz]
    float_t  k;                  // SOGI damping coefficient
    float_t  fn_Hz;              // PLL closed-loop bandwidth [Hz]
    float_t  zeta;               // Loop damping ratio
    float_t  f_min_Hz;           // Lower accepted/clamped frequency [Hz]
    float_t  f_max_Hz;           // Upper accepted/clamped frequency [Hz]
    float_t  lock_eps;           // Filtered |eps_norm| lock threshold
    float_t  unlock_eps;         // Filtered |eps_norm| unlock threshold
    uint32_t lock_tau_cycles;    // Lock detector LPF tau in nominal cycles
    float_t  lock_hold_sec;      // Consecutive valid time before locked=true
    float_t  v_est_tau_sec;      // Amplitude estimator time constant [s]
    float_t  v_min_pu;           // Minimum valid SOGI amplitude [pu peak]
    float_t  freq_lpf_cutoff_Hz; // Display frequency LPF cutoff [Hz]
} SOGI_PLL_CONFIG_T;

typedef struct {
    // SOGI states (pu, same base as input)
    float_t  x1;                 // va : in-phase filtered output
    float_t  x2;                 // vb : quadrature output (lags va 90 deg)
    float_t  k;                  // SOGI damping coefficient

    // PI loop filter
    float_t  Kp;                 // Proportional gain
    float_t  Ki;                 // Integral gain
    float_t  invKi;              // 1/Ki, used by anti-windup back-calculation
    float_t  int_state;          // Integrator accumulator (Tustin method)
    float_t  eps_prev;           // Previous normalised phase error

    // Amplitude normalisation
    float_t  v_est;              // Estimated SOGI output peak amplitude [pu]
    float_t  v_est_alpha;        // EMA coefficient for v_est
    float_t  eps_norm;           // Normalised phase error, eps_raw/v_est_safe
    float_t  v_min;              // Minimum valid amplitude for normalisation

    // VCO
    float_t  omega;              // Estimated angular frequency [rad/s]
    float_t  omega_ff;           // Feed-forward angular frequency [rad/s]
    float_t  theta;              // Estimated phase angle [0, 2*pi) [rad]
    float_t  sin_theta;          // Cached sin(theta)
    float_t  cos_theta;          // Cached cos(theta); input is aligned to cos

    // Frequency
    float_t  freq_raw;           // Unfiltered estimated frequency [Hz]
    float_t  freq_filt;          // Low-pass filtered frequency [Hz]
    float_t  freq_filt_alpha;    // EMA coefficient for freq_filt
    bool     freq_limited;       // True when omega was clamped this update

    // Lock detection
    float_t  eps_mag_lpf;        // Low-pass filtered |eps_norm|
    float_t  lock_alpha;         // EMA coefficient for eps_mag_lpf
    float_t  lock_eps;           // Lock threshold
    float_t  unlock_eps;         // Unlock threshold
    uint32_t lock_cnt;           // Consecutive in-spec sample counter
    uint32_t lock_hold_cnt_limit;// Lock hold sample limit from Ts
    bool     locked;             // PLL lock status flag

    // Configuration (set by Init, read-only afterwards)
    float_t  Ts;                 // Sampling period [s]
    float_t  omega_min;          // Minimum clamped omega [rad/s]
    float_t  omega_max;          // Maximum clamped omega [rad/s]
    float_t  f0;                 // Nominal frequency [Hz]
} SOGI_PLL_T;

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef SOGI_PLL_C
    #define SOGI_PLL_C_PUBLIC
    #define SOGI_PLL_C_CONST
#else
    #define SOGI_PLL_C_PUBLIC extern
    #define SOGI_PLL_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables

//------------------------------------------------------------------------------
// Public Function Prototypes
SOGI_PLL_C_PUBLIC void SOGI_PLL_ConfigDefault(SOGI_PLL_CONFIG_T *pCfg);
SOGI_PLL_C_PUBLIC void SOGI_PLL_InitConfig(SOGI_PLL_T *pPll, const SOGI_PLL_CONFIG_T *pCfg);
SOGI_PLL_C_PUBLIC void SOGI_PLL_Init(SOGI_PLL_T *pPll, float_t Ts, float_t f0_Hz,
                                     float_t k, float_t fn_Hz, float_t zeta);
SOGI_PLL_C_PUBLIC void SOGI_PLL_Reset(SOGI_PLL_T *pPll);
SOGI_PLL_C_PUBLIC void SOGI_PLL_Update(SOGI_PLL_T *pPll, float_t vIn);

// Estimated grid phase angle [0, 2*pi) [rad]
inline static float_t SOGI_PLL_GetAngle(const SOGI_PLL_T *pPll)          { return pPll->theta; }
// Low-pass filtered grid frequency [Hz]
inline static float_t SOGI_PLL_GetFrequency(const SOGI_PLL_T *pPll)      { return pPll->freq_filt; }
// Unfiltered grid frequency [Hz]
inline static float_t SOGI_PLL_GetFrequencyRaw(const SOGI_PLL_T *pPll)   { return pPll->freq_raw; }
// Cached sin(theta)
inline static float_t SOGI_PLL_GetSin(const SOGI_PLL_T *pPll)            { return pPll->sin_theta; }
// Cached cos(theta); in phase with the input voltage
inline static float_t SOGI_PLL_GetCos(const SOGI_PLL_T *pPll)            { return pPll->cos_theta; }
// Estimated input peak amplitude [pu]
inline static float_t SOGI_PLL_GetAmplitude(const SOGI_PLL_T *pPll)      { return pPll->v_est; }
// Normalised phase-detector output
inline static float_t SOGI_PLL_GetPhaseError(const SOGI_PLL_T *pPll)     { return pPll->eps_norm; }
// True when omega was clamped in the last update
inline static bool    SOGI_PLL_IsFrequencyLimited(const SOGI_PLL_T *pPll){ return pPll->freq_limited; }
// True when PLL is locked
inline static bool    SOGI_PLL_IsLocked(const SOGI_PLL_T *pPll)          { return pPll->locked; }

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* SOGI_PLL_H */
