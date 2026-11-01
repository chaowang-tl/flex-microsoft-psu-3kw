/**
 * CMP4 Generated Driver Source File
 * 
 * @file      cmp4.c
 *            
 * @ingroup   cmpdriver
 *            
 * @brief     This is the generated driver source file for CMP4 driver
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

// Section: Included Files

#include <xc.h>
#include "../cmp4.h"

#ifndef CMP_FPDMDAC_ADDRESS
#define CMP_FPDMDAC_ADDRESS 0x7F20B0UL
#endif //CMP_FPDMDAC_ADDRESS

// Section: File specific functions

static void (*CMP4_EventHandler)(void) = NULL;

// Section: Driver Interface
static const struct DAC_DC_INTERFACE dac4_dc_interface = {
    .Enable = &CMP4_DACEnable,
    .Disable = &CMP4_DACDisable,
    .DataWrite = &CMP4_DACDataWrite,
};

const struct CMP_INTERFACE CMP_DAC4 = {
    .Initialize = &CMP4_Initialize,
    .Deinitialize = &CMP4_Deinitialize,
    .Enable = &CMP4_Enable,
    .Disable = &CMP4_Disable,
    .StatusGet = &CMP4_StatusGet,
    .Calibrate = &CMP4_Calibrate,
    
    .EventCallbackRegister = &CMP4_EventCallbackRegister,
    .Tasks = &CMP4_Tasks,
    .cmp_dac_dc_interface = &dac4_dc_interface,
    .cmp_dac_slope_interface = NULL,
};

// Section: CMP4 Module APIs

void CMP4_Initialize(void)
{           
    // Comparator Register settings
    DACCTRL1 = 0x3F7F0000UL; //FCLKDIV 1:1; DNLADJ 0x0; SIDL disabled; ON disabled; NEGINLADJ 127; POSINLADJ 63; RREN disabled; 
    DACCTRL2 = 0x8A0001UL; //TMODTIME 1; SSTIME 138; 
    DAC4CON = 0x800CUL; //DACOEN disabled; UPDTMDIS disabled; IRQM Interrupts are disabled; DACEN enabled; UPDREQ disabled; UPDTRG Any write(s) to DACDAT or SLPDAT sets the UPDATE bit immediately; TMCB 0; 
    DAC4CMP = 0x60018UL; //INNSEL DACx; INPSEL CMP4D; CMPSTAT disabled; FLTREN disabled; CBE disabled; HYSSEL 30 mv hysteresis; HYSPOL Falling Edge; 

    //Slope Settings
    DAC4DAT = 0xF3200CDUL; //DACLOW 205; DACDAT 3890; 
    DAC4SLPCON = 0x0UL; //SLPSTRT None; SLPSTOPB None; SLPSTOPA None; HCFSEL None; FFSEN disabled; PSE Negative; TWME disabled; HME disabled; SLOPEN disabled; 
    DAC4SLPDAT = 0x0UL; //SLPDAT 0; 
    
    CMP4_EventCallbackRegister(&CMP4_EventCallback);
    
    DACCTRL1bits.ON = 1U;
    //wait till update is complete, data registers are not writable until update is complete
    while(DAC4CONbits.UPDATE == 1U)
    {
    }
}

void CMP4_Deinitialize(void)
{ 
    DACCTRL1bits.ON = 0;
    
    
    // Comparator Register settings
    DACCTRL1 = 0x3F7F0000UL;
    DACCTRL2 = 0x8A0001UL;
    DAC4CON = 0x0UL;
    DAC4CMP = 0x0UL;

    //Slope Settings
    DAC4DAT = 0x0UL;
    DAC4SLPCON = 0x0UL;
    DAC4SLPDAT = 0x0UL;
}

void CMP4_Calibrate(void)
{
    uint32_t *fpdmdac = (uint32_t*)CMP_FPDMDAC_ADDRESS;
    DACCTRL1bits.POSINLADJ = ((*fpdmdac) & _DACCTRL1_POSINLADJ_MASK) >> _DACCTRL1_POSINLADJ_POSITION;
    DACCTRL1bits.NEGINLADJ = ((*fpdmdac) & _DACCTRL1_NEGINLADJ_MASK) >> _DACCTRL1_NEGINLADJ_POSITION;
    DACCTRL1bits.DNLADJ = ((*fpdmdac) & _DACCTRL1_DNLADJ_MASK) >> _DACCTRL1_DNLADJ_POSITION;
    DACCTRL1bits.RREN = 1U;
}

void CMP4_EventCallbackRegister(void (*handler)(void))
{
    if(NULL != handler)
    {
        CMP4_EventHandler = handler;
    }
}

void __attribute__ ((weak)) CMP4_EventCallback(void)
{ 
   
}
 
void CMP4_Tasks(void)
{
    if(IFS2bits.CMP4IF == 1U)
    {
        // CMP4 callback function 
        if(NULL != CMP4_EventHandler)
        {
            (*CMP4_EventHandler)();
        }
    
        // clear the CMP4 interrupt flag
        IFS2bits.CMP4IF = 0U;
    }
}

/**
 End of File
*/
