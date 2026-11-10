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
* File Name       : pfc_pwm.c
* Description     : Totem-pole PFC PWM access layer (fast legs PG3/PG4, slow leg PG5)
*
*   PG3 = phase-A fast leg, PG4 = phase-B fast leg (180 deg, triggered by PG3),
*   PG5 = line-frequency slow leg, always driven through output override.
*   Fast legs use SWAP so PGxDC always represents the boost-switch duty.
*   Override changes on PG3/PG4/PG5 are synchronized to the local PWM time
*   base (OSYNC = 0b00, set in PwmInitExt), so no UPDREQ is needed.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/28
* Modified by     : Chao Wang
* Modified on     : 2026/11/10
*******************************************************************************/
#define PFC_PWM_C

//------------------------------------------------------------------------------
// Include
#include "pfc_pwm.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions

//------------------------------------------------------------------------------
// Public Variables
float g_pfcPwmPeriodCnt;

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes

//------------------------------------------------------------------------------
// Private Variables

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : PFC_PWM_Init
* Description     : Put all PFC legs into a safe state and cache the period
* Purpose         : Ensure PG3/PG4/PG5 outputs are overridden low before the
*                   control loop is allowed to run
* Input           : None
* Return          : None
* Calls           : PFC_PWM_AllOutputsOff
* Called by       : SMPS_SM_Init
* Note            : Call after PWM_Initialize (MCC) and before PWM_Enable
*******************************************************************************/
void PFC_PWM_Init(void)
{
    g_pfcPwmPeriodCnt = (float)PG3PER;

    // Duty point first, then duty (same order as DutyASet/BSet)
    PG3TRIGB = 1UL << 31;
    PG3DC    = 0UL;
    PG4TRIGB = 1UL << 31;
    PG4DC    = 0UL;
    PG1DC    = 0UL;                 // Sync window closed
    PG2DC    = 0UL;
    PFC_PWM_PhaseASyncGateOpen();   // Window open: LOGCON passes the switch pins
    PFC_PWM_PhaseBSyncGateOpen();
    PFC_PWM_AllOutputsOff();
}

/*******************************************************************************
* Public Function : PFC_PWM_AllOutputsOff
* Description     : Force every PFC switch off through output override
* Purpose         : Shutdown path for faults, stop and zero-crossing blanking
* Input           : None
* Return          : None
* Calls           : None
* Called by       : PFC_PWM_Init, PwmInitExt, PFC_LOOP_Reset, SmpsShutdown (T1), SmpsFastTrip (65 kHz ISRs)
* Note            : Safe from any context: a preempted read-modify-write can
*                   only leave the overrides set (outputs off)
*******************************************************************************/
void PFC_PWM_AllOutputsOff(void)
{
    PG3IOCON2 = (PG3IOCON2 & ~PFC_PWM_OVRDAT_MASK) | PFC_PWM_OVREN_MASK;
    PG4IOCON2 = (PG4IOCON2 & ~PFC_PWM_OVRDAT_MASK) | PFC_PWM_OVREN_MASK;
    PG5IOCON2 = (PG5IOCON2 & ~PFC_PWM_OVRDAT_MASK) | PFC_PWM_OVREN_MASK;
}

/*******************************************************************************
* Public Function : PFC_PWM_SlowLegSet
* Description     : Drive the line-frequency slow leg for the given polarity
* Purpose         : Turn on the slow-leg switch matching the grid half-cycle
* Input           : polarity - PFC_PWM_POL_BLANK / _POS / _NEG
* Return          : None
* Calls           : None
* Called by       : PFC_LOOP_PhaseAUpdate
* Note            : BLANK forces both switches off; polarity changes always
*                   pass through BLANK so the slow leg never shoots through
*******************************************************************************/
void PFC_PWM_SlowLegSet(PFC_PWM_POLARITY_e polarity)
{
    uint32_t ovrdat;

    switch (polarity)
    {
        case PFC_PWM_POL_POS:
            ovrdat = PFC_PWM_SLOW_LEG_POS_OVRDAT;
            break;

        case PFC_PWM_POL_NEG:
            ovrdat = PFC_PWM_SLOW_LEG_NEG_OVRDAT;
            break;

        default:
            ovrdat = 0U;
            break;
    }

    PG5IOCON2 = (PG5IOCON2 & ~PFC_PWM_OVRDAT_MASK)
              | (ovrdat << PFC_PWM_OVRDAT_POSITION)
              | PFC_PWM_OVREN_MASK;
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation

/******************************************************************************/
#undef PFC_PWM_C
