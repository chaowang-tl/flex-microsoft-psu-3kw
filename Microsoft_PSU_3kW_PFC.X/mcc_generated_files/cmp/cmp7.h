/**
 * CMP7 Generated Driver Header File 
 * 
 * @file      cmp7.h
 *            
 * @ingroup   cmpdriver
 *            
 * @brief     This is the generated driver header file for the CMP7 driver
 *
 * @skipline @version   PLIB Version 1.1.6
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

#ifndef CMP7_H
#define CMP7_H


#ifdef __cplusplus
extern "C" {
#endif

// Section: Included Files

#include <stddef.h>
#include <stdbool.h>
#include <xc.h>
#include "cmp_interface.h"

// Section: Data Type Definitions

/**
 @ingroup  cmpdriver
 @brief    Structure object of type CMP_INTERFACE with the 
           custom name given by the user in the Melody Driver User interface. 
           The default name e.g. CMP_DAC1 can be changed by the 
           user in the CMP user interface. 
           This allows defining a structure with application specific name 
           using the 'Custom Name' field. Application specific name allows the 
           API Portability.
*/
extern const struct CMP_INTERFACE CMP_DAC7;

/**
 * @ingroup  cmpdriver
 * @brief    This macro defines input clock for CMP7 
 */
#define CMP7_CLOCK_FREQUENCY 500000000UL

/**
 * @ingroup  cmpdriver
 * @brief    This macro defines the Custom Name for \ref CMP7_Initialize API
 */
#define CMP_DAC7_Initialize CMP7_Initialize
/**
 * @ingroup  cmpdriver
 * @brief    This macro defines the Custom Name for \ref CMP7_Deinitialize API
 */
#define CMP_DAC7_Deinitialize CMP7_Deinitialize
/**
 * @ingroup  cmpdriver
 * @brief    This macro defines the Custom Name for \ref CMP7_StatusGet API
 */
#define CMP_DAC7_StatusGet CMP7_StatusGet
/**
 * @ingroup  cmpdriver
 * @brief    This macro defines the Custom Name for \ref CMP7_Enable API
 */
#define CMP_DAC7_Enable CMP7_Enable
/**
 * @ingroup  cmpdriver
 * @brief    This macro defines the Custom Name for \ref CMP7_Disable API
 */
#define CMP_DAC7_Disable CMP7_Disable
/**
 * @ingroup  cmpdriver
 * @brief    This macro defines the Custom Name for \ref CMP7_DACEnable API
 */
#define CMP_DAC7_DACEnable CMP7_DACEnable
/**
 * @ingroup  cmpdriver
 * @brief    This macro defines the Custom Name for \ref CMP7_DACDisable API
 */
#define CMP_DAC7_DACDisable CMP7_DACDisable
/**
 * @ingroup  cmpdriver
 * @brief    This macro defines the Custom Name for \ref CMP7_DACDataWrite API
 */
#define CMP_DAC7_DACDataWrite CMP7_DACDataWrite
/**
 * @ingroup  cmpdriver
 * @brief    This macro defines the Custom Name for \ref CMP7_EventCallbackRegister API
 */
#define CMP_DAC7_EventCallbackRegister CMP7_EventCallbackRegister
/**
 * @ingroup  cmpdriver
 * @brief    This macro defines the Custom Name for \ref CMP7_Tasks API
 */
#define CMP_DAC7_Tasks CMP7_Tasks

// Section: CMP7 Module APIs

/**
 * @ingroup  cmpdriver
 * @brief    Initialize the CMP7 module
 * @param    none
 * @return   none  
 */

void CMP7_Initialize(void);

/**
 * @ingroup  cmpdriver
 * @brief    Deinitializes the CMP7 to POR values
 * @param    none
 * @return   none  
 */
void CMP7_Deinitialize(void);

/**
 * @ingroup  cmpdriver
 * @brief    Calibrates the CMP7 module and enables ripple reduction mode
 * @param    none
 * @return   none  
 */
void CMP7_Calibrate(void);
        
/**
 * @ingroup  cmpdriver
 * @brief    This inline function returns the comparator output status 
 * @param    none
 * @return   true   - Comparator output is high
 * @return   false  - Comparator output is low
 */
inline static bool CMP7_StatusGet(void)
{
    return (DAC7CMPbits.CMPSTAT);
}

/**
 * @ingroup  cmpdriver
 * @brief    This inline function enables the common DAC module
 * @param    none
 * @return   none 
 */
inline static void CMP7_Enable(void)
{
    DACCTRL1bits.ON = 1U;
}
    
/**
 * @ingroup  cmpdriver
 * @brief    This inline function disables the common DAC module
 * @param    none
 * @return   none  
 */
inline static void CMP7_Disable(void)
{
    DACCTRL1bits.ON = 0U;
}

/**
 * @ingroup  cmpdriver
 * @brief    This inline function enables the individual DAC module
 * @param    none
 * @return   none 
 */
inline static void CMP7_DACEnable(void)
{
    DAC7CONbits.DACEN = 1U;
}
    
/**
 * @ingroup  cmpdriver
 * @brief    This inline function disables the individual DAC module
 * @param    none
 * @return   none
 */
inline static void CMP7_DACDisable(void)
{
    DAC7CONbits.DACEN = 0U;
    //wait till update is complete, data registers are not writable until update is complete
    while(DAC7CONbits.UPDATE == 1U)
    {
    }
}

/**
 * @ingroup    cmpdriver
 * @brief      This inline function writes DAC data to register
 * @param[in]  value - DAC Data write value
 * @return     none 
 */
inline static void CMP7_DACDataWrite(size_t value)
{
    DAC7DATbits.DACDAT = value;
    //wait till update is complete, data registers are not writable until update is complete
    while(DAC7CONbits.UPDATE == 1U)
    {
    }
}

/**
 * @ingroup    cmpdriver
 * @brief      This function can be used to override default callback and to 
 *             define custom callback for CMP7 Event event
 * @param[in]  handler - Address of the callback function.  
 * @return     none  
 */
void CMP7_EventCallbackRegister(void (*handler)(void));

/**
 * @ingroup  cmpdriver
 * @brief    This is the default callback with weak attribute. 
 *           The user can override and implement the default callback without 
 *           weak attribute or can register a custom callback function using  
 *           CMP7_EventCallbackRegister.
 * @param    none
 * @return   none  
 */
void CMP7_EventCallback(void);

/**
 * @ingroup  cmpdriver
 * @brief    The Task function can be called in the main application using the High Speed
 *           Comparator, when interrupts are not used.  This would thus introduce the 
 *           polling mode feature of the Analog Comparator.
 * @param    none
 * @return   none  
 */
void CMP7_Tasks(void);

#ifdef __cplusplus
}
#endif

#endif //CMP7_H

/**
  End of File
*/

