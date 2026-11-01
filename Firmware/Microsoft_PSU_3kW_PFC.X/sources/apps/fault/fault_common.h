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
* File Name       : fault_common.h
* Description     : Generic fault detection objects (threshold compare, debounce)
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/22
* Modified by     : Chao Wang
* Modified on     : 2026/10/28
*******************************************************************************/

#ifndef FAULT_COMMON_H
#define FAULT_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Include
#include <xc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "fault.h"

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions

typedef enum {
    FLT_CMP_GREATER_THAN = 0,
    FLT_CMP_LESS_THAN
} FAULT_CMP_MODE_e;

typedef struct {
    volatile float  *pSource;
    float            threshold;
    float            hysteresis;
    FAULT_CMP_MODE_e cmpMode;
    FAULTLOG_FLAGS_e faultId;
    uint16_t         debounceSet;
    uint16_t         debounceClear;
    uint16_t         setCounter;
    uint16_t         clearCounter;
    bool             detected;
} FAULT_DETECT_T;

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef FAULT_COMMON_C
    #define FAULT_COMMON_C_PUBLIC
    #define FAULT_COMMON_C_CONST
#else
    #define FAULT_COMMON_C_PUBLIC extern
    #define FAULT_COMMON_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Function Prototypes

FAULT_COMMON_C_PUBLIC void FAULT_DETECT_Init(FAULT_DETECT_T *pDet,
                                              volatile float *pSource,
                                              float threshold,
                                              float hysteresis,
                                              FAULT_CMP_MODE_e cmpMode,
                                              FAULTLOG_FLAGS_e faultId,
                                              uint16_t debounceSet,
                                              uint16_t debounceClear);

FAULT_COMMON_C_PUBLIC void FAULT_DETECT_Evaluate(FAULT_DETECT_T *pDet);

FAULT_COMMON_C_PUBLIC void FAULT_DETECT_Reset(FAULT_DETECT_T *pDet);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* FAULT_COMMON_H */
