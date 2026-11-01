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
* File Name       : smps_control.h
* Description     : PFC per-unit bases, measurements and float NPNZ controllers
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/18
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#ifndef SMPS_CONTROL_H
#define SMPS_CONTROL_H

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
#include "../pll/sogi_pll.h"
#include "../../config/board_config.h"

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
// Per-unit base values (1.0 PU = base value), from the 230 Vac / 3 kW / 400 V rating.
// Each phase current also uses IL_BASE, so at 230 Vac full load each phase peaks at 0.5 PU.
#define PFC_VAC_NOM_RMS_V       230.0f      // V rms, nominal grid voltage for the bases
#define PFC_P_NOM_W             3000.0f     // W, rated power
#define VAC_BASE                (PFC_VAC_NOM_RMS_V * 1.41421356f)                 // 325.27 V, grid peak at 230 Vac
#define VOUT_BASE               400.0f                                            // V, nominal bulk voltage
#define IL_BASE                 (PFC_P_NOM_W / PFC_VAC_NOM_RMS_V * 1.41421356f)   // 18.45 A, input current peak at 230 Vac / 3 kW
#define TEMP_BASE               25.0f       // degC, temperature base
#define FREQ_BASE               50.0f       // Hz, grid frequency base

// Loop rates
#define PFC_FAST_LOOP_HZ        ((float)BOARD_FAST_LOOP_HZ)  // Phase A / B ISR rate (board_config.h)
#define PFC_VLOOP_DIVIDER       BOARD_VLOOP_DIVIDER          // Voltage loop runs every N phase-B ISRs
#define PFC_VLOOP_HZ            (PFC_FAST_LOOP_HZ / (float)PFC_VLOOP_DIVIDER)  // 8.125 kHz

// Limit macros (type-agnostic)
#define UPDNLMT(Var, Max, Min)    do { if ((Var) > (Max)) (Var) = (Max); \
                                       if ((Var) < (Min)) (Var) = (Min); } while(0)
#define DOWNLMT(Var, Min)         do { if ((Var) < (Min)) (Var) = (Min); } while(0)

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions
// Floating-point NPNZ digital controller
typedef struct
{
    float* aCoefficients;                  // A coefficients (output history)
    float* bCoefficients;                  // B coefficients (error history)
    float* controlHistory;                 // N output samples, newest first
    float* errorHistory;                   // N+1 error samples, newest first
    volatile float* controlReference;      // Reference input
    volatile float* controlFeedback;       // Feedback input
    volatile float* controlOutput;         // Controller output
    float  maxOutput;                      // Output upper clamp
    float  minOutput;                      // Output lower clamp
} __attribute__((aligned)) SMPS_FLOAT_NPNZ_T;

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef SMPS_CONTROL_C
    #define SMPS_CONTROL_C_PUBLIC
    #define SMPS_CONTROL_C_CONST
#else
    #define SMPS_CONTROL_C_PUBLIC extern
    #define SMPS_CONTROL_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
// Controllers
SMPS_CONTROL_C_PUBLIC SMPS_FLOAT_NPNZ_T g_vLoop2p2z;  // Voltage loop, 8.125 kHz
SMPS_CONTROL_C_PUBLIC SMPS_FLOAT_NPNZ_T g_iaLoop2p2z; // Phase-A current loop, 65 kHz
SMPS_CONTROL_C_PUBLIC SMPS_FLOAT_NPNZ_T g_ibLoop2p2z; // Phase-B current loop, 65 kHz

// Grid PLL
SMPS_CONTROL_C_PUBLIC SOGI_PLL_T g_pll; // Updated in phase-A ISR

// Measurements (PU, written by ISR)
SMPS_CONTROL_C_PUBLIC volatile float g_pfcVacPU;       // AD2CH0-AD2CH1 (L-N), phase-A ISR
SMPS_CONTROL_C_PUBLIC volatile float g_pfcVoutPU;      // AD3CH0, phase-B ISR
SMPS_CONTROL_C_PUBLIC volatile float g_pfcVoutNotchPU; // Vout after the 2x line notch, voltage-loop feedback
SMPS_CONTROL_C_PUBLIC volatile float g_pfcIaUpPU;      // AD5CH0 phase-A high-side switch, phase-A ISR
SMPS_CONTROL_C_PUBLIC volatile float g_pfcIaDnPU;      // AD5CH1 phase-A low-side switch, phase-A ISR
SMPS_CONTROL_C_PUBLIC volatile float g_pfcIbUpPU;      // AD1CH0 phase-B high-side switch, phase-B ISR
SMPS_CONTROL_C_PUBLIC volatile float g_pfcIbDnPU;      // AD1CH1 phase-B low-side switch, phase-B ISR
SMPS_CONTROL_C_PUBLIC volatile float g_pfcIaPU;        // Phase-A inductor current magnitude
SMPS_CONTROL_C_PUBLIC volatile float g_pfcIbPU;        // Phase-B inductor current magnitude
SMPS_CONTROL_C_PUBLIC volatile float g_pfcIaFiltPU;    // Low-pass |iA| for slow OCP
SMPS_CONTROL_C_PUBLIC volatile float g_pfcIbFiltPU;    // Low-pass |iB| for slow OCP
SMPS_CONTROL_C_PUBLIC volatile float g_pfcVacPeakPU;   // PLL amplitude estimate, T1
SMPS_CONTROL_C_PUBLIC volatile float g_pfcFreqPU;      // PLL frequency / FREQ_BASE, T1
SMPS_CONTROL_C_PUBLIC volatile float g_pfcTempGanPU;   // AD3CH1, T1
SMPS_CONTROL_C_PUBLIC volatile float g_pfcTempInletPU; // AD4CH0, T1
SMPS_CONTROL_C_PUBLIC volatile float g_pfcTempLlcAPU;  // AD4CH1, T1
SMPS_CONTROL_C_PUBLIC volatile float g_pfcTempLlcBPU;  // AD4CH2, T1

// Control loop variables (PU)
SMPS_CONTROL_C_PUBLIC volatile float g_smpsPfcVrefPU;       // Ramped Vout reference, T1 -> phase-B ISR
SMPS_CONTROL_C_PUBLIC volatile float g_smpsPfcVrefTargetPU; // Vout reference target
SMPS_CONTROL_C_PUBLIC volatile float g_smpsPfcVctrlPU;      // Voltage loop output (power demand)
SMPS_CONTROL_C_PUBLIC volatile float g_smpsPfcIrefPU;       // Per-phase current reference
SMPS_CONTROL_C_PUBLIC volatile float g_smpsPfcIaCtrlPU;     // Phase-A current loop output
SMPS_CONTROL_C_PUBLIC volatile float g_smpsPfcIbCtrlPU;     // Phase-B current loop output

//------------------------------------------------------------------------------
// Public Function Prototypes
SMPS_CONTROL_C_PUBLIC void SMPS_FloatControllerInit(void);
SMPS_CONTROL_C_PUBLIC void SMPS_FloatController2P2ZUpdate(SMPS_FLOAT_NPNZ_T const * const ctrl); // ~370 ns at -O2
SMPS_CONTROL_C_PUBLIC void SMPS_FloatController2P2ZReset(SMPS_FLOAT_NPNZ_T const * const ctrl);
SMPS_CONTROL_C_PUBLIC void SMPS_FloatController2P2ZTrackOutput(SMPS_FLOAT_NPNZ_T const * const ctrl, float output);
SMPS_CONTROL_C_PUBLIC void SMPS_FloatController2P2ZErrorSeed(SMPS_FLOAT_NPNZ_T const * const ctrl);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* SMPS_CONTROL_H */
