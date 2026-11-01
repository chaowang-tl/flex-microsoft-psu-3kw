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
* File Name       : scheduler.h
* Description     : Cooperative task scheduler driven by 100us T1 tick
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/22
* Modified by     : Chao Wang
* Modified on     : 2026/10/27
*******************************************************************************/

#ifndef SCHEDULER_H
#define SCHEDULER_H

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
#include "../config/board_config.h"

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
#define SCHED_TICK_US               BOARD_SLOW_TICK_US          // T1 period, 100 us

#define SCHED_PERIOD_1MS            (1000U  / SCHED_TICK_US)    // 10
#define SCHED_PERIOD_10MS           (10000U / SCHED_TICK_US)    // 100
#define SCHED_PERIOD_500MS          (500000UL / SCHED_TICK_US)  // 5000

#define SCHED_INIT_CNT_1MS          SCHED_PERIOD_1MS            // first fire at tick 10
#define SCHED_INIT_CNT_10MS         (SCHED_PERIOD_10MS - 3U)    // first fire at tick 3
#define SCHED_INIT_CNT_500MS        (SCHED_PERIOD_500MS - 7U)   // first fire at tick 7

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef SCHEDULER_C
    #define SCHEDULER_C_PUBLIC
    #define SCHEDULER_C_CONST
#else
    #define SCHEDULER_C_PUBLIC extern
    #define SCHEDULER_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
SCHEDULER_C_PUBLIC volatile uint16_t g_schedulerTickCount;

//------------------------------------------------------------------------------
// Public Function Prototypes
SCHEDULER_C_PUBLIC void SCHEDULER_Init(void);
SCHEDULER_C_PUBLIC void SCHEDULER_Run(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
