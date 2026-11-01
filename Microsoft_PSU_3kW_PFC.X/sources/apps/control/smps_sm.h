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
* File Name       : smps_sm.h
* Description     : PFC state machine interface
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/18
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#ifndef SMPS_SM_H
#define SMPS_SM_H

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
#include "smps_control.h"

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
// Vout reference limits [PU of VOUT_BASE]
#define SMPS_VOUT_REF_PU_DEFAULT          (400.0f / VOUT_BASE)
#define SMPS_VOUT_REF_MIN_PU              (350.0f / VOUT_BASE)
#define SMPS_VOUT_REF_MAX_PU              (410.0f / VOUT_BASE)

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions
typedef enum
{
    SMPS_STATE_INIT = 0,
    SMPS_STATE_STANDBY,
    SMPS_STATE_PREPARE,
    SMPS_STATE_SOFTSTART,
    SMPS_STATE_RUN,
    SMPS_STATE_STOPPING,
    SMPS_STATE_FAULT
} SMPS_STATE_e;

typedef struct
{
    SMPS_STATE_e state;
    bool enableCommand;
    uint16_t stateTimer;
    uint16_t runEntryCounter;
    uint32_t startupTimer;
} SMPS_SM_T;

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef SMPS_SM_C
    #define SMPS_SM_C_PUBLIC
    #define SMPS_SM_C_CONST
#else
    #define SMPS_SM_C_PUBLIC extern
    #define SMPS_SM_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
SMPS_SM_C_PUBLIC volatile SMPS_SM_T g_sm;      // T1 owns state; enableCommand also written from the main loop

//------------------------------------------------------------------------------
// Public Function Prototypes
SMPS_SM_C_PUBLIC void SMPS_SM_Init(void);
SMPS_SM_C_PUBLIC void SMPS_SM_TaskPhaseA(void);
SMPS_SM_C_PUBLIC void SMPS_SM_TaskPhaseB(void);
SMPS_SM_C_PUBLIC void SMPS_SM_TaskSlow(void);
SMPS_SM_C_PUBLIC void SMPS_SM_Enable(bool enable);
SMPS_SM_C_PUBLIC void SMPS_SM_SetVoutRef(float refPU);
SMPS_SM_C_PUBLIC void SMPS_SM_RequestFaultClear(void);
SMPS_SM_C_PUBLIC bool SMPS_SM_HealthConsume(void);
SMPS_SM_C_PUBLIC bool SMPS_SM_IsPowerGood(void);

// Snapshot of the state machine state (single 32-bit read)
inline static SMPS_STATE_e SMPS_SM_GetState(void)
{
    return g_sm.state;
}

// True while the converter enable command is set
inline static bool SMPS_SM_IsEnabled(void)
{
    return g_sm.enableCommand;
}

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* SMPS_SM_H */
