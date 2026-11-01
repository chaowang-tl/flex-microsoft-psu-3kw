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
* File Name       : smps_control.c
* Description     : PFC per-unit measurements and float NPNZ controllers
*
*   All loops use the LLC-side SMPS_FLOAT_NPNZ_T library. Placeholders are
*   PI controllers written in 2P2Z form (backward-Euler integrator):
*       u[n] = u[n-1] + (Kp + Ki*Ts)*e[n] - Kp*e[n-1]
*       A1 = 1, A2 = 0, B0 = Kp + Ki*Ts, B1 = -Kp, B2 = 0
*   Replace with designed 2P2Z coefficients (fP0/fP1/fZ1) once the plant
*   is measured. Clamped output is stored in history (anti-windup).
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/18
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#define SMPS_CONTROL_C

//------------------------------------------------------------------------------
// Include
#include "smps_control.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions
// Power stage and loop design inputs (PI gains below are derived from these)
#define PFC_L_H                     BOARD_PFC_L_H       // H, boost inductor per phase (board_config.h)
#define PFC_COUT_F                  BOARD_PFC_COUT_F    // F, bulk capacitance (board_config.h)
#define PFC_VOUT_NOM_V              VOUT_BASE   // V, nominal bulk voltage
#define I_LOOP_FC_HZ                4000.0f     // current-loop crossover
#define V_LOOP_FC_HZ                20.0f       // voltage-loop crossover; 2x line ripple removed by the notch
#define PFC_TWO_PI                  6.28318530f

// Plant gains in PU, both plants are integrators G/s:
//   current: diL_pu/dt  = Vout / (L * IL_BASE) * dD
//   voltage: dVout_pu/dt = (0.5 * VAC_BASE * IL_BASE) / (C * Vout * VOUT_BASE) * dvctrl
//            (vctrl = 1 PU is the rated 3 kW at any grid voltage)
#define I_LOOP_PLANT_G              (PFC_VOUT_NOM_V / (PFC_L_H * IL_BASE))
#define V_LOOP_PLANT_G              ((0.5f * VAC_BASE * IL_BASE) / (PFC_COUT_F * PFC_VOUT_NOM_V * VOUT_BASE))

// Voltage loop: fs = 65 kHz / 8 = 8.125 kHz, Vout error [PU] -> power demand [PU]
// PI by zero placement: Kp = wc / G, zero at fc/5 (Kp 6.702, Ki 168.4)
#define V_LOOP_TS                   ((float)PFC_VLOOP_DIVIDER / PFC_FAST_LOOP_HZ)
#define V_LOOP_KP                   (PFC_TWO_PI * V_LOOP_FC_HZ / V_LOOP_PLANT_G)
#define V_LOOP_KI                   (V_LOOP_KP * PFC_TWO_PI * V_LOOP_FC_HZ / 5.0f)
#define V_COMP_FLOAT_2P2Z_COEFF_A1  1.0f
#define V_COMP_FLOAT_2P2Z_COEFF_A2  0.0f
#define V_COMP_FLOAT_2P2Z_COEFF_B0  (V_LOOP_KP + V_LOOP_KI * V_LOOP_TS)
#define V_COMP_FLOAT_2P2Z_COEFF_B1  (-V_LOOP_KP)
#define V_COMP_FLOAT_2P2Z_COEFF_B2  0.0f
#define V_COMP_FLOAT_2P2Z_MAX_CLAMP 1.2f        // 3.6 kW
#define V_COMP_FLOAT_2P2Z_MIN_CLAMP (-1.2f)

