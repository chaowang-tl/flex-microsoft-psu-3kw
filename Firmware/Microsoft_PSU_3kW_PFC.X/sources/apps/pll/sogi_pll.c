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
* File Name       : sogi_pll.c
* Description     : Reusable single-phase SOGI-PLL implementation
*
*   v_in --> [SOGI] --> va, vb --> [PD + V normalise] --> eps_norm --> [PI + VCO] --> theta, f
*                ^_________________________feedback omega___________________________|
*
*   1. SOGI quadrature signal generator (forward Euler, frequency adaptive)
*      x1_dot = w * ( k*(v_in - x1) - x2 )      x1 = va (in-phase, va ~= V*cos(theta))
*      x2_dot = w * x1                          x2 = vb (lags va by 90 deg)
*   2. Phase detector + amplitude normalisation
*      eps_raw  = vb*cos(theta_hat) - va*sin(theta_hat) = V*sin(theta_grid - theta_hat)
*      eps_norm = eps_raw / max(v_est, v_min)
*   3. PI loop filter (Tustin) + VCO, type-2 PLL
*      omega_hat = omega_ff + Kp*eps_norm + Ki*integral(eps_norm)
*      theta_hat += Ts * omega_hat
*   Gain design (normalised phase detector, Kd ~= 1):
*      wn = 2*pi*fn_Hz,  Kp = 2*zeta*wn,  Ki = wn^2
*
*   The input voltage lines up with cos(theta): use SOGI_PLL_GetCos() as the
*   current-reference shape and as the polarity reference.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/05/03
* Modified by     : Chao Wang
* Modified on     : 2026/10/28
*******************************************************************************/
#define SOGI_PLL_C

//------------------------------------------------------------------------------
// Include
#include "sogi_pll.h"
#include <string.h>

//------------------------------------------------------------------------------
// Constants and Macro Definitions
#define TWO_PI_F    6.28318530f
#define INV_TWO_PI  0.15915494f

//------------------------------------------------------------------------------
// Public Variables

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes
static float_t  PositiveOrDefault(float_t value, float_t defaultValue);
static uint32_t NonZeroOrDefault(uint32_t value, uint32_t defaultValue);
//------------------------------------------------------------------------------
// Private Variables

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : SOGI_PLL_ConfigDefault
* Description     : Fill a configuration structure with module defaults
* Purpose         : Let applications customise only the fields they need
* Input           : pCfg - pointer to configuration structure
* Return          : None
* Calls           : None
* Called by       : Application, SOGI_PLL_Init, SOGI_PLL_InitConfig
* Note            : Values are compile-time defaults
*******************************************************************************/
void SOGI_PLL_ConfigDefault(SOGI_PLL_CONFIG_T *pCfg)
{
    if (pCfg == NULL)
    {
        return;
    }

    pCfg->Ts                 = SOGI_PLL_TS_SEC;
    pCfg->f0_Hz              = SOGI_PLL_F0_HZ;
    pCfg->k                  = SOGI_PLL_K;
    pCfg->fn_Hz              = SOGI_PLL_FN_HZ;
    pCfg->zeta               = SOGI_PLL_ZETA;
    pCfg->f_min_Hz           = SOGI_PLL_F_MIN_HZ;
    pCfg->f_max_Hz           = SOGI_PLL_F_MAX_HZ;
    pCfg->lock_eps           = SOGI_PLL_LOCK_EPS;
    pCfg->unlock_eps         = SOGI_PLL_UNLOCK_EPS;
    pCfg->lock_tau_cycles    = SOGI_PLL_LOCK_TAU_CYC;
    pCfg->lock_hold_sec      = SOGI_PLL_LOCK_HOLD_SEC;
    pCfg->v_est_tau_sec      = SOGI_PLL_V_EST_TAU_SEC;
    pCfg->v_min_pu           = SOGI_PLL_V_MIN_PU;
    pCfg->freq_lpf_cutoff_Hz = SOGI_PLL_FREQ_LPF_CUTOFF_HZ;
}

