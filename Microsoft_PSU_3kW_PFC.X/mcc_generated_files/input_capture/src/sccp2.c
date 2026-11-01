/**
 * SCCP2-InputCapture Generated Driver Source File
 * 
 * @file 	  sccp2.c
 * 
 * @ingroup   mccpdriver
 * 
 * @brief 	  This is the generated driver source file for SCCP2-InputCapture driver
 *
 * @skipline @version   PLIB Version 1.2.3
 *
 * @skipline  Device : dsPIC33AK256MPS206
*/

/*
© [2026] Microchip Technology Inc. and its subsidiaries.

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
#include <stddef.h> 
#include "../sccp2.h"

// Section: File specific functions

static void (*SCCP2_InputCaptureHandler)(void) = NULL;

// Section: Driver Interface

const struct INPUT_CAPTURE_INTERFACE Input_Capture2 = {
    .Initialize          = &SCCP2_InputCapture_Initialize,
    .Deinitialize        = &SCCP2_InputCapture_Deinitialize,
    .Start               = &SCCP2_InputCapture_Start,
    .Stop                = &SCCP2_InputCapture_Stop,
    .InputCapture_CallbackRegister = &SCCP2_InputCapture_CallbackRegister,
    .Tasks               = &SCCP2_InputCapture_Tasks,
    .DataRead            = &SCCP2_InputCapture_DataRead,
    .HasBufferOverflowed = &SCCP2_InputCapture_HasBufferOverflowed,
    .IsBufferEmpty       = &SCCP2_InputCapture_IsBufferEmpty,
    .OverflowFlagClear   = &SCCP2_InputCapture_OverflowFlagClear,
};

// Section: Driver Interface Function Definitions

void SCCP2_InputCapture_Initialize(void)
{
    //MOD Every rising edge; CCSEL enabled; T32 16 Bit; TMRPS 1:1; CLKSEL Standard Speed Peripheral Clock; TMRSYNC disabled; SIDL disabled; ON disabled; SYNC None; ALTSYNC disabled; ONESHOT disabled; TRIGEN disabled; OPS Each Time Base Period Match; RTRGEN disabled; OPSSRC Timer Interrupt Event; 
    CCP2CON1 = 0x11UL;
    //ASDG disabled; SSDG disabled; ASDGM disabled; PWMRSEN disabled; ICS None; AUXOUT Disabled; ICGSM Level-Sensitive mode; OCAEN disabled; OENSYNC disabled; 
    CCP2CON2 = 0x0UL;
    //PSSACE Tri-state; POLACE disabled; OSCNT None; OETRIG disabled; PSSBDF Tri-state; POLBDF disabled; 
    CCP2CON3 = 0x0UL;
    //ICOV disabled; SCEVT disabled; ASEVT disabled; TRCLR disabled; TRSET disabled; ICGARM disabled; RAWIP disabled; RBWIP disabled; TMRLWIP disabled; TMRHWIP disabled; PRLWIP disabled; 
    CCP2STAT = 0x0UL;
    //TMRL 0x0; TMRH 0x0; 
    CCP2TMR = 0x0UL;
    //PRL 0; PRH 0; 
    CCP2PR = 0x0UL;
    //BUFL 0x0; BUFH 0x0; 
    CCP2BUF = 0x0UL;
    //CMPA 0x0; 
    CCP2RA = 0x0UL;
    //CMPB 0x0; 
    CCP2RB = 0x0UL;
    
    SCCP2_InputCapture_CallbackRegister(&SCCP2_InputCapture_Callback);



}

void SCCP2_InputCapture_Deinitialize(void)
{
    CCP2CON1bits.ON = 0;
    
    
    CCP2CON1 = 0x0UL;
    CCP2CON2 = 0x1000000UL;
    CCP2CON3 = 0x0UL;
    CCP2STAT = 0x0UL;
    CCP2TMR = 0x0UL;
    CCP2PR = 0xFFFFFFFFUL;
    CCP2BUF = 0x0UL;
    CCP2RA = 0x0UL;
    CCP2RB = 0x0UL;
}

void SCCP2_InputCapture_Start(void)
{
    
    CCP2CON1bits.ON = 1;
}

void SCCP2_InputCapture_Stop(void)
{
    CCP2CON1bits.ON = 0;
    
}

void SCCP2_InputCapture_CallbackRegister(void (*handler)(void))
{
    if(NULL != handler)
    {
        SCCP2_InputCaptureHandler = handler;
    }
}

void __attribute__ ((weak)) SCCP2_InputCapture_Callback (void)
{ 

} 

void SCCP2_InputCapture_Tasks(void)
{
    if(IFS1bits.CCP2IF == 1)
    {
        if(NULL != SCCP2_InputCaptureHandler)
        {
            (*SCCP2_InputCaptureHandler)();
        }
        IFS1bits.CCP2IF = 0;
    }
}

uint32_t SCCP2_InputCapture_DataRead(void)
{
    uint32_t captureVal = 0;

    captureVal = CCP2BUF;

    return(captureVal);
}

bool SCCP2_InputCapture_HasBufferOverflowed(void)
{
    return(CCP2STATbits.ICOV);
}

bool SCCP2_InputCapture_IsBufferEmpty(void)
{
    return(!CCP2STATbits.ICBNE);
}

void SCCP2_InputCapture_OverflowFlagClear(void)
{
    CCP2STATbits.ICOV = 0;
}

/**
 End of File
*/