// Current loops: fs = 65 kHz, current error [PU] -> duty correction
// PI by zero placement: Kp = wc / G, zero at fc/10 (Kp 0.348, Ki 873.9)
#define I_LOOP_TS                   (1.0f / PFC_FAST_LOOP_HZ)
#define I_LOOP_KP                   (PFC_TWO_PI * I_LOOP_FC_HZ / I_LOOP_PLANT_G)
#define I_LOOP_KI                   (I_LOOP_KP * PFC_TWO_PI * I_LOOP_FC_HZ / 10.0f)
#define I_COMP_FLOAT_2P2Z_COEFF_A1  1.0f
#define I_COMP_FLOAT_2P2Z_COEFF_A2  0.0f
#define I_COMP_FLOAT_2P2Z_COEFF_B0  (I_LOOP_KP + I_LOOP_KI * I_LOOP_TS)
#define I_COMP_FLOAT_2P2Z_COEFF_B1  (-I_LOOP_KP)
#define I_COMP_FLOAT_2P2Z_COEFF_B2  0.0f
#define I_COMP_FLOAT_2P2Z_MAX_CLAMP 0.5f
#define I_COMP_FLOAT_2P2Z_MIN_CLAMP (-0.5f)

//------------------------------------------------------------------------------
// Public Variables
SMPS_FLOAT_NPNZ_T g_vLoop2p2z;
SMPS_FLOAT_NPNZ_T g_iaLoop2p2z;
SMPS_FLOAT_NPNZ_T g_ibLoop2p2z;

SOGI_PLL_T g_pll;

volatile float g_pfcVacPU;
volatile float g_pfcVoutPU;
volatile float g_pfcVoutNotchPU;
volatile float g_pfcIaUpPU;
volatile float g_pfcIaDnPU;
volatile float g_pfcIbUpPU;
volatile float g_pfcIbDnPU;
volatile float g_pfcIaPU;
volatile float g_pfcIbPU;
volatile float g_pfcIaFiltPU;
volatile float g_pfcIbFiltPU;
volatile float g_pfcVacPeakPU;
volatile float g_pfcFreqPU = 1.0f;
volatile float g_pfcTempGanPU;
volatile float g_pfcTempInletPU;
volatile float g_pfcTempLlcAPU;
volatile float g_pfcTempLlcBPU;

volatile float g_smpsPfcVrefPU;
volatile float g_smpsPfcVrefTargetPU;
volatile float g_smpsPfcVctrlPU;
volatile float g_smpsPfcIrefPU;
volatile float g_smpsPfcIaCtrlPU;
volatile float g_smpsPfcIbCtrlPU;

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes
static void Controller2P2ZSetup(SMPS_FLOAT_NPNZ_T *ctrl, float *aCoef, float *bCoef,
                         float *ctrlHist, float *errHist,
                         float a1, float a2, float b0, float b1, float b2,
                         volatile float *ref, volatile float *fb, volatile float *out,
                         float maxOut, float minOut);
//------------------------------------------------------------------------------
// Private Variables
float __attribute__((near, aligned)) vFloatComp2p2zACoefficients[2];
float __attribute__((near, aligned)) vFloatComp2p2zBCoefficients[3];
float __attribute__((near, aligned)) vFloatComp2p2zControlHistory[2];
float __attribute__((near, aligned)) vFloatComp2p2zErrorHistory[3];

float __attribute__((near, aligned)) iaFloatComp2p2zACoefficients[2];
float __attribute__((near, aligned)) iaFloatComp2p2zBCoefficients[3];
float __attribute__((near, aligned)) iaFloatComp2p2zControlHistory[2];
float __attribute__((near, aligned)) iaFloatComp2p2zErrorHistory[3];

