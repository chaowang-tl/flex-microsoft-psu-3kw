/**
 * CMP5 Generated Driver Source File
 * 
 * @file      cmp5.c
 *            
 * @ingroup   cmpdriver
 *            
 * @brief     This is the generated driver source file for CMP5 driver
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
#include "../cmp5.h"

#ifndef CMP_FPDMDAC_ADDRESS
#define CMP_FPDMDAC_ADDRESS 0x7F20B0UL
#endif //CMP_FPDMDAC_ADDRESS

// Section: File specific functions

static void (*CMP5_EventHandler)(void) = NULL;

// Section: Driver Interface
static const struct DAC_DC_INTERFACE dac5_dc_interface = {
    .Enable = &CMP5_DACEnable,
    .Disable = &CMP5_DACDisable,
    .DataWrite = &CMP5_DACDataWrite,
};

const struct CMP_INTERFACE CMP_DAC5 = {
    .Initialize = &CMP5_Initialize,
    .Deinitialize = &CMP5_Deinitialize,
    .Enable = &CMP5_Enable,
    .Disable = &CMP5_Disable,
    .StatusGet = &CMP5_StatusGet,
    .Calibrate = &CMP5_Calibrate,
    
    .EventCallbackRegister = &CMP5_EventCallbackRegister,
    .Tasks = &CMP5_Tasks,
    .cmp_dac_dc_interface = &dac5_dc_interface,
    .cmp_dac_slope_interface = NULL,
};

// Section: CMP5 Module APIs

void CMP5_Initialize(void)
{           
    // Comparator Register settings
    DACCTRL1 = 0x3F7F0000UL; //FCLKDIV 1:1; DNLADJ 0x0; SIDL disabled; ON disabled; NEGINLADJ 127; POSINLADJ 63; RREN disabled; 
    DACCTRL2 = 0x8A0001UL; //TMODTIME 1; SSTIME 138; 
    DAC5CON = 0x800CUL; //DACOEN disabled; UPDTMDIS disabled; IRQM Interrupts are disabled; DACEN enabled; UPDREQ disabled; UPDTRG Any write(s) to DACDAT or SLPDAT sets the UPDATE bit immediately; TMCB 0; 
    DAC5CMP = 0x60010UL; //INNSEL DACx; INPSEL CMP5C; CMPSTAT disabled; FLTREN disabled; CBE disabled; HYSSEL 30 mv hysteresis; HYSPOL Falling Edge; 

    //Slope Settings
    DAC5DAT = 0xF3200CDUL; //DACLOW 205; DACDAT 3890; 
    DAC5SLPCON = 0x0UL; //SLPSTRT None; SLPSTOPB None; SLPSTOPA None; HCFSEL None; FFSEN disabled; PSE Negative; TWME disabled; HME disabled; SLOPEN disabled; 
    DAC5SLPDAT = 0x0UL; //SLPDAT 0; 
    
    CMP5_EventCallbackRegister(&CMP5_EventCallback);
    
    DACCTRL1bits.ON = 1U;
    //wait till update is complete, data registers are not writable until update is complete
    while(DAC5CONbits.UPDATE == 1U)
    {
    }
}

void CMP5_Deinitialize(void)
{ 
    DACCTRL1bits.ON = 0;
    
    
    // Comparator Register settings
    DACCTRL1 = 0x3F7F0000UL;
    DACCTRL2 = 0x8A0001UL;
    DAC5CON = 0x0UL;
    DAC5CMP = 0x0UL;

    //Slope Settings
    DAC5DAT = 0x0UL;
    DAC5SLPCON = 0x0UL;
    DAC5SLPDAT = 0x0UL;
}

void CMP5_Calibrate(void)
{
    uint32_t *fpdmdac = (uint32_t*)CMP_FPDMDAC_ADDRESS;
    DACCTRL1bits.POSINLADJ = ((*fpdmdac) & _DACCTRL1_POSINLADJ_MASK) >> _DACCTRL1_POSINLADJ_POSITION;
    DACCTRL1bits.NEGINLADJ = ((*fpdmdac) & _DACCTRL1_NEGINLADJ_MASK) >> _DACCTRL1_NEGINLADJ_POSITION;
    DACCTRL1bits.DNLADJ = ((*fpdmdac) & _DACCTRL1_DNLADJ_MASK) >> _DACCTRL1_DNLADJ_POSITION;
    DACCTRL1bits.RREN = 1U;
}

void CMP5_EventCallbackRegister(void (*handler)(void))
{
    if(NULL != handler)
    {
        CMP5_EventHandler = handler;
    }
}

void __attribute__ ((weak)) CMP5_EventCallback(void)
{ 
   
}
 
void CMP5_Tasks(void)
{
    if(IFS8bits.CMP5IF == 1U)
    {
        // CMP5 callback function 
        if(NULL != CMP5_EventHandler)
        {
            (*CMP5_EventHandler)();
        }
    
        // clear the CMP5 interrupt flag
        IFS8bits.CMP5IF = 0U;
    }
}

/**
 End of File
*/
