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
* File Name       : il_cal.h
* Description     : Inductor-current CT zero-offset calibration
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/11/01
* Modified by     :
* Modified on     :
*******************************************************************************/
#ifndef IL_CAL_H
#define IL_CAL_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Include
#include <stdint.h>
#include <stdbool.h>

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
#define IL_CAL_OFFSET_DEFAULT   2048        // counts, mid-scale of the 12-bit ADC
#define IL_CAL_SHIFT            12U         // 4096 samples per channel, ~63 ms at 65 kHz
#define IL_CAL_SAMPLES          (1UL << IL_CAL_SHIFT)
#define IL_CAL_WINDOW           256         // counts, max accepted deviation from the default

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions
// Zero-current offset calibration of one phase (high-/low-side CT)
typedef struct
{
    uint32_t sumUp;             // Accumulated raw counts, high-side CT
    uint32_t sumDn;             // Accumulated raw counts, low-side CT
    uint32_t count;             // Samples accumulated so far
    int32_t  offsetUp;          // Zero-current offset, high-side CT [counts]
    int32_t  offsetDn;          // Zero-current offset, low-side CT [counts]
    volatile bool active;       // Accumulating
    volatile bool done;         // Finished (accepted or rejected)
    volatile bool ok;           // Both offsets inside the accepted window
} IL_CAL_T;

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef IL_CAL_C
    #define IL_CAL_C_PUBLIC
    #define IL_CAL_C_CONST
#else
    #define IL_CAL_C_PUBLIC extern
    #define IL_CAL_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
IL_CAL_C_PUBLIC IL_CAL_T g_ilCalA; // Phase A (AD5CH0 / AD5CH1), phase-A ISR
IL_CAL_C_PUBLIC IL_CAL_T g_ilCalB; // Phase B (AD1CH0 / AD1CH1), phase-B ISR

//------------------------------------------------------------------------------
// Public Function Prototypes
IL_CAL_C_PUBLIC void IL_CAL_Start(void);
IL_CAL_C_PUBLIC void IL_CAL_Update(IL_CAL_T *cal, uint32_t rawUp, uint32_t rawDn);
IL_CAL_C_PUBLIC bool IL_CAL_Done(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* IL_CAL_H */
