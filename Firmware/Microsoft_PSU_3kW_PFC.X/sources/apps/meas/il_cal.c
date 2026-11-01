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
* File Name       : il_cal.c
* Description     : Inductor-current CT zero-offset calibration
*
*   At power-up, with PWM off (iL = 0), each ADC ISR passes the raw counts
*   of its two CTs to IL_CAL_Update. After IL_CAL_SAMPLES samples the
*   averages become the zero-current offsets, unless one is outside
*   IL_CAL_OFFSET_DEFAULT +- IL_CAL_WINDOW (then the default is kept and
*   ok stays false). Startup requires done and both phase results ok.
*
*   Split out of user_isr so that the state machine and the ISR file no
*   longer depend on each other: user_isr reads the ADC and feeds this
*   module, smps_sm only asks whether it is done.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/11/01
* Modified by     :
* Modified on     :
*******************************************************************************/
#define IL_CAL_C

//------------------------------------------------------------------------------
// Include
#include <stdlib.h>
#include <xc.h>
#include "il_cal.h"
#include "../control/smps_sm.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions

//------------------------------------------------------------------------------
// Public Variables
// Defaults until the calibration runs: mid-scale offsets, done = true
IL_CAL_T g_ilCalA = { 0U, 0U, 0U, IL_CAL_OFFSET_DEFAULT, IL_CAL_OFFSET_DEFAULT, false, true, false };
IL_CAL_T g_ilCalB = { 0U, 0U, 0U, IL_CAL_OFFSET_DEFAULT, IL_CAL_OFFSET_DEFAULT, false, true, false };

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes
static void IlCalReset(IL_CAL_T *cal);
//------------------------------------------------------------------------------
// Private Variables

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : IL_CAL_Start
* Description     : Start the CT zero-offset calibration of both phases
* Purpose         : Measure the real zero-current counts of the four CTs
* Input           : None
* Return          : None
* Calls           : IlCalReset
* Called by       : main; stopped-state maintenance
* Note            : Sampling runs in the ADC ISRs, so it only starts once
*                   interrupts are on. PWM must be off for the whole run.
*                   Running/armed states are rejected. IRQ state is preserved.
*******************************************************************************/
void IL_CAL_Start(void)
{
    uint32_t irqState = __builtin_get_isr_state();
    __builtin_disable_interrupts();
    SMPS_STATE_e state = SMPS_SM_GetState();
    if (state == SMPS_STATE_INIT
        || ((state == SMPS_STATE_STANDBY || state == SMPS_STATE_FAULT)
            && !SMPS_SM_IsEnabled()))
    {
        IlCalReset(&g_ilCalA);
        IlCalReset(&g_ilCalB);
    }
    __builtin_set_isr_state(irqState);
}

/*******************************************************************************
* Public Function : IL_CAL_Update
* Description     : Accumulate one sample pair and finish after N samples
* Purpose         : Average the raw counts to get each CT's zero-current offset
* Input           : cal   - calibration state of the phase (g_ilCalA / g_ilCalB)
*                   rawUp - raw high-side CT counts
*                   rawDn - raw low-side CT counts
* Return          : None
* Calls           : labs
* Called by       : _AD5CH1Interrupt, _AD1CH1Interrupt (while cal->active)
* Note            : An average outside the window is rejected: the default
*                   offset is kept and ok stays false
*******************************************************************************/
void IL_CAL_Update(IL_CAL_T *cal, uint32_t rawUp, uint32_t rawDn)
{
    int32_t offUp;
    int32_t offDn;

    cal->sumUp += rawUp;
    cal->sumDn += rawDn;

    if (++cal->count < IL_CAL_SAMPLES)
    {
        return;
    }

    offUp = (int32_t)((cal->sumUp + (IL_CAL_SAMPLES >> 1U)) >> IL_CAL_SHIFT);
    offDn = (int32_t)((cal->sumDn + (IL_CAL_SAMPLES >> 1U)) >> IL_CAL_SHIFT);

    if ((labs(offUp - IL_CAL_OFFSET_DEFAULT) <= IL_CAL_WINDOW)
        && (labs(offDn - IL_CAL_OFFSET_DEFAULT) <= IL_CAL_WINDOW))
    {
        cal->offsetUp = offUp;
        cal->offsetDn = offDn;
        cal->ok       = true;
    }

    cal->active = false;
    cal->done   = true;
}

/*******************************************************************************
* Public Function : IL_CAL_Done
* Description     : Report whether both phases finished calibrating
* Purpose         : Let the state machine hold INIT until offsets are valid
* Input           : None
* Return          : true when phase A and phase B are done
* Calls           : None
* Called by       : SMPS_SM_TaskSlow
* Note            : Also true when a result was rejected (default offset kept);
*                   g_ilCalA.ok / g_ilCalB.ok show whether it was accepted
*******************************************************************************/
bool IL_CAL_Done(void)
{
    return g_ilCalA.done && g_ilCalB.done;
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation
/*******************************************************************************
* Private Function : IlCalReset
* Description     : Restart one phase's zero-offset calibration
* Purpose         : Clear the accumulators and fall back to the default offset
* Input           : cal - calibration state of the phase
* Return          : None
* Calls           : None
* Called by       : IL_CAL_Start
* Note            : active is set last, after the accumulators are cleared
*******************************************************************************/
static void IlCalReset(IL_CAL_T *cal)
{
    cal->active   = false;
    cal->sumUp    = 0U;
    cal->sumDn    = 0U;
    cal->count    = 0U;
    cal->offsetUp = IL_CAL_OFFSET_DEFAULT;
    cal->offsetDn = IL_CAL_OFFSET_DEFAULT;
    cal->done     = false;
    cal->ok       = false;
    cal->active   = true;
}

/******************************************************************************/
#undef IL_CAL_C
