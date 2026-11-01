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
* File Name       : main.c
* Description     : Main function
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/27
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/

//------------------------------------------------------------------------------
// Include
#include "mcc_generated_files/system/system.h"
#include "mcc_generated_files/timer/tmr1.h"
#include "mcc_generated_files/system/watchdog.h"
#include "mcc_generated_files/system/interrupt.h"
#include "sources/drivers/mcc_extended/mcc_ext.h"
#include "sources/scheduler/scheduler.h"
#include "sources/isr/user_isr.h"
#include "sources/apps/control/smps_sm.h"
#include "sources/apps/meas/il_cal.h"
#include "sources/apps/comm/llc_link.h"
#include "sources/drivers/llc_signal/llc_signal.h"
#include "sources/third_party/X2Cscope/X2Cscope.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions

//------------------------------------------------------------------------------
// Private Variables

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : main
* Description     : Application entry point
* Purpose         : Initialize system and run main loop
* Input           : None
* Return          : int
* Calls           : SYSTEM_Initialize, MCC_EXT_SystemInitialize,
*                   USER_ISR_AdcScaleInit, SMPS_SM_Init, LLC_SIGNAL_Init,
*                   LLC_LINK_Init, IL_CAL_Start, SCHEDULER_Init, TMR1_Start,
*                   X2Cscope_Init, WATCHDOG_TimerSoftwareEnable,
*                   INTERRUPT_GlobalEnable, SCHEDULER_Run
* Called by       : Startup code
* Note            : The PWM generators are started at the end of
*                   MCC_EXT_SystemInitialize with all PFC outputs forced low;
*                   early interrupts acknowledge flags only until g_userIsrReady is set. Every module
*                   used by an ISR is initialised before that point
*******************************************************************************/
int main(void)
{
    SYSTEM_Initialize();
    MCC_EXT_SystemInitialize();

    USER_ISR_AdcScaleInit();
    SMPS_SM_Init();             // Converter: PWM safe state, loops, PLL, faults
    LLC_SIGNAL_Init();          // RC4 bus-voltage PWM, RD6 load capture
    LLC_LINK_Init();            // UART1 link to the LLC
#if !APP_PLL_PWM_TEST_ENABLE
    IL_CAL_Start();             // Physical CT calibration only with real sensing
#endif

    SCHEDULER_Init();
    // Enable the lower-priority periodic service task after the fast path is ready
    TMR1_Start();               // 10 kHz housekeeping ISR, priority 1

    // Bring up the debug/monitoring interface used by X2Cscope
    X2Cscope_Init();

    WATCHDOG_TimerSoftwareEnable();

    g_userIsrReady = true;
    INTERRUPT_GlobalEnable();

    while(1)
    {
        SCHEDULER_Run();
    }
}

/******************************************************************************/
