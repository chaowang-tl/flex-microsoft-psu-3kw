/**
 * UREF Generated Driver Header File
 * 
 * @file      uref.h
 * 
 * @defgroup  urefdriver UREF Driver
 * 
 * @brief     This is the generated driver header file for the UREF driver
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

#ifndef UREF_H
#define UREF_H


#ifdef __cplusplus
extern "C" {
#endif

// Section: Included Files

#include <xc.h>
#include <stdint.h>
// Section: Data Type Definitions

// Section: Interface Routines

/**
 * @ingroup  urefdriver
 * @brief    Initializes the UREF module
 * @param    none
 * @return   none  
 */
void UREF_Initialize(void);

/**
 * @ingroup  urefdriver
 * @brief    Deinitializes the UREF to POR values
 * @param    none
 * @return   none  
 */
void UREF_Deinitialize(void);

/**
 * @ingroup  urefdriver
 * @brief    This inline function enables UREF module
 * @pre      The UREF_Initialize function should be called.
 * @param    none
 * @return   none  
 */
inline static void UREF_Enable( void )
{
    UREFCONbits.ON = 1U; //Enable uref;
}

/**
 * @ingroup  urefdriver
 * @brief    This inline function disables UREF module
 * @param    none
 * @return   none  
 */
inline static void UREF_Disable( void )
{
    UREFCONbits.ON = 0U; //Disable uref;
}


#ifdef __cplusplus
}
#endif

#endif //UREF_H

/**
 End of File
*/




