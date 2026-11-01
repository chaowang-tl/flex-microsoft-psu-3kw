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
* File Name       : grid_meas.h
* Description     : Grid voltage and input current RMS (PU and physical units)
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/28
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#ifndef GRID_MEAS_H
#define GRID_MEAS_H

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
#include "../control/smps_control.h"

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
// Forced latch if no PLL cycle boundary is seen (> 1 cycle at 44 Hz / 65 kHz = 1477)
#define GRID_MEAS_N_MAX         2048U

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef GRID_MEAS_C
    #define GRID_MEAS_C_PUBLIC
    #define GRID_MEAS_C_CONST
#else
    #define GRID_MEAS_C_PUBLIC extern
    #define GRID_MEAS_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
GRID_MEAS_C_PUBLIC volatile float g_pfcVacRmsPU;        // Grid voltage RMS [PU of VAC_BASE], T1
GRID_MEAS_C_PUBLIC volatile float g_pfcIinRmsPU;        // Input current RMS, |iA|+|iB| [PU of IL_BASE], T1
GRID_MEAS_C_PUBLIC volatile float g_pfcVacRms;          // Grid voltage RMS [V], T1
GRID_MEAS_C_PUBLIC volatile float g_pfcIinRms;          // Input current RMS [A], T1
GRID_MEAS_C_PUBLIC volatile bool     g_xcapDischarging; // RA4 X-cap discharge active, T1
GRID_MEAS_C_PUBLIC volatile uint32_t g_xcapEvents;      // Discharge pulses started
GRID_MEAS_C_PUBLIC volatile uint16_t g_xcapNoCrossMs;   // Time since the last zero crossing [ms]

//------------------------------------------------------------------------------
// Public Function Prototypes
GRID_MEAS_C_PUBLIC void GRID_MEAS_Init(void);
GRID_MEAS_C_PUBLIC void GRID_MEAS_SampleUpdate(void);
GRID_MEAS_C_PUBLIC void GRID_MEAS_TaskSlow(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* GRID_MEAS_H */
