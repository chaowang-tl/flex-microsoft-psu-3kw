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
* File Name       : pfc_pwm.h
* Description     : Totem-pole PFC PWM access layer (fast legs PG3/PG4, slow leg PG5)
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/28
* Modified by     : Chao Wang
* Modified on     : 2026/11/10
*******************************************************************************/
#ifndef PFC_PWM_H
#define PFC_PWM_H

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
// Override masks in PGxIOCON2
#define PFC_PWM_OVREN_MASK          (_PG3IOCON2_OVRENH_MASK | _PG3IOCON2_OVRENL_MASK)
#define PFC_PWM_OVRDAT_MASK         _PG3IOCON2_OVRDAT_MASK
#define PFC_PWM_OVRDAT_POSITION     _PG3IOCON2_OVRDAT_POSITION

// Slow-leg override data per polarity (OVRDAT<1> = PWMxH, OVRDAT<0> = PWMxL)
// TODO: verify against schematic which slow-leg switch conducts in each half-cycle
#define PFC_PWM_SLOW_LEG_POS_OVRDAT 0x1U
#define PFC_PWM_SLOW_LEG_NEG_OVRDAT 0x2U

// Fast-leg SWAP per polarity so PGxDC always drives the boost switch
// TODO: verify; positive half-cycle assumes the low-side fast switch is the boost switch
#define PFC_PWM_FAST_LEG_SWAP_POS   1U
#define PFC_PWM_FAST_LEG_SWAP_NEG   0U

// Override-enable bit of the pin carrying the synchronous rectifier per half-cycle.
// Override acts on the pins, after SWAP: with SWAP = 1 PGxDC drives the PWMxL pin,
// so the rectifier is on PWMxH, and the other way round with SWAP = 0.
// TODO: verify; assumes PWMxH pin = high-side switch, PWMxL pin = low-side switch
#define PFC_PWM_SYNC_OVREN_POS      _PG3IOCON2_OVRENH_MASK
#define PFC_PWM_SYNC_OVREN_NEG      _PG3IOCON2_OVRENL_MASK

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions
typedef enum
{
    PFC_PWM_POL_BLANK = 0,      // Zero-crossing window: all legs off
    PFC_PWM_POL_POS,            // vac > 0
    PFC_PWM_POL_NEG             // vac < 0
} PFC_PWM_POLARITY_e;

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef PFC_PWM_C
    #define PFC_PWM_C_PUBLIC
    #define PFC_PWM_C_CONST
#else
    #define PFC_PWM_C_PUBLIC extern
    #define PFC_PWM_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
PFC_PWM_C_PUBLIC float g_pfcPwmPeriodCnt;   // PG3PER as float, set by PFC_PWM_Init

//------------------------------------------------------------------------------
// Public Function Prototypes
PFC_PWM_C_PUBLIC void PFC_PWM_Init(void);
PFC_PWM_C_PUBLIC void PFC_PWM_AllOutputsOff(void);
PFC_PWM_C_PUBLIC void PFC_PWM_SlowLegSet(PFC_PWM_POLARITY_e polarity);

// Sync-window gate: PG1 (phase A) / PG2 (phase B) window pins PWM1H/L and PWM2H/L
// feed the LOGCON AND gates. Override on the PG1/PG2 pins forces each window side:
//   OVREN bit set + OVRDAT 1 = pass (window forced high, boost side untouched)
//   OVREN bit set + OVRDAT 0 = block (window forced low, sync side off)
//   OVREN bit clear          = follow the PG1/PG2 window pulse
#define PFC_PWM_WIN_OVREN_ALL_A     (_PG1IOCON2_OVRENH_MASK | _PG1IOCON2_OVRENL_MASK)
#define PFC_PWM_WIN_OVREN_ALL_B     (_PG2IOCON2_OVRENH_MASK | _PG2IOCON2_OVRENL_MASK)
#define PFC_PWM_WIN_DAT_MASK        _PG1IOCON2_OVRDAT_MASK   // Same field position in PG2
#define PFC_PWM_WIN_DAT_H           (2UL << _PG1IOCON2_OVRDAT_POSITION)  // OVRDAT<1> = PWMxH
#define PFC_PWM_WIN_DAT_L           (1UL << _PG1IOCON2_OVRDAT_POSITION)  // OVRDAT<0> = PWMxL

