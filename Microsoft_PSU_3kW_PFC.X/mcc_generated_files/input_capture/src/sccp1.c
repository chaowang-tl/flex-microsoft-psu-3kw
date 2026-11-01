/**
 * SCCP1-InputCapture Generated Driver Source File
 * 
 * @file 	  sccp1.c
 * 
 * @ingroup   mccpdriver
 * 
 * @brief 	  This is the generated driver source file for SCCP1-InputCapture driver
 *
 * @skipline @version   PLIB Version 1.2.3
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
#include <stddef.h> 
#include "../sccp1.h"

// Section: File specific functions

static void (*SCCP1_InputCaptureHandler)(void) = NULL;

// Section: Driver Interface

const struct INPUT_CAPTURE_INTERFACE Input_Capture1 = {
    .Initialize          = &SCCP1_InputCapture_Initialize,
    .Deinitialize        = &SCCP1_InputCapture_Deinitialize,
    .Start               = &SCCP1_InputCapture_Start,
    .Stop                = &SCCP1_InputCapture_Stop,
    .InputCapture_CallbackRegister = &SCCP1_InputCapture_CallbackRegister,
    .Tasks               = &SCCP1_InputCapture_Tasks,
    .DataRead            = &SCCP1_InputCapture_DataRead,
    .HasBufferOverflowed = &SCCP1_InputCapture_HasBufferOverflowed,
    .IsBufferEmpty       = &SCCP1_InputCapture_IsBufferEmpty,
    .OverflowFlagClear   = &SCCP1_InputCapture_OverflowFlagClear,
};

// Section: Driver Interface Function Definitions

void SCCP1_InputCapture_Initialize(void)
{
    //MOD Every falling edge; CCSEL enabled; T32 16 Bit; TMRPS 1:1; CLKSEL Standard Speed Peripheral Clock; TMRSYNC disabled; SIDL disabled; ON disabled; SYNC SCCP2; ALTSYNC disabled; ONESHOT disabled; TRIGEN disabled; OPS Each Time Base Period Match; RTRGEN disabled; OPSSRC Timer Interrupt Event; 
    CCP1CON1 = 0x20012UL;
    //ASDG disabled; SSDG disabled; ASDGM disabled; PWMRSEN disabled; ICS None; AUXOUT Disabled; ICGSM Level-Sensitive mode; OCAEN disabled; OENSYNC disabled; 
    CCP1CON2 = 0x0UL;
    //PSSACE Tri-state; POLACE disabled; OSCNT None; OETRIG disabled; PSSBDF Tri-state; POLBDF disabled; 
    CCP1CON3 = 0x0UL;
    //ICOV disabled; SCEVT disabled; ASEVT disabled; TRCLR disabled; TRSET disabled; ICGARM disabled; RAWIP disabled; RBWIP disabled; TMRLWIP disabled; TMRHWIP disabled; PRLWIP disabled; 
    CCP1STAT = 0x0UL;
    //TMRL 0x0; TMRH 0x0; 
    CCP1TMR = 0x0UL;
    //PRL 0; PRH 0; 
    CCP1PR = 0x0UL;
    //BUFL 0x0; BUFH 0x0; 
    CCP1BUF = 0x0UL;
    //CMPA 0x0; 
    CCP1RA = 0x0UL;
    //CMPB 0x0; 
    CCP1RB = 0x0UL;
    
    SCCP1_InputCapture_CallbackRegister(&SCCP1_InputCapture_Callback);



}

void SCCP1_InputCapture_Deinitialize(void)
{
    CCP1CON1bits.ON = 0;
    
    
    CCP1CON1 = 0x0UL;
    CCP1CON2 = 0x1000000UL;
    CCP1CON3 = 0x0UL;
    CCP1STAT = 0x0UL;
    CCP1TMR = 0x0UL;
    CCP1PR = 0xFFFFFFFFUL;
    CCP1BUF = 0x0UL;
    CCP1RA = 0x0UL;
    CCP1RB = 0x0UL;
}

void SCCP1_InputCapture_Start(void)
{
    
    CCP1CON1bits.ON = 1;
}

void SCCP1_InputCapture_Stop(void)
{
    CCP1CON1bits.ON = 0;
    
}

void SCCP1_InputCapture_CallbackRegister(void (*handler)(void))
{
    if(NULL != handler)
    {
        SCCP1_InputCaptureHandler = handler;
    }
}

void __attribute__ ((weak)) SCCP1_InputCapture_Callback (void)
{ 

} 

void SCCP1_InputCapture_Tasks(void)
{
    if(IFS1bits.CCP1IF == 1)
    {
        if(NULL != SCCP1_InputCaptureHandler)
        {
            (*SCCP1_InputCaptureHandler)();
        }
        IFS1bits.CCP1IF = 0;
    }
}

uint32_t SCCP1_InputCapture_DataRead(void)
{
    uint32_t captureVal = 0;

    captureVal = CCP1BUF;

    return(captureVal);
}

bool SCCP1_InputCapture_HasBufferOverflowed(void)
{
    return(CCP1STATbits.ICOV);
}

bool SCCP1_InputCapture_IsBufferEmpty(void)
{
    return(!CCP1STATbits.ICBNE);
}

void SCCP1_InputCapture_OverflowFlagClear(void)
{
    CCP1STATbits.ICOV = 0;
}

/**
 End of File
*/
