/**
 * PINS Generated Driver Header File 
 * 
 * @file      pins.h
 *            
 * @defgroup  pinsdriver Pins Driver
 *            
 * @brief     The Pin Driver directs the operation and function of 
 *            the selected device pins using dsPIC MCUs.
 *
 * @skipline @version   PLIB Version 1.0.5
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

#ifndef PINS_H
#define PINS_H
// Section: Includes
#include <xc.h>

/**
 * @ingroup  pinsdriver
 * @brief    Locks all the Peripheral Remapping registers and cannot be written.
 * @return   none  
 */
#define PINS_PPSLock()           (RPCONbits.IOLOCK = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Unlocks all the Peripheral Remapping registers and can be written.
 * @return   none  
 */
#define PINS_PPSUnlock()         (RPCONbits.IOLOCK = 0)

// Section: Device Pin Macros
/**
 * @ingroup  pinsdriver
 * @brief    Sets the RA0 GPIO Pin which has a custom name of IO_RA0 to High
 * @pre      The RA0 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RA0_SetHigh()          (_LATA0 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RA0 GPIO Pin which has a custom name of IO_RA0 to Low
 * @pre      The RA0 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RA0_SetLow()           (_LATA0 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RA0 GPIO Pin which has a custom name of IO_RA0
 * @pre      The RA0 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RA0_Toggle()           (_LATA0 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RA0 GPIO Pin which has a custom name of IO_RA0
 * @param    none
 * @return   none  
 */
#define IO_RA0_GetValue()         _RA0

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RA0 GPIO Pin which has a custom name of IO_RA0 as Input
 * @param    none
 * @return   none  
 */
#define IO_RA0_SetDigitalInput()  (_TRISA0 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RA0 GPIO Pin which has a custom name of IO_RA0 as Output
 * @param    none
 * @return   none  
 */