// Write phase-A boost duty (0..1). TRIGB (duty point, bit31 = CAHALF) is written
// before DC so both take effect at the same SOC.
inline static void PFC_PWM_DutyASet(float duty)
{
    uint32_t dc = (uint32_t)(duty * g_pfcPwmPeriodCnt) & 0x000FFFFFUL;
    PG3TRIGB = dc | (1UL << 31);
    PG3DC    = dc;
}

// Write phase-B boost duty (0..1); TRIGB before DC, as for phase A
inline static void PFC_PWM_DutyBSet(float duty)
{
    uint32_t dc = (uint32_t)(duty * g_pfcPwmPeriodCnt) & 0x000FFFFFUL;
    PG4TRIGB = dc | (1UL << 31);
    PG4DC    = dc;
}

// Boost-side window bit pattern for a polarity (the side that is not the sync pin).
// POS: sync on PWMxH, boost on PWMxL. NEG: sync on PWMxL, boost on PWMxH.
inline static uint32_t PFC_PWM_WinBoostDat(PFC_PWM_POLARITY_e polarity)
{
    return (polarity == PFC_PWM_POL_NEG) ? PFC_PWM_WIN_DAT_H : PFC_PWM_WIN_DAT_L;
}

inline static uint32_t PFC_PWM_WinBoostOvren(PFC_PWM_POLARITY_e polarity, uint32_t ovrenH, uint32_t ovrenL)
{
    return (polarity == PFC_PWM_POL_NEG) ? ovrenH : ovrenL;
}

// Phase-A window gate
inline static void PFC_PWM_PhaseASyncGateOpen(void)
{
    PG1IOCON2 = (PG1IOCON2 & ~(PFC_PWM_WIN_OVREN_ALL_A | PFC_PWM_WIN_DAT_MASK))
              | PFC_PWM_WIN_OVREN_ALL_A | PFC_PWM_WIN_DAT_H | PFC_PWM_WIN_DAT_L;
}

// Pending: sync side blocked, boost side passed through
inline static void PFC_PWM_PhaseASyncGatePending(PFC_PWM_POLARITY_e polarity)
{
    PG1IOCON2 = (PG1IOCON2 & ~(PFC_PWM_WIN_OVREN_ALL_A | PFC_PWM_WIN_DAT_MASK))
              | PFC_PWM_WIN_OVREN_ALL_A | PFC_PWM_WinBoostDat(polarity);
}

// Window: boost side passed through, sync side follows the PG1 window pulse
inline static void PFC_PWM_PhaseASyncGateWindow(PFC_PWM_POLARITY_e polarity)
{
    PG1IOCON2 = (PG1IOCON2 & ~(PFC_PWM_WIN_OVREN_ALL_A | PFC_PWM_WIN_DAT_MASK))
              | PFC_PWM_WinBoostOvren(polarity, _PG1IOCON2_OVRENH_MASK, _PG1IOCON2_OVRENL_MASK)
              | PFC_PWM_WinBoostDat(polarity);
}

// Window width of the sync pulse (PG1DC), counts; takes effect at the next PG1 SOC
inline static void PFC_PWM_PhaseASyncWindowSet(uint32_t windowCnt)
{
    PG1DC = windowCnt & 0x000FFFFFUL;
}

// Phase-B window gate
inline static void PFC_PWM_PhaseBSyncGateOpen(void)
{
    PG2IOCON2 = (PG2IOCON2 & ~(PFC_PWM_WIN_OVREN_ALL_B | PFC_PWM_WIN_DAT_MASK))
              | PFC_PWM_WIN_OVREN_ALL_B | PFC_PWM_WIN_DAT_H | PFC_PWM_WIN_DAT_L;
}