float __attribute__((near, aligned)) ibFloatComp2p2zACoefficients[2];
float __attribute__((near, aligned)) ibFloatComp2p2zBCoefficients[3];
float __attribute__((near, aligned)) ibFloatComp2p2zControlHistory[2];
float __attribute__((near, aligned)) ibFloatComp2p2zErrorHistory[3];

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : SMPS_FloatControllerInit
* Description     : Initialize the voltage loop and both current loops
* Purpose         : Load coefficients, bind PU inputs/outputs, clear history
* Input           : None
* Return          : None
* Calls           : Controller2P2ZSetup
* Called by       : SMPS_SM_Init
* Note            : Must be called before the ADC ISRs are enabled
*******************************************************************************/
void SMPS_FloatControllerInit(void)
{
    Controller2P2ZSetup(&g_vLoop2p2z,
                        vFloatComp2p2zACoefficients, vFloatComp2p2zBCoefficients,
                        vFloatComp2p2zControlHistory, vFloatComp2p2zErrorHistory,
                        V_COMP_FLOAT_2P2Z_COEFF_A1, V_COMP_FLOAT_2P2Z_COEFF_A2,
                        V_COMP_FLOAT_2P2Z_COEFF_B0, V_COMP_FLOAT_2P2Z_COEFF_B1,
                        V_COMP_FLOAT_2P2Z_COEFF_B2,
                        &g_smpsPfcVrefPU, &g_pfcVoutNotchPU, &g_smpsPfcVctrlPU,
                        V_COMP_FLOAT_2P2Z_MAX_CLAMP, V_COMP_FLOAT_2P2Z_MIN_CLAMP);

    Controller2P2ZSetup(&g_iaLoop2p2z,
                        iaFloatComp2p2zACoefficients, iaFloatComp2p2zBCoefficients,
                        iaFloatComp2p2zControlHistory, iaFloatComp2p2zErrorHistory,
                        I_COMP_FLOAT_2P2Z_COEFF_A1, I_COMP_FLOAT_2P2Z_COEFF_A2,
                        I_COMP_FLOAT_2P2Z_COEFF_B0, I_COMP_FLOAT_2P2Z_COEFF_B1,
                        I_COMP_FLOAT_2P2Z_COEFF_B2,
                        &g_smpsPfcIrefPU, &g_pfcIaPU, &g_smpsPfcIaCtrlPU,
                        I_COMP_FLOAT_2P2Z_MAX_CLAMP, I_COMP_FLOAT_2P2Z_MIN_CLAMP);

    Controller2P2ZSetup(&g_ibLoop2p2z,
                        ibFloatComp2p2zACoefficients, ibFloatComp2p2zBCoefficients,
                        ibFloatComp2p2zControlHistory, ibFloatComp2p2zErrorHistory,
                        I_COMP_FLOAT_2P2Z_COEFF_A1, I_COMP_FLOAT_2P2Z_COEFF_A2,
                        I_COMP_FLOAT_2P2Z_COEFF_B0, I_COMP_FLOAT_2P2Z_COEFF_B1,
                        I_COMP_FLOAT_2P2Z_COEFF_B2,
                        &g_smpsPfcIrefPU, &g_pfcIbPU, &g_smpsPfcIbCtrlPU,
                        I_COMP_FLOAT_2P2Z_MAX_CLAMP, I_COMP_FLOAT_2P2Z_MIN_CLAMP);
}

/*******************************************************************************
* Public Function : SMPS_FloatController2P2ZUpdate
* Description     : Update 2-pole 2-zero floating-point controller
* Purpose         : Execute one control cycle
* Input           : ctrl - Pointer to controller data structure
* Return          : None
* Calls           : UPDNLMT
* Called by       : PfcVoltageLoop, PfcDutyCompute (pfc_loop.c)
* Note            : Execution time ~370 ns (-O2)
*******************************************************************************/
void SMPS_FloatController2P2ZUpdate(SMPS_FLOAT_NPNZ_T const * const ctrl)
{
    ctrl->errorHistory[2] = ctrl->errorHistory[1];
    ctrl->errorHistory[1] = ctrl->errorHistory[0];
    ctrl->errorHistory[0] = *ctrl->controlReference - *ctrl->controlFeedback;

    *ctrl->controlOutput = ctrl->bCoefficients[0] * ctrl->errorHistory[0]
                         + ctrl->bCoefficients[1] * ctrl->errorHistory[1]
                         + ctrl->bCoefficients[2] * ctrl->errorHistory[2]
                         + ctrl->aCoefficients[0] * ctrl->controlHistory[0]
                         + ctrl->aCoefficients[1] * ctrl->controlHistory[1];
    UPDNLMT(*ctrl->controlOutput, ctrl->maxOutput, ctrl->minOutput);

    ctrl->controlHistory[1] = ctrl->controlHistory[0];
    ctrl->controlHistory[0] = *ctrl->controlOutput;
}