/*******************************************************************************
* Public Function : SOGI_PLL_InitConfig
* Description     : Initialise PLL from a configuration structure
* Purpose         : Configure an instance without editing macros
* Input           : pPll - pointer to SOGI_PLL_T instance
*                   pCfg - pointer to configuration, NULL selects defaults
* Return          : None
* Calls           : memset, SOGI_PLL_ConfigDefault
* Called by       : Application
* Note            : Invalid or non-positive fields fall back to defaults.
*                   Do not call while the ISR is running SOGI_PLL_Update.
*******************************************************************************/
void SOGI_PLL_InitConfig(SOGI_PLL_T *pPll, const SOGI_PLL_CONFIG_T *pCfg)
{
    SOGI_PLL_CONFIG_T defaultCfg;
    SOGI_PLL_CONFIG_T cfg;
    float_t wn;
    float_t tau;
    float_t tauF;

    if (pPll == NULL)
    {
        return;
    }

    SOGI_PLL_ConfigDefault(&defaultCfg);
    cfg = (pCfg != NULL) ? *pCfg : defaultCfg;

    cfg.Ts                 = PositiveOrDefault(cfg.Ts, defaultCfg.Ts);
    cfg.f0_Hz              = PositiveOrDefault(cfg.f0_Hz, defaultCfg.f0_Hz);
    cfg.k                  = PositiveOrDefault(cfg.k, defaultCfg.k);
    cfg.fn_Hz              = PositiveOrDefault(cfg.fn_Hz, defaultCfg.fn_Hz);
    cfg.zeta               = PositiveOrDefault(cfg.zeta, defaultCfg.zeta);
    cfg.f_min_Hz           = PositiveOrDefault(cfg.f_min_Hz, defaultCfg.f_min_Hz);
    cfg.f_max_Hz           = PositiveOrDefault(cfg.f_max_Hz, defaultCfg.f_max_Hz);
    cfg.lock_eps           = PositiveOrDefault(cfg.lock_eps, defaultCfg.lock_eps);
    cfg.unlock_eps         = PositiveOrDefault(cfg.unlock_eps, defaultCfg.unlock_eps);
    cfg.lock_tau_cycles    = NonZeroOrDefault(cfg.lock_tau_cycles, defaultCfg.lock_tau_cycles);
    cfg.lock_hold_sec      = PositiveOrDefault(cfg.lock_hold_sec, defaultCfg.lock_hold_sec);
    cfg.v_est_tau_sec      = PositiveOrDefault(cfg.v_est_tau_sec, defaultCfg.v_est_tau_sec);
    cfg.v_min_pu           = PositiveOrDefault(cfg.v_min_pu, defaultCfg.v_min_pu);
    cfg.freq_lpf_cutoff_Hz = PositiveOrDefault(cfg.freq_lpf_cutoff_Hz, defaultCfg.freq_lpf_cutoff_Hz);

    if (cfg.f_min_Hz >= cfg.f_max_Hz)
    {
        cfg.f_min_Hz = defaultCfg.f_min_Hz;
        cfg.f_max_Hz = defaultCfg.f_max_Hz;
    }

    if (cfg.unlock_eps <= cfg.lock_eps)
    {
        cfg.unlock_eps = defaultCfg.unlock_eps;
    }

    memset(pPll, 0, sizeof(SOGI_PLL_T));

    pPll->Ts         = cfg.Ts;
    pPll->k          = cfg.k;
    pPll->f0         = cfg.f0_Hz;
    pPll->omega_ff   = TWO_PI_F * cfg.f0_Hz;
    pPll->omega      = pPll->omega_ff;
    pPll->omega_min  = TWO_PI_F * cfg.f_min_Hz;
    pPll->omega_max  = TWO_PI_F * cfg.f_max_Hz;
    pPll->freq_raw   = cfg.f0_Hz;
    pPll->sin_theta  = 0.0f;
    pPll->cos_theta  = 1.0f;
    pPll->v_min      = cfg.v_min_pu;
    pPll->lock_eps   = cfg.lock_eps;
    pPll->unlock_eps = cfg.unlock_eps;

    // PI gains: Kp = 2*zeta*wn, Ki = wn^2
    wn = TWO_PI_F * cfg.fn_Hz;
    pPll->Kp    = 2.0f * cfg.zeta * wn;
    pPll->Ki    = wn * wn;
    pPll->invKi = 1.0f / pPll->Ki;

    // Lock-detection LPF
    tau = (float_t)cfg.lock_tau_cycles / cfg.f0_Hz;
    pPll->lock_alpha = cfg.Ts / (tau + cfg.Ts);
    pPll->lock_hold_cnt_limit = (uint32_t)((cfg.lock_hold_sec / cfg.Ts) + 0.5f);
    if (pPll->lock_hold_cnt_limit == 0u)
    {
        pPll->lock_hold_cnt_limit = 1u;
    }
    else if (pPll->lock_hold_cnt_limit > 100000u)
    {
        pPll->lock_hold_cnt_limit = 100000u;
    }

    // Frequency display LPF
    tauF = 1.0f / (TWO_PI_F * cfg.freq_lpf_cutoff_Hz);
    pPll->freq_filt_alpha = cfg.Ts / (tauF + cfg.Ts);
    pPll->freq_filt       = cfg.f0_Hz;

    // Amplitude estimator LPF
    pPll->v_est_alpha = cfg.Ts / (cfg.v_est_tau_sec + cfg.Ts);
}

