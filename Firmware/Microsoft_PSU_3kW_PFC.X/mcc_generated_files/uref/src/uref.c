/**
 * UREF Generated Driver Source File
 * 
 * @file      uref.c
 * 
 * @ingroup   urefdriver
 * 
 * @brief     This is the generated driver source file for UREF driver
 *
 * @skipline @version   PLIB Version 1.1.0
 *
 * @skipline  Device : dsPIC33AK256MPS206
*/

/*
? [2026] Microchip Technology Inc. and its subsidiaries.

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

// Section: Included Files

#include <stdlib.h>
#include "../uref.h"

// Section: File specific functions
// Section: Driver Interface Function Definitions

void UREF_Initialize (void)
{
    // INSEL DAC7; ON enabled; UREFOUTEN enabled; DACOUT1EN disabled; DACOUT2EN disabled; 
    UREFCON = 0x8C01UL;
}

void UREF_Deinitialize (void)
{
    UREF_Disable();
    
    UREFCON = 0x0UL;
}

/**
 End of File
*/