/*******************************************************************************
* Public Function : SMPS_FloatController2P2ZReset
* Description     : Reset 2-pole 2-zero floating-point controller
* Purpose         : Clear history buffers and output
* Input           : ctrl - Pointer to controller data structure
* Return          : None
* Calls           : None
* Called by       : PFC_LOOP_Reset
* Note            : None
*******************************************************************************/
void SMPS_FloatController2P2ZReset(SMPS_FLOAT_NPNZ_T const * const ctrl)
{
    ctrl->errorHistory[2] = 0.0f;
    ctrl->errorHistory[1] = 0.0f;
    ctrl->errorHistory[0] = 0.0f;

    ctrl->controlHistory[1] = 0.0f;
    ctrl->controlHistory[0] = 0.0f;

    *ctrl->controlOutput = 0.0f;
}

/*******************************************************************************
* Public Function : SMPS_FloatController2P2ZErrorSeed
* Description     : Fill the error history with the present error
* Purpose         : Bumpless resume of a frozen controller
* Input           : ctrl - Pointer to controller data structure
* Return          : None
* Calls           : None
* Called by       : PfcDutyCompute (soft-start to closed-loop hand-over)
* Note            : Control history and output are kept. Removes the
*                   proportional kick Kp*(e[n]-e[n-1]) caused by a stale e[n-1]
*******************************************************************************/
void SMPS_FloatController2P2ZErrorSeed(SMPS_FLOAT_NPNZ_T const * const ctrl)
{
    float error = *ctrl->controlReference - *ctrl->controlFeedback;

    ctrl->errorHistory[2] = error;
    ctrl->errorHistory[1] = error;
    ctrl->errorHistory[0] = error;
}

/*******************************************************************************
* Public Function : SMPS_FloatController2P2ZTrackOutput
* Description     : Track the applied output after external saturation
* Purpose         : Track the applied output after external saturation
* Input           : ctrl - controller; output - applied PI contribution
* Return          : None
* Calls           : UPDNLMT
* Called by       : PfcVoltageLoop, PfcDutyCompute
* Note            : User application code; preserves MCC ownership
*******************************************************************************/
void SMPS_FloatController2P2ZTrackOutput(SMPS_FLOAT_NPNZ_T const * const ctrl, float output)
{
    UPDNLMT(output, ctrl->maxOutput, ctrl->minOutput);
    *ctrl->controlOutput = output;
    ctrl->controlHistory[0] = output;
    ctrl->controlHistory[1] = output;
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation
/*******************************************************************************
* Private Function : Controller2P2ZSetup
* Description     : Bind buffers, coefficients and PU signals to a 2P2Z object
* Purpose         : Remove repeated setup code for the three PFC loops
* Input           : ctrl, buffers, coefficients, I/O pointers, clamps
* Return          : None
* Calls           : SMPS_FloatController2P2ZReset
* Called by       : SMPS_FloatControllerInit
* Note            : None
*******************************************************************************/
static void Controller2P2ZSetup(SMPS_FLOAT_NPNZ_T *ctrl, float *aCoef, float *bCoef,
                         float *ctrlHist, float *errHist,
                         float a1, float a2, float b0, float b1, float b2,
                         volatile float *ref, volatile float *fb, volatile float *out,
                         float maxOut, float minOut)
{
    aCoef[0] = a1;
    aCoef[1] = a2;
    bCoef[0] = b0;
    bCoef[1] = b1;
    bCoef[2] = b2;

    ctrl->aCoefficients    = aCoef;
    ctrl->bCoefficients    = bCoef;
    ctrl->controlHistory   = ctrlHist;
    ctrl->errorHistory     = errHist;
    ctrl->controlReference = ref;
    ctrl->controlFeedback  = fb;
    ctrl->controlOutput    = out;
    ctrl->maxOutput        = maxOut;
    ctrl->minOutput        = minOut;

    SMPS_FloatController2P2ZReset(ctrl);
}

/******************************************************************************/
#undef SMPS_CONTROL_C
