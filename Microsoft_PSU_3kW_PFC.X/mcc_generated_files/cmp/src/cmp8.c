/**
 * CMP8 Generated Driver Source File
 * 
 * @file      cmp8.c
 *            
 * @ingroup   cmpdriver
 *            
 * @brief     This is the generated driver source file for CMP8 driver
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
#include "../cmp8.h"

#ifndef CMP_FPDMDAC_ADDRESS
#define CMP_FPDMDAC_ADDRESS 0x7F20B0UL
#endif //CMP_FPDMDAC_ADDRESS

// Section: File specific functions

static void (*CMP8_EventHandler)(void) = NULL;

// Section: Driver Interface
static const struct DAC_DC_INTERFACE dac8_dc_interface = {
    .Enable = &CMP8_DACEnable,
    .Disable = &CMP8_DACDisable,
    .DataWrite = &CMP8_DACDataWrite,
};

const struct CMP_INTERFACE CMP_DAC8 = {
    .Initialize = &CMP8_Initialize,
    .Deinitialize = &CMP8_Deinitialize,
    .Enable = &CMP8_Enable,
    .Disable = &CMP8_Disable,
    .StatusGet = &CMP8_StatusGet,
    .Calibrate = &CMP8_Calibrate,
    
    .EventCallbackRegister = &CMP8_EventCallbackRegister,
    .Tasks = &CMP8_Tasks,
    .cmp_dac_dc_interface = &dac8_dc_interface,
    .cmp_dac_slope_interface = NULL,
};

// Section: CMP8 Module APIs

void CMP8_Initialize(void)
{           
    // Comparator Register settings
    DACCTRL1 = 0x3F7F0000UL; //FCLKDIV 1:1; DNLADJ 0x0; SIDL disabled; ON disabled; NEGINLADJ 127; POSINLADJ 63; RREN disabled; 
    DACCTRL2 = 0x8A0001UL; //TMODTIME 1; SSTIME 138; 
    DAC8CON = 0x810CUL; //DACOEN enabled; UPDTMDIS disabled; IRQM Interrupts are disabled; DACEN enabled; UPDREQ disabled; UPDTRG Any write(s) to DACDAT or SLPDAT sets the UPDATE bit immediately; TMCB 0; 
    DAC8CMP = 0x20UL; //INNSEL DACx; INPSEL Bandgap 0.8V; CMPSTAT disabled; FLTREN disabled; CBE disabled; HYSSEL No hysteresis is selected; HYSPOL Rising Edge; 

    //Slope Settings
    DAC8DAT = 0xF3200CDUL; //DACLOW 205; DACDAT 3890; 
    DAC8SLPCON = 0x0UL; //SLPSTRT None; SLPSTOPB None; SLPSTOPA None; HCFSEL None; FFSEN disabled; PSE Negative; TWME disabled; HME disabled; SLOPEN disabled; 
    DAC8SLPDAT = 0x0UL; //SLPDAT 0; 
    
    CMP8_EventCallbackRegister(&CMP8_EventCallback);
    
    DACCTRL1bits.ON = 1U;
    //wait till update is complete, data registers are not writable until update is complete
    while(DAC8CONbits.UPDATE == 1U)
    {
    }
}

void CMP8_Deinitialize(void)
{ 
    DACCTRL1bits.ON = 0;
    
    
    // Comparator Register settings
    DACCTRL1 = 0x3F7F0000UL;
    DACCTRL2 = 0x8A0001UL;
    DAC8CON = 0x0UL;
    DAC8CMP = 0x0UL;

    //Slope Settings
    DAC8DAT = 0x0UL;
    DAC8SLPCON = 0x0UL;
    DAC8SLPDAT = 0x0UL;
}

void CMP8_Calibrate(void)
{
    uint32_t *fpdmdac = (uint32_t*)CMP_FPDMDAC_ADDRESS;
    DACCTRL1bits.POSINLADJ = ((*fpdmdac) & _DACCTRL1_POSINLADJ_MASK) >> _DACCTRL1_POSINLADJ_POSITION;
    DACCTRL1bits.NEGINLADJ = ((*fpdmdac) & _DACCTRL1_NEGINLADJ_MASK) >> _DACCTRL1_NEGINLADJ_POSITION;
    DACCTRL1bits.DNLADJ = ((*fpdmdac) & _DACCTRL1_DNLADJ_MASK) >> _DACCTRL1_DNLADJ_POSITION;
    DACCTRL1bits.RREN = 1U;
}

void CMP8_EventCallbackRegister(void (*handler)(void))
{
    if(NULL != handler)
    {
        CMP8_EventHandler = handler;
    }
}

void __attribute__ ((weak)) CMP8_EventCallback(void)
{ 
   
}
 
void CMP8_Tasks(void)
{
    if(IFS8bits.CMP8IF == 1U)
    {
        // CMP8 callback function 
        if(NULL != CMP8_EventHandler)
        {
            (*CMP8_EventHandler)();
        }
    
        // clear the CMP8 interrupt flag
        IFS8bits.CMP8IF = 0U;
    }
}

/**
 End of File
*/
