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
* File Name       : app_tasks.c
* Description     : Project-specific task implementations for the scheduler
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/22
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/

#define APP_TASKS_C

//------------------------------------------------------------------------------
// Include
#include "app_tasks.h"
#include "../../mcc_generated_files/system/watchdog.h"
#include "../third_party/X2Cscope/X2Cscope.h"
#include "../apps/comm/llc_link.h"
#include "../apps/control/smps_control.h"
#include "../apps/control/smps_sm.h"
#include "../drivers/llc_signal/llc_signal.h"

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

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : APP_TASKS_Always
* Description     : Runs every main-loop iteration, not gated by tick
* Purpose         : Background communication and always-on services
* Input           : None
* Return          : None
* Calls           : SMPS_SM_HealthConsume, WATCHDOG_TimerClear,
*                   X2Cscope_Communicate, LLC_LINK_Task
* Called by       : SCHEDULER_Run
* Note            : The watchdog is cleared only when T1 confirmed both ADC ISRs
*                   ran since the last clear (at most once per 100 us), so a
*                   stall in any context resets the MCU.
*                   LLC_LINK_Task answers LLC requests well within the 2 ms
*                   response-start budget of the protocol
*******************************************************************************/
void APP_TASKS_Always(void)
{
    if (SMPS_SM_HealthConsume())
    {
        WATCHDOG_TimerClear();
    }
    X2Cscope_Communicate();
    LLC_LINK_Task();
}

/*******************************************************************************
* Public Function : APP_TASKS_Task100us
* Description     : Runs every 100us tick in main-loop context
* Purpose         : Low-priority periodic task
* Input           : None
* Return          : None
* Calls           : None
* Called by       : SCHEDULER_Run
* Note            : None
*******************************************************************************/
void APP_TASKS_Task100us(void)
{

}

/*******************************************************************************
* Public Function : APP_TASKS_Task1ms
* Description     : Runs every 1ms in main-loop context
* Purpose         : LLC link timers and the RC4 bus-voltage signal
* Input           : None
* Return          : None
* Calls           : LLC_LINK_Tick1ms, LLC_SIGNAL_VbusUpdate
* Called by       : SCHEDULER_Run
* Note            : None
*******************************************************************************/
void APP_TASKS_Task1ms(void)
{
    LLC_LINK_Tick1ms();
    LLC_SIGNAL_VbusUpdate(g_pfcVoutPU * VOUT_BASE);
}

/*******************************************************************************
* Public Function : APP_TASKS_Task10ms
* Description     : Runs every 10ms in main-loop context
* Purpose         : Low-priority periodic task
* Input           : None
* Return          : None
* Calls           : None
* Called by       : SCHEDULER_Run
* Note            : None
*******************************************************************************/
void APP_TASKS_Task10ms(void)
{

}

/*******************************************************************************
* Public Function : APP_TASKS_Task500ms
* Description     : Runs every 500ms in main-loop context
* Purpose         : Low-priority periodic task
* Input           : None
* Return          : None
* Calls           : None
* Called by       : SCHEDULER_Run
* Note            : None
*******************************************************************************/
void APP_TASKS_Task500ms(void)
{

}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation

/******************************************************************************/
#undef APP_TASKS_C