#define IO_RA0_SetDigitalOutput() (_TRISA0 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RA3 GPIO Pin which has a custom name of IO_RA3 to High
 * @pre      The RA3 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RA3_SetHigh()          (_LATA3 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RA3 GPIO Pin which has a custom name of IO_RA3 to Low
 * @pre      The RA3 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RA3_SetLow()           (_LATA3 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RA3 GPIO Pin which has a custom name of IO_RA3
 * @pre      The RA3 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RA3_Toggle()           (_LATA3 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RA3 GPIO Pin which has a custom name of IO_RA3
 * @param    none
 * @return   none  
 */
#define IO_RA3_GetValue()         _RA3

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RA3 GPIO Pin which has a custom name of IO_RA3 as Input
 * @param    none
 * @return   none  
 */
#define IO_RA3_SetDigitalInput()  (_TRISA3 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RA3 GPIO Pin which has a custom name of IO_RA3 as Output
 * @param    none
 * @return   none  
 */
#define IO_RA3_SetDigitalOutput() (_TRISA3 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RA4 GPIO Pin which has a custom name of IO_RA4 to High
 * @pre      The RA4 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RA4_SetHigh()          (_LATA4 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RA4 GPIO Pin which has a custom name of IO_RA4 to Low
 * @pre      The RA4 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RA4_SetLow()           (_LATA4 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RA4 GPIO Pin which has a custom name of IO_RA4
 * @pre      The RA4 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RA4_Toggle()           (_LATA4 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RA4 GPIO Pin which has a custom name of IO_RA4
 * @param    none
 * @return   none  
 */
#define IO_RA4_GetValue()         _RA4

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RA4 GPIO Pin which has a custom name of IO_RA4 as Input
 * @param    none
 * @return   none  
 */
#define IO_RA4_SetDigitalInput()  (_TRISA4 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RA4 GPIO Pin which has a custom name of IO_RA4 as Output
 * @param    none
 * @return   none  
 */
#define IO_RA4_SetDigitalOutput() (_TRISA4 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RA7 GPIO Pin which has a custom name of IO_RA7 to High
 * @pre      The RA7 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RA7_SetHigh()          (_LATA7 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RA7 GPIO Pin which has a custom name of IO_RA7 to Low
 * @pre      The RA7 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RA7_SetLow()           (_LATA7 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RA7 GPIO Pin which has a custom name of IO_RA7
 * @pre      The RA7 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RA7_Toggle()           (_LATA7 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RA7 GPIO Pin which has a custom name of IO_RA7
 * @param    none
 * @return   none  
 */
#define IO_RA7_GetValue()         _RA7

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RA7 GPIO Pin which has a custom name of IO_RA7 as Input
 * @param    none
 * @return   none  
 */
#define IO_RA7_SetDigitalInput()  (_TRISA7 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RA7 GPIO Pin which has a custom name of IO_RA7 as Output
 * @param    none
 * @return   none  
 */
#define IO_RA7_SetDigitalOutput() (_TRISA7 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB0 GPIO Pin which has a custom name of IO_RB0 to High
 * @pre      The RB0 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RB0_SetHigh()          (_LATB0 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB0 GPIO Pin which has a custom name of IO_RB0 to Low
 * @pre      The RB0 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB0_SetLow()           (_LATB0 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RB0 GPIO Pin which has a custom name of IO_RB0
 * @pre      The RB0 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB0_Toggle()           (_LATB0 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RB0 GPIO Pin which has a custom name of IO_RB0
 * @param    none
 * @return   none  
 */
#define IO_RB0_GetValue()         _RB0

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB0 GPIO Pin which has a custom name of IO_RB0 as Input
 * @param    none
 * @return   none  
 */
#define IO_RB0_SetDigitalInput()  (_TRISB0 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB0 GPIO Pin which has a custom name of IO_RB0 as Output
 * @param    none
 * @return   none  
 */
#define IO_RB0_SetDigitalOutput() (_TRISB0 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB1 GPIO Pin which has a custom name of IO_RB1 to High
 * @pre      The RB1 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RB1_SetHigh()          (_LATB1 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB1 GPIO Pin which has a custom name of IO_RB1 to Low
 * @pre      The RB1 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB1_SetLow()           (_LATB1 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RB1 GPIO Pin which has a custom name of IO_RB1
 * @pre      The RB1 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB1_Toggle()           (_LATB1 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RB1 GPIO Pin which has a custom name of IO_RB1
 * @param    none
 * @return   none  
 */
#define IO_RB1_GetValue()         _RB1

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB1 GPIO Pin which has a custom name of IO_RB1 as Input
 * @param    none
 * @return   none  
 */
#define IO_RB1_SetDigitalInput()  (_TRISB1 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB1 GPIO Pin which has a custom name of IO_RB1 as Output
 * @param    none
 * @return   none  
 */
#define IO_RB1_SetDigitalOutput() (_TRISB1 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB2 GPIO Pin which has a custom name of IO_RB2 to High
 * @pre      The RB2 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RB2_SetHigh()          (_LATB2 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB2 GPIO Pin which has a custom name of IO_RB2 to Low
 * @pre      The RB2 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB2_SetLow()           (_LATB2 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RB2 GPIO Pin which has a custom name of IO_RB2
 * @pre      The RB2 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB2_Toggle()           (_LATB2 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RB2 GPIO Pin which has a custom name of IO_RB2
 * @param    none
 * @return   none  
 */
#define IO_RB2_GetValue()         _RB2

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB2 GPIO Pin which has a custom name of IO_RB2 as Input
 * @param    none
 * @return   none  
 */
#define IO_RB2_SetDigitalInput()  (_TRISB2 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB2 GPIO Pin which has a custom name of IO_RB2 as Output
 * @param    none
 * @return   none  
 */
#define IO_RB2_SetDigitalOutput() (_TRISB2 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB6 GPIO Pin which has a custom name of IO_RB6 to High
 * @pre      The RB6 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RB6_SetHigh()          (_LATB6 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB6 GPIO Pin which has a custom name of IO_RB6 to Low
 * @pre      The RB6 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB6_SetLow()           (_LATB6 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RB6 GPIO Pin which has a custom name of IO_RB6
 * @pre      The RB6 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB6_Toggle()           (_LATB6 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RB6 GPIO Pin which has a custom name of IO_RB6
 * @param    none
 * @return   none  
 */
#define IO_RB6_GetValue()         _RB6

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB6 GPIO Pin which has a custom name of IO_RB6 as Input
 * @param    none
 * @return   none  
 */
#define IO_RB6_SetDigitalInput()  (_TRISB6 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB6 GPIO Pin which has a custom name of IO_RB6 as Output
 * @param    none
 * @return   none  
 */
#define IO_RB6_SetDigitalOutput() (_TRISB6 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB7 GPIO Pin which has a custom name of IO_RB7 to High
 * @pre      The RB7 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RB7_SetHigh()          (_LATB7 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB7 GPIO Pin which has a custom name of IO_RB7 to Low
 * @pre      The RB7 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB7_SetLow()           (_LATB7 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RB7 GPIO Pin which has a custom name of IO_RB7
 * @pre      The RB7 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB7_Toggle()           (_LATB7 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RB7 GPIO Pin which has a custom name of IO_RB7
 * @param    none
 * @return   none  
 */
#define IO_RB7_GetValue()         _RB7

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB7 GPIO Pin which has a custom name of IO_RB7 as Input
 * @param    none
 * @return   none  
 */
#define IO_RB7_SetDigitalInput()  (_TRISB7 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB7 GPIO Pin which has a custom name of IO_RB7 as Output
 * @param    none
 * @return   none  
 */
#define IO_RB7_SetDigitalOutput() (_TRISB7 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB10 GPIO Pin which has a custom name of IO_RB10 to High
 * @pre      The RB10 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RB10_SetHigh()          (_LATB10 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB10 GPIO Pin which has a custom name of IO_RB10 to Low
 * @pre      The RB10 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB10_SetLow()           (_LATB10 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RB10 GPIO Pin which has a custom name of IO_RB10
 * @pre      The RB10 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB10_Toggle()           (_LATB10 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RB10 GPIO Pin which has a custom name of IO_RB10
 * @param    none
 * @return   none  
 */
#define IO_RB10_GetValue()         _RB10

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB10 GPIO Pin which has a custom name of IO_RB10 as Input
 * @param    none
 * @return   none  
 */
#define IO_RB10_SetDigitalInput()  (_TRISB10 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB10 GPIO Pin which has a custom name of IO_RB10 as Output
 * @param    none
 * @return   none  
 */
#define IO_RB10_SetDigitalOutput() (_TRISB10 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB11 GPIO Pin which has a custom name of IO_RB11 to High
 * @pre      The RB11 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RB11_SetHigh()          (_LATB11 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RB11 GPIO Pin which has a custom name of IO_RB11 to Low
 * @pre      The RB11 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB11_SetLow()           (_LATB11 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RB11 GPIO Pin which has a custom name of IO_RB11
 * @pre      The RB11 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RB11_Toggle()           (_LATB11 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RB11 GPIO Pin which has a custom name of IO_RB11
 * @param    none
 * @return   none  
 */
#define IO_RB11_GetValue()         _RB11

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB11 GPIO Pin which has a custom name of IO_RB11 as Input
 * @param    none
 * @return   none  
 */
#define IO_RB11_SetDigitalInput()  (_TRISB11 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RB11 GPIO Pin which has a custom name of IO_RB11 as Output
 * @param    none
 * @return   none  
 */
#define IO_RB11_SetDigitalOutput() (_TRISB11 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RD4 GPIO Pin which has a custom name of IO_RD4 to High
 * @pre      The RD4 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RD4_SetHigh()          (_LATD4 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RD4 GPIO Pin which has a custom name of IO_RD4 to Low
 * @pre      The RD4 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RD4_SetLow()           (_LATD4 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RD4 GPIO Pin which has a custom name of IO_RD4
 * @pre      The RD4 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RD4_Toggle()           (_LATD4 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RD4 GPIO Pin which has a custom name of IO_RD4
 * @param    none
 * @return   none  
 */
#define IO_RD4_GetValue()         _RD4

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RD4 GPIO Pin which has a custom name of IO_RD4 as Input
 * @param    none
 * @return   none  
 */
#define IO_RD4_SetDigitalInput()  (_TRISD4 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RD4 GPIO Pin which has a custom name of IO_RD4 as Output
 * @param    none
 * @return   none  
 */
#define IO_RD4_SetDigitalOutput() (_TRISD4 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RD7 GPIO Pin which has a custom name of IO_RD7 to High
 * @pre      The RD7 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RD7_SetHigh()          (_LATD7 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RD7 GPIO Pin which has a custom name of IO_RD7 to Low
 * @pre      The RD7 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RD7_SetLow()           (_LATD7 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RD7 GPIO Pin which has a custom name of IO_RD7
 * @pre      The RD7 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RD7_Toggle()           (_LATD7 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RD7 GPIO Pin which has a custom name of IO_RD7
 * @param    none
 * @return   none  
 */
#define IO_RD7_GetValue()         _RD7

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RD7 GPIO Pin which has a custom name of IO_RD7 as Input
 * @param    none
 * @return   none  
 */
#define IO_RD7_SetDigitalInput()  (_TRISD7 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RD7 GPIO Pin which has a custom name of IO_RD7 as Output
 * @param    none
 * @return   none  
 */
#define IO_RD7_SetDigitalOutput() (_TRISD7 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RD8 GPIO Pin which has a custom name of IO_RD8 to High
 * @pre      The RD8 must be set as Output Pin             
 * @param    none
 * @return   none  
 */
#define IO_RD8_SetHigh()          (_LATD8 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Sets the RD8 GPIO Pin which has a custom name of IO_RD8 to Low
 * @pre      The RD8 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RD8_SetLow()           (_LATD8 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Toggles the RD8 GPIO Pin which has a custom name of IO_RD8
 * @pre      The RD8 must be set as Output Pin
 * @param    none
 * @return   none  
 */
#define IO_RD8_Toggle()           (_LATD8 ^= 1)

/**
 * @ingroup  pinsdriver
 * @brief    Reads the value of the RD8 GPIO Pin which has a custom name of IO_RD8
 * @param    none
 * @return   none  
 */
#define IO_RD8_GetValue()         _RD8

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RD8 GPIO Pin which has a custom name of IO_RD8 as Input
 * @param    none
 * @return   none  
 */
#define IO_RD8_SetDigitalInput()  (_TRISD8 = 1)

/**
 * @ingroup  pinsdriver
 * @brief    Configures the RD8 GPIO Pin which has a custom name of IO_RD8 as Output
 * @param    none
 * @return   none  
 */
#define IO_RD8_SetDigitalOutput() (_TRISD8 = 0)

/**
 * @ingroup  pinsdriver
 * @brief    Initializes the PINS module
 * @param    none
 * @return   none  
 */
void PINS_Initialize(void);



#endif
