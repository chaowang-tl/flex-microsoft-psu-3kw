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
* File Name       : scheduler.c
* Description     : Cooperative task scheduler driven by 100us T1 tick
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/22
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/

#define SCHEDULER_C

//------------------------------------------------------------------------------
// Include
#include "scheduler.h"
#include "app_tasks.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions

//------------------------------------------------------------------------------
// Public Variables

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes

//------------------------------------------------------------------------------
// Private Variables
uint16_t schedLocalTick;
uint16_t schedCnt1ms;
uint16_t schedCnt10ms;
uint16_t schedCnt500ms;

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : SCHEDULER_Init
* Description     : Initialize scheduler counters with staggered offsets
* Purpose         : Ensure 1ms/10ms/500ms tasks fire at different ticks
* Input           : None
* Return          : None
* Calls           : None
* Called by       : main
* Note            : Call before enabling interrupts
*******************************************************************************/
void SCHEDULER_Init(void)
{
    schedLocalTick = 0;
    schedCnt1ms    = SCHED_INIT_CNT_1MS;
    schedCnt10ms   = SCHED_INIT_CNT_10MS;
    schedCnt500ms  = SCHED_INIT_CNT_500MS;
}

/*******************************************************************************
* Public Function : SCHEDULER_Run
* Description     : Poll for new T1 ticks and dispatch cooperative tasks
* Purpose         : Main-loop scheduler - processes one tick per call at most
* Input           : None
* Return          : None
* Calls           : APP_TASKS_Always, APP_TASKS_Task100us, APP_TASKS_Task1ms, APP_TASKS_Task10ms, APP_TASKS_Task500ms
* Called by       : main while(1)
* Note            : Non-preemptive; tasks must not block
*******************************************************************************/
void SCHEDULER_Run(void)
{
    APP_TASKS_Always();

    uint16_t isrTick = g_schedulerTickCount;

    if (isrTick == schedLocalTick)
    {
        return;
    }

    // One tick per call: a late main loop catches up over the next calls
    // instead of dropping ticks, so the 1 ms / 10 ms timers keep real time
    schedLocalTick++;

    APP_TASKS_Task100us();

    if (--schedCnt1ms == 0)
    {
        schedCnt1ms = SCHED_PERIOD_1MS;
        APP_TASKS_Task1ms();
    }

    if (--schedCnt10ms == 0)
    {
        schedCnt10ms = SCHED_PERIOD_10MS;
        APP_TASKS_Task10ms();
    }

    if (--schedCnt500ms == 0)
    {
        schedCnt500ms = SCHED_PERIOD_500MS;
        APP_TASKS_Task500ms();
    }
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation

/******************************************************************************/
#undef SCHEDULER_C