/*******************************************************************************
* Public Function : SOGI_PLL_Init
* Description     : Initialise PLL with the most common parameters
* Purpose         : Compatibility wrapper around SOGI_PLL_InitConfig
* Input           : pPll  - pointer to SOGI_PLL_T instance
*                   Ts    - sampling period [s]
*                   f0_Hz - nominal grid frequency [Hz]
*                   k     - SOGI damping gain
*                   fn_Hz - PLL closed-loop bandwidth [Hz]
*                   zeta  - loop damping ratio
* Return          : None
* Calls           : SOGI_PLL_ConfigDefault, SOGI_PLL_InitConfig
* Called by       : Application
* Note            : None
*******************************************************************************/
void SOGI_PLL_Init(SOGI_PLL_T *pPll, float_t Ts, float_t f0_Hz,
                   float_t k, float_t fn_Hz, float_t zeta)
{
    SOGI_PLL_CONFIG_T cfg;

    SOGI_PLL_ConfigDefault(&cfg);
    cfg.Ts    = Ts;
    cfg.f0_Hz = f0_Hz;
    cfg.k     = k;
    cfg.fn_Hz = fn_Hz;
    cfg.zeta  = zeta;

    SOGI_PLL_InitConfig(pPll, &cfg);
}

/*******************************************************************************
* Public Function : SOGI_PLL_Reset
* Description     : Zero all integrators and states, keep parameters
* Purpose         : Restart synchronisation after a grid fault
* Input           : pPll - pointer to SOGI_PLL_T instance
* Return          : None
* Calls           : None
* Called by       : Application
* Note            : Parameters from Init/InitConfig are preserved
*******************************************************************************/
void SOGI_PLL_Reset(SOGI_PLL_T *pPll)
{
    if (pPll == NULL)
    {
        return;
    }

    pPll->x1           = 0.0f;
    pPll->x2           = 0.0f;
    pPll->int_state    = 0.0f;
    pPll->eps_prev     = 0.0f;
    pPll->omega        = pPll->omega_ff;
    pPll->theta        = 0.0f;
    pPll->sin_theta    = 0.0f;
    pPll->cos_theta    = 1.0f;
    pPll->freq_raw     = pPll->f0;
    pPll->freq_filt    = pPll->f0;
    pPll->freq_limited = false;
    pPll->v_est        = 0.0f;
    pPll->eps_norm     = 0.0f;
    pPll->eps_mag_lpf  = 0.0f;
    pPll->lock_cnt     = 0u;
    pPll->locked       = false;
}

