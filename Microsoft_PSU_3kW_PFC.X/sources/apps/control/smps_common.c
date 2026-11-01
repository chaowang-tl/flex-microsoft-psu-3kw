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
* File Name       : smps_common.c
* Description     : Common SMPS utility functions implementation
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/18
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#define SMPS_COMMON_C

//------------------------------------------------------------------------------
// Include
#include "smps_common.h"

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
* Public Function : SMPS_FloatSoftStartRampToTarget
* Description     : Bidirectional soft-start ramp for floating-point reference
* Purpose         : Smoothly ramp reference value to target with configurable step
* Input           : currentValue - Pointer to current floating-point value
*                   targetValue  - Desired target floating-point value
*                   rampStep     - Ramp step size per call (must be positive)
* Return          : true = target reached, false = still ramping
* Calls           : None
* Called by       : SMPS_SM_TaskSlow
* Note            : Handles ramp-up and ramp-down, clamps to prevent overshoot
*******************************************************************************/
bool SMPS_FloatSoftStartRampToTarget(volatile float* const currentValue, float targetValue, float rampStep)
{
    if (*currentValue < targetValue)
    {
        *currentValue += rampStep;
        if (*currentValue >= targetValue)
        {
            *currentValue = targetValue;
        }
    }
    else if (*currentValue > targetValue)
    {
        *currentValue -= rampStep;
        if (*currentValue <= targetValue)
        {
            *currentValue = targetValue;
        }
    }
    return !(*currentValue < targetValue) && !(*currentValue > targetValue);
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation

/******************************************************************************/
#undef SMPS_COMMON_C
