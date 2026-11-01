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
* File Name       : fault_common.c
* Description     : Generic fault detection objects (threshold compare, debounce)
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/22
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/

#define FAULT_COMMON_C

//------------------------------------------------------------------------------
// Include
#include "fault_common.h"

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
* Public Function : FAULT_DETECT_Init
* Description     : Configure a fault detection object
* Purpose         : Bind source, threshold, compare mode, and debounce counts
* Input           : pDet         - pointer to detection object
*                   pSource      - pointer to measured value (float)
*                   threshold    - trip threshold
*                   hysteresis   - clear offset from threshold (deadband)
*                   cmpMode      - GREATER_THAN or LESS_THAN
*                   faultId      - index into FAULTLOG_FLAGS_e
*                   debounceSet  - consecutive trips before FAULT_Set (0 = instant)
*                   debounceClear- consecutive clears before FAULT_Clear (0 = instant)
* Return          : None
* Calls           : None
* Called by       : FAULT_Init
* Note            : None
*******************************************************************************/
void FAULT_DETECT_Init(FAULT_DETECT_T *pDet,
                        volatile float *pSource,
                        float threshold,
                        float hysteresis,
                        FAULT_CMP_MODE_e cmpMode,
                        FAULTLOG_FLAGS_e faultId,
                        uint16_t debounceSet,
                        uint16_t debounceClear)
{
    pDet->pSource       = pSource;
    pDet->threshold     = threshold;
    pDet->hysteresis    = hysteresis;
    pDet->cmpMode       = cmpMode;
    pDet->faultId       = faultId;
    pDet->debounceSet   = debounceSet;
    pDet->debounceClear = debounceClear;
    pDet->setCounter    = 0;
    pDet->clearCounter  = 0;
    pDet->detected      = false;
}

/*******************************************************************************
* Public Function : FAULT_DETECT_Evaluate
* Description     : Compare source against threshold and manage set/clear
* Purpose         : Call periodically; handles debounce and calls FAULT_Set/Clear
* Input           : pDet - pointer to detection object
* Return          : None
* Calls           : FAULT_Set, FAULT_Clear
* Called by       : FAULT_Evaluate (T1)
* Note            : None
*******************************************************************************/
void FAULT_DETECT_Evaluate(FAULT_DETECT_T *pDet)
{
    bool tripped;
    bool cleared;
    float value;

    if (pDet->pSource == NULL)
    {
        return;
    }

    value = *(pDet->pSource);

    if (pDet->cmpMode == FLT_CMP_GREATER_THAN)
    {
        tripped = (value >= pDet->threshold);
        cleared = (value < (pDet->threshold - pDet->hysteresis));
    }
    else
    {
        tripped = (value <= pDet->threshold);
        cleared = (value > (pDet->threshold + pDet->hysteresis));
    }

    if (!isfinite(value)) { tripped = true; cleared = false; }

    if (tripped)
    {
        pDet->clearCounter = 0;

        if (!pDet->detected)
        {
            pDet->setCounter++;
            if (pDet->setCounter >= pDet->debounceSet)
            {
                pDet->detected = true;
                pDet->setCounter = 0;
                FAULT_Set(pDet->faultId);
            }
        }
    }
    else if (cleared)
    {
        pDet->setCounter = 0;

        if (pDet->detected)
        {
            pDet->clearCounter++;
            if (pDet->clearCounter >= pDet->debounceClear)
            {
                pDet->detected = false;
                pDet->clearCounter = 0;
                FAULT_Clear(pDet->faultId);
            }
        }
    }
    else
    {
        pDet->setCounter = 0;
        pDet->clearCounter = 0;
    }
}

/*******************************************************************************
* Public Function : FAULT_DETECT_Reset
* Description     : Reset detection object counters and state
* Purpose         : Clear debounce state without changing configuration
* Input           : pDet - pointer to detection object
* Return          : None
* Calls           : None
* Called by       : FAULT_ClearAll, FAULT_Evaluate
* Note            : Does not call FAULT_Clear; only resets local state
*******************************************************************************/
void FAULT_DETECT_Reset(FAULT_DETECT_T *pDet)
{
    pDet->setCounter   = 0;
    pDet->clearCounter = 0;
    pDet->detected     = false;
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation

/******************************************************************************/
#undef FAULT_COMMON_C
