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
* Description     : Totem-pole PFC PWM access layer (fast legs PG1/PG2, slow leg PG3)
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/28
* Modified by     : Chao Wang
* Modified on     : 2026/10/30
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
#define PFC_PWM_OVREN_MASK          (_PG1IOCON2_OVRENH_MASK | _PG1IOCON2_OVRENL_MASK)
#define PFC_PWM_OVRDAT_MASK         _PG1IOCON2_OVRDAT_MASK
#define PFC_PWM_OVRDAT_POSITION     _PG1IOCON2_OVRDAT_POSITION

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
#define PFC_PWM_SYNC_OVREN_POS      _PG1IOCON2_OVRENH_MASK
#define PFC_PWM_SYNC_OVREN_NEG      _PG1IOCON2_OVRENL_MASK

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
PFC_PWM_C_PUBLIC float g_pfcPwmPeriodCnt;   // PG1PER as float, set by PFC_PWM_Init

//------------------------------------------------------------------------------
// Public Function Prototypes
PFC_PWM_C_PUBLIC void PFC_PWM_Init(void);
PFC_PWM_C_PUBLIC void PFC_PWM_AllOutputsOff(void);
PFC_PWM_C_PUBLIC void PFC_PWM_SlowLegSet(PFC_PWM_POLARITY_e polarity);

// Write phase-A boost duty (0..1); the DC write also latches pending overrides
inline static void PFC_PWM_DutyASet(float duty)
{
    PG1DC = (uint32_t)(duty * g_pfcPwmPeriodCnt) & 0x000FFFFFUL;
}

// Write phase-B boost duty (0..1)
inline static void PFC_PWM_DutyBSet(float duty)
{
    PG2DC = (uint32_t)(duty * g_pfcPwmPeriodCnt) & 0x000FFFFFUL;
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
    PG1IOCON2 &= ~(PFC_PWM_OVREN_MASK & ~PFC_PWM_SyncOvrenMask(polarity));
}

// Release the phase-A synchronous rectifier pin (after the boost pin is released)
inline static void PFC_PWM_PhaseASyncEnable(PFC_PWM_POLARITY_e polarity)
{
    PG1IOCON2 &= ~PFC_PWM_SyncOvrenMask(polarity);
}

// Force phase-A outputs low
inline static void PFC_PWM_PhaseADisable(void)
{
    PG1IOCON2 = (PG1IOCON2 & ~PFC_PWM_OVRDAT_MASK) | PFC_PWM_OVREN_MASK;
}

// Release the phase-B boost-switch pin; the synchronous rectifier pin stays forced low
inline static void PFC_PWM_PhaseBBoostEnable(PFC_PWM_POLARITY_e polarity)
{
    PG2IOCON2 &= ~(PFC_PWM_OVREN_MASK & ~PFC_PWM_SyncOvrenMask(polarity));
}

// Release the phase-B synchronous rectifier pin (after the boost pin is released)
inline static void PFC_PWM_PhaseBSyncEnable(PFC_PWM_POLARITY_e polarity)
{
    PG2IOCON2 &= ~PFC_PWM_SyncOvrenMask(polarity);
}

// Force phase-B outputs low
inline static void PFC_PWM_PhaseBDisable(void)
{
    PG2IOCON2 = (PG2IOCON2 & ~PFC_PWM_OVRDAT_MASK) | PFC_PWM_OVREN_MASK;
}

// Select which fast-leg switch PG1DC drives for the given polarity
inline static void PFC_PWM_PhaseASwapSet(PFC_PWM_POLARITY_e polarity)
{
    PG1IOCON1bits.SWAP = (polarity == PFC_PWM_POL_NEG) ? PFC_PWM_FAST_LEG_SWAP_NEG
                                                        : PFC_PWM_FAST_LEG_SWAP_POS;
}

// Select which fast-leg switch PG2DC drives for the given polarity
inline static void PFC_PWM_PhaseBSwapSet(PFC_PWM_POLARITY_e polarity)
{
    PG2IOCON1bits.SWAP = (polarity == PFC_PWM_POL_NEG) ? PFC_PWM_FAST_LEG_SWAP_NEG
                                                        : PFC_PWM_FAST_LEG_SWAP_POS;
}

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* PFC_PWM_H */
