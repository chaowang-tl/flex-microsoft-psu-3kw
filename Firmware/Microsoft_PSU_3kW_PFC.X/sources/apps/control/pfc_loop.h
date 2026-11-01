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
* File Name       : pfc_loop.h
* Description     : Interleaved totem-pole PFC fast control path
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/28
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#ifndef PFC_LOOP_H
#define PFC_LOOP_H

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
#include "../../drivers/pfc_pwm/pfc_pwm.h"

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
#define PFC_IL_FILT_ALPHA       0.0921f     // ~1 kHz LPF at the 65 kHz sampling rate

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef PFC_LOOP_C
    #define PFC_LOOP_C_PUBLIC
    #define PFC_LOOP_C_CONST
#else
    #define PFC_LOOP_C_PUBLIC extern
    #define PFC_LOOP_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
PFC_LOOP_C_PUBLIC volatile PFC_PWM_POLARITY_e g_pfcPolarity; // Set by phase-A ISR, followed by phase B
PFC_LOOP_C_PUBLIC volatile float g_pfcDutyAPU;               // Last phase-A duty (0..1)
PFC_LOOP_C_PUBLIC volatile float g_pfcDutyBPU;               // Last phase-B duty (0..1)
PFC_LOOP_C_PUBLIC volatile uint32_t g_pfcZcRejectCnt;        // BLANK exits blocked by PLL sign check
PFC_LOOP_C_PUBLIC volatile uint32_t g_pfcZcMismatchCnt;      // Sign flips outside the low band
PFC_LOOP_C_PUBLIC volatile float g_pfcLoadFfPU;              // LLC load feed-forward [PU power], voltage-loop rate
PFC_LOOP_C_PUBLIC volatile float g_pfcPowerDemandPU;         // PI + feed-forward, clamped [PU power]

//------------------------------------------------------------------------------
// Public Function Prototypes
PFC_LOOP_C_PUBLIC void PFC_LOOP_Init(void);
PFC_LOOP_C_PUBLIC void PFC_LOOP_Reset(void);
PFC_LOOP_C_PUBLIC void PFC_LOOP_GridUpdate(void);
PFC_LOOP_C_PUBLIC void PFC_LOOP_PhaseAUpdate(void);
PFC_LOOP_C_PUBLIC void PFC_LOOP_PhaseBUpdate(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* PFC_LOOP_H */