/*******************************************************************************
* Public Function : SOGI_PLL_Update
* Description     : Execute one PLL iteration
* Purpose         : Track grid phase, frequency and amplitude in real time
* Input           : pPll - pointer to SOGI_PLL_T instance
*                   vIn  - per-unit grid voltage sample
* Return          : None
* Calls           : sinf, cosf, sqrtf, fabsf
* Called by       : PFC_LOOP_GridUpdate (65 kHz phase-A ISR)
* Note            : Order: SOGI -> PD -> v_est -> eps_norm -> PI -> VCO -> lock.
*                   About 1.3 us on dsPIC33AK at -O2 (SOGI_PLL_Design.md 7.2)
*******************************************************************************/
void SOGI_PLL_Update(SOGI_PLL_T *pPll, float_t vIn)
{
    const float_t Ts = pPll->Ts;
    float_t e;
    float_t dx1;
    float_t dx2;
    float_t epsRaw;
    float_t vMag;
    float_t vEstSafe;
    float_t eps;
    float_t newInt;
    float_t omegaRaw;
    float_t epsAbs;
    bool    inRange;
    bool    voltageOk;

    // 1. SOGI (forward Euler, adaptive resonance w = omega)
    e   = vIn - pPll->x1;
    dx1 = pPll->omega * (pPll->k * e - pPll->x2);
    dx2 = pPll->omega * pPll->x1;
    pPll->x1 += Ts * dx1;
    pPll->x2 += Ts * dx2;

    // 2. Phase detector and amplitude normalisation
    epsRaw = pPll->x2 * pPll->cos_theta - pPll->x1 * pPll->sin_theta;

    vMag = sqrtf((pPll->x1 * pPll->x1) + (pPll->x2 * pPll->x2));
    pPll->v_est += pPll->v_est_alpha * (vMag - pPll->v_est);

    vEstSafe = (pPll->v_est > pPll->v_min) ? pPll->v_est : pPll->v_min;
    eps = epsRaw / vEstSafe;
    pPll->eps_norm = eps;

    // 3. PI loop filter (Tustin), back-calculation anti-windup when clamped
    newInt   = pPll->int_state + Ts * 0.5f * (eps + pPll->eps_prev);
    omegaRaw = pPll->omega_ff + pPll->Kp * eps + pPll->Ki * newInt;

    if (omegaRaw > pPll->omega_max)
    {
        pPll->int_state    = (pPll->omega_max - pPll->omega_ff - pPll->Kp * eps) * pPll->invKi;
        pPll->omega        = pPll->omega_max;
        pPll->freq_limited = true;
    }
    else if (omegaRaw < pPll->omega_min)
    {
        pPll->int_state    = (pPll->omega_min - pPll->omega_ff - pPll->Kp * eps) * pPll->invKi;
        pPll->omega        = pPll->omega_min;
        pPll->freq_limited = true;
    }
    else
    {
        pPll->int_state    = newInt;
        pPll->omega        = omegaRaw;
        pPll->freq_limited = false;
    }

    pPll->eps_prev = eps;

    // 4. VCO, wrap to [0, 2*pi)
    pPll->theta += Ts * pPll->omega;
    if (pPll->theta >= TWO_PI_F)
    {
        pPll->theta -= TWO_PI_F;
    }
    else if (pPll->theta < 0.0f)
    {
        pPll->theta += TWO_PI_F;
    }

    pPll->sin_theta = sinf(pPll->theta);
    pPll->cos_theta = cosf(pPll->theta);

    pPll->freq_raw   = pPll->omega * INV_TWO_PI;
    pPll->freq_filt += pPll->freq_filt_alpha * (pPll->freq_raw - pPll->freq_filt);

    // 5. Lock detection: hold before lock, drop immediately on loss
    epsAbs = fabsf(pPll->eps_norm);
    pPll->eps_mag_lpf += pPll->lock_alpha * (epsAbs - pPll->eps_mag_lpf);

    inRange   = !pPll->freq_limited;
    voltageOk = (pPll->v_est >= pPll->v_min);

    if (!pPll->locked)
    {
        if ((pPll->eps_mag_lpf < pPll->lock_eps) && inRange && voltageOk)
        {
            if (pPll->lock_cnt < pPll->lock_hold_cnt_limit)
            {
                pPll->lock_cnt++;
            }
            if (pPll->lock_cnt >= pPll->lock_hold_cnt_limit)
            {
                pPll->locked = true;
            }
        }
        else
        {
            pPll->lock_cnt = 0u;
        }
    }
    else if ((pPll->eps_mag_lpf > pPll->unlock_eps) || !inRange || !voltageOk)
    {
        pPll->locked   = false;
        pPll->lock_cnt = 0u;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation
/*******************************************************************************
* Private Function : PositiveOrDefault
* Description     : Select a positive value or a fallback
* Purpose         : Sanitise configuration fields
* Input           : value        - candidate value
*                   defaultValue - fallback value
* Return          : value if > 0, otherwise defaultValue
* Calls           : None
* Called by       : SOGI_PLL_InitConfig
* Note            : None
*******************************************************************************/
static float_t PositiveOrDefault(float_t value, float_t defaultValue)
{
    return (value > 0.0f) ? value : defaultValue;
}

/*******************************************************************************
* Private Function : NonZeroOrDefault
* Description     : Select a non-zero integer value or a fallback
* Purpose         : Sanitise configuration fields
* Input           : value        - candidate value
*                   defaultValue - fallback value
* Return          : value if != 0, otherwise defaultValue
* Calls           : None
* Called by       : SOGI_PLL_InitConfig
* Note            : None
*******************************************************************************/
static uint32_t NonZeroOrDefault(uint32_t value, uint32_t defaultValue)
{
    return (value != 0u) ? value : defaultValue;
}

/******************************************************************************/
#undef SOGI_PLL_C