inline static void PFC_PWM_PhaseBSyncGatePending(PFC_PWM_POLARITY_e polarity)
{
    PG2IOCON2 = (PG2IOCON2 & ~(PFC_PWM_WIN_OVREN_ALL_B | PFC_PWM_WIN_DAT_MASK))
              | PFC_PWM_WIN_OVREN_ALL_B | PFC_PWM_WinBoostDat(polarity);
}

inline static void PFC_PWM_PhaseBSyncGateWindow(PFC_PWM_POLARITY_e polarity)
{
    PG2IOCON2 = (PG2IOCON2 & ~(PFC_PWM_WIN_OVREN_ALL_B | PFC_PWM_WIN_DAT_MASK))
              | PFC_PWM_WinBoostOvren(polarity, _PG2IOCON2_OVRENH_MASK, _PG2IOCON2_OVRENL_MASK)
              | PFC_PWM_WinBoostDat(polarity);
}

inline static void PFC_PWM_PhaseBSyncWindowSet(uint32_t windowCnt)
{
    PG2DC = windowCnt & 0x000FFFFFUL;
}

// Override-enable bit of the synchronous rectifier pin for the given polarity
inline static uint32_t PFC_PWM_SyncOvrenMask(PFC_PWM_POLARITY_e polarity)
{
    return (polarity == PFC_PWM_POL_NEG) ? PFC_PWM_SYNC_OVREN_NEG
                                         : PFC_PWM_SYNC_OVREN_POS;
}

// Release the phase-A boost-switch pin; the synchronous rectifier pin stays forced low
inline static void PFC_PWM_PhaseABoostEnable(PFC_PWM_POLARITY_e polarity)
{
    PG3IOCON2 &= ~(PFC_PWM_OVREN_MASK & ~PFC_PWM_SyncOvrenMask(polarity));
}

// Release the phase-A synchronous rectifier pin (after the boost pin is released)
inline static void PFC_PWM_PhaseASyncEnable(PFC_PWM_POLARITY_e polarity)
{
    PG3IOCON2 &= ~PFC_PWM_SyncOvrenMask(polarity);
}

// Force phase-A outputs low
inline static void PFC_PWM_PhaseADisable(void)
{
    PG3IOCON2 = (PG3IOCON2 & ~PFC_PWM_OVRDAT_MASK) | PFC_PWM_OVREN_MASK;
}

// Release the phase-B boost-switch pin; the synchronous rectifier pin stays forced low
inline static void PFC_PWM_PhaseBBoostEnable(PFC_PWM_POLARITY_e polarity)
{
    PG4IOCON2 &= ~(PFC_PWM_OVREN_MASK & ~PFC_PWM_SyncOvrenMask(polarity));
}

// Release the phase-B synchronous rectifier pin (after the boost pin is released)
inline static void PFC_PWM_PhaseBSyncEnable(PFC_PWM_POLARITY_e polarity)
{
    PG4IOCON2 &= ~PFC_PWM_SyncOvrenMask(polarity);
}

// Force phase-B outputs low
inline static void PFC_PWM_PhaseBDisable(void)
{
    PG4IOCON2 = (PG4IOCON2 & ~PFC_PWM_OVRDAT_MASK) | PFC_PWM_OVREN_MASK;
}

// Select which fast-leg switch PG3DC drives for the given polarity
inline static void PFC_PWM_PhaseASwapSet(PFC_PWM_POLARITY_e polarity)
{
    PG3IOCON1bits.SWAP = (polarity == PFC_PWM_POL_NEG) ? PFC_PWM_FAST_LEG_SWAP_NEG
                                                        : PFC_PWM_FAST_LEG_SWAP_POS;
}

// Select which fast-leg switch PG4DC drives for the given polarity
inline static void PFC_PWM_PhaseBSwapSet(PFC_PWM_POLARITY_e polarity)
{
    PG4IOCON1bits.SWAP = (polarity == PFC_PWM_POL_NEG) ? PFC_PWM_FAST_LEG_SWAP_NEG
                                                        : PFC_PWM_FAST_LEG_SWAP_POS;
}

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* PFC_PWM_H */
