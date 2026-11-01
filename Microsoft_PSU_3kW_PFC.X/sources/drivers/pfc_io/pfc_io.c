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
* File Name       : pfc_io.c
* Description     : PFC discrete outputs (inrush relay, X-cap bleeder, unused pins)
*
*   The only module that drives these GPIOs:
*     RB2  inrush-limiter bypass relay, high = closed
*     RA4  X-capacitor bleeder,         high = discharge
*     RA0, RB6, RB7, RB10, RB11 unused, driven low
*   PFC_IO_Init forces level and direction itself, so an MCC regeneration
*   with other TRIS/LAT defaults cannot leave a safety output floating.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/11/01
* Modified by     :
* Modified on     :
*******************************************************************************/
#define PFC_IO_C

//------------------------------------------------------------------------------
// Include
#include "pfc_io.h"
#include "../../../mcc_generated_files/system/pins.h"

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
* Public Function : PFC_IO_Init
* Description     : Put every PFC discrete output in its safe state
* Purpose         : Relay open, bleeder off, unused pins low, all as outputs
* Input           : None
* Return          : None
* Calls           : IO_Rxx_SetLow, IO_Rxx_SetDigitalOutput (MCC pins.h)
* Called by       : MCC_EXT_SystemInitialize (PinsInitExt)
* Note            : The latch is written before the direction, so no pin
*                   glitches high when it turns into an output
*******************************************************************************/
void PFC_IO_Init(void)
{
    IO_RB2_SetLow();                // Inrush bypass relay open
    IO_RB2_SetDigitalOutput();
    IO_RA4_SetLow();                // X-cap bleeder off
    IO_RA4_SetDigitalOutput();

    IO_RA0_SetLow();
    IO_RA0_SetDigitalOutput();
    IO_RB6_SetLow();
    IO_RB6_SetDigitalOutput();
    IO_RB7_SetLow();
    IO_RB7_SetDigitalOutput();
    IO_RB10_SetLow();
    IO_RB10_SetDigitalOutput();
    IO_RB11_SetLow();
    IO_RB11_SetDigitalOutput();
}

/*******************************************************************************
* Public Function : PFC_IO_RelaySet
* Description     : Drive the inrush-limiter bypass relay (RB2)
* Purpose         : Bypass the limiter after pre-charge, open it on shutdown
* Input           : close - true = relay closed (RB2 high)
* Return          : None
* Calls           : IO_RB2_SetHigh, IO_RB2_SetLow
* Called by       : SmpsRelaySet (smps_sm.c, T1)
* Note            : Single-bit LAT write, safe from any context
*******************************************************************************/
void PFC_IO_RelaySet(bool close)
{
    if (close)
    {
        IO_RB2_SetHigh();
    }
    else
    {
        IO_RB2_SetLow();
    }
}

/*******************************************************************************
* Public Function : PFC_IO_XcapSet
* Description     : Drive the X-capacitor bleeder (RA4)
* Purpose         : Discharge the inlet capacitor after AC removal
* Input           : on - true = bleeder on (RA4 high)
* Return          : None
* Calls           : IO_RA4_SetHigh, IO_RA4_SetLow
* Called by       : GridXcapSet (grid_meas.c, T1)
* Note            : Single-bit LAT write, safe from any context
*******************************************************************************/
void PFC_IO_XcapSet(bool on)
{
    if (on)
    {
        IO_RA4_SetHigh();
    }
    else
    {
        IO_RA4_SetLow();
    }
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation

/******************************************************************************/
#undef PFC_IO_C
