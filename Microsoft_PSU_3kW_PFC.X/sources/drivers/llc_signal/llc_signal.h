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
* File Name       : llc_signal.h
* Description     : Duty-coded discrete signals to/from the LLC (RC4 out, RD6 in)
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/31
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#ifndef LLC_SIGNAL_H
#define LLC_SIGNAL_H

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

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef LLC_SIGNAL_C
    #define LLC_SIGNAL_C_PUBLIC
    #define LLC_SIGNAL_C_CONST
#else
    #define LLC_SIGNAL_C_PUBLIC extern
    #define LLC_SIGNAL_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
LLC_SIGNAL_C_PUBLIC volatile float    g_llcLoadPowerW;   // RD6 decoded LLC load [W], 0 when invalid, phase-B ISR
LLC_SIGNAL_C_PUBLIC volatile bool     g_llcLoadValid;    // RD6 duty in range and edges present, phase-B ISR
LLC_SIGNAL_C_PUBLIC volatile float    g_llcLoadDuty;     // RD6 filtered duty (0..1), phase-B ISR
LLC_SIGNAL_C_PUBLIC volatile float    g_llcLoadFreqHz;   // Measured SCCP2 rising-edge frequency; 0 on timeout
LLC_SIGNAL_C_PUBLIC volatile uint32_t g_llcLoadBadCnt;   // RD6 range/frequency/overflow/backlog errors
LLC_SIGNAL_C_PUBLIC volatile float    g_llcVbusDuty;     // RC4 duty being sent (0..1), APP_TASKS_Task1ms

//------------------------------------------------------------------------------
// Public Function Prototypes
LLC_SIGNAL_C_PUBLIC void LLC_SIGNAL_Init(void);
LLC_SIGNAL_C_PUBLIC void LLC_SIGNAL_LoadUpdate(void);
LLC_SIGNAL_C_PUBLIC void LLC_SIGNAL_VbusUpdate(float vbusV);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* LLC_SIGNAL_H */
