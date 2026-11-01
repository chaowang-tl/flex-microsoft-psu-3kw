/**
 * ADC3 Generated Driver Source File
 * 
 * @file      adc3.c
 *            
 * @ingroup   adcdriver
 *            
 * @brief     This is the generated driver source file for ADC3 driver        
 *
 * @skipline @version   PLIB Version 1.2.2
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
#include <stddef.h>
#include "../adc3.h"

// Section: File specific functions

static void (*ADC3_ChannelHandler)(enum ADC3_CHANNEL channel, uint16_t adcVal) = NULL;
static void (*ADC3_Result32BitChannelHandler)(enum ADC3_CHANNEL channel, uint32_t adcVal) = NULL;
static void (*ADC3_ComparatorHandler)(enum ADC3_CMP comparator) = NULL;

// Section: File specific data type definitions

/**
 @ingroup  adcdriver
 @enum     ADC3_PWM_TRIG_SRCS
 @brief    Defines the PWM ADC TRIGGER sources available for the module to use.
*/
enum ADC3_PWM_TRIG_SRCS {
    PWM8_TRIGGER2 = 0x13, 
    PWM8_TRIGGER1 = 0x12, 
    PWM7_TRIGGER2 = 0x11, 
    PWM7_TRIGGER1 = 0x10, 
    PWM6_TRIGGER2 = 0xf, 
    PWM6_TRIGGER1 = 0xe, 
    PWM5_TRIGGER2 = 0xd, 
    PWM5_TRIGGER1 = 0xc, 
    PWM4_TRIGGER2 = 0xb, 
    PWM4_TRIGGER1 = 0xa, 
    PWM3_TRIGGER2 = 0x9, 
    PWM3_TRIGGER1 = 0x8, 
    PWM2_TRIGGER2 = 0x7, 
    PWM2_TRIGGER1 = 0x6, 
    PWM1_TRIGGER2 = 0x5, 
    PWM1_TRIGGER1 = 0x4, 
};

//Defines an object for ADC_MULTICORE.
static const struct ADC_MULTICORE adc3Multicore = {
    .ChannelSoftwareTriggerEnable           = &ADC3_ChannelSoftwareTriggerEnable,
    .ChannelSoftwareTriggerDisable          = &ADC3_ChannelSoftwareTriggerDisable,
    .SampleCountGet                         = NULL,
    .SampleCountStatusGet                   = NULL,
    .ChannelTasks                           = &ADC3_ChannelTasks, 
    .ComparatorTasks                        = NULL,
    .IndividualChannelInterruptEnable       = &ADC3_IndividualChannelInterruptEnable,
    .IndividualChannelInterruptDisable      = &ADC3_IndividualChannelInterruptDisable,
    .IndividualChannelInterruptFlagClear    = &ADC3_IndividualChannelInterruptFlagClear,
    .IndividualChannelInterruptPrioritySet  = &ADC3_IndividualChannelInterruptPrioritySet,
    .ChannelCallbackRegister                = &ADC3_ChannelCallbackRegister,
    .Result32BitChannelCallbackRegister     = &ADC3_Result32BitChannelCallbackRegister,
    .ComparatorCallbackRegister             = &ADC3_ComparatorCallbackRegister,
    .CorePowerEnable                        = NULL,
    .SharedCorePowerEnable                  = &ADC3_SharedCorePowerEnable,
    .CoreCalibration                        = NULL,
    .SharedCoreCalibration                  = &ADC3_SharedCoreCalibration,
    .PWMTriggerSourceSet                    = &ADC3_PWMTriggerSourceSet
};

//Defines an object for ADC_INTERFACE.

const struct ADC_INTERFACE ADC3 = {
    .Initialize             = &ADC3_Initialize,
    .Deinitialize           = &ADC3_Deinitialize,
    .Enable                 = &ADC3_Enable,
    .IsReady                = &ADC3_IsReady,
    .Disable                = &ADC3_Disable,
    .SoftwareTriggerEnable  = &ADC3_SoftwareTriggerEnable,
    .SoftwareTriggerDisable = &ADC3_SoftwareTriggerDisable,
    .ChannelSelect          = NULL, 
    .ConversionResultGet    = &ADC3_ConversionResultGet,
    .SecondaryAccumulatorGet= NULL,
    .IsConversionComplete   = &ADC3_IsConversionComplete,
    .ResolutionSet          = NULL,
    .InterruptEnable        = NULL,
    .InterruptDisable       = NULL,
    .InterruptFlagClear     = NULL,
    .InterruptPrioritySet   = NULL,
    .CommonCallbackRegister = NULL,
    .Tasks                  = NULL,
    .CalibrationReferenceSet = NULL,
    .adcMulticoreInterface = &adc3Multicore,
};

// Section: Driver Interface Function Definitions

void ADC3_Initialize(void)
{
    //CALCNT Wait for 2 activity free ADC clock cycles; BUFEN disabled; TSTEN disabled; ON enabled; STNDBY disabled; RPTCNT 1 ADC clock cycles between triggers; CALRATE Every second; ACALEN disabled; CALREQ Calibration cycle is not requested; 
    AD3CON = (uint32_t)0x8000UL & ~_AD1CON_ON_MASK;
    //DATAOVR 0x0; 
    AD3DATAOVR = 0x0UL;
    //CH0RDY disabled; CH1RDY disabled; CH2RDY disabled; CH3RDY disabled; CH4RDY disabled; CH5RDY disabled; CH6RDY disabled; CH7RDY disabled; 
    AD3STAT = 0x0UL;
    //CH0RRDY disabled; CH1RRDY disabled; CH2RRDY disabled; CH3RRDY disabled; CH4RRDY disabled; CH5RRDY disabled; CH6RRDY disabled; CH7RRDY disabled; 
    AD3RSTAT = 0x0UL;
    //CH0CMP disabled; CH1CMP disabled; CH2CMP disabled; CH3CMP disabled; CH4CMP disabled; CH5CMP disabled; CH6CMP disabled; CH7CMP disabled; 
    AD3CMPSTAT = 0x0UL;
    //CH0TRG disabled; CH1TRG disabled; CH2TRG disabled; CH3TRG disabled; CH4TRG disabled; CH5TRG disabled; CH6TRG disabled; CH7TRG disabled; 
    AD3SWTRG = 0x0UL;
    //TRG1SRC PWM2 Trigger1; MODE Single sample initiated by TRG1SRC[4:0] trigger; TRG2SRC Triggers are disabled; ACCNUM 4 samples, 13 bits result; SAMC 6.5 TAD; IRQSEL enabled; EIEN disabled; TRG1POL disabled; PINSEL AD3AN0; NINSEL AD3ANN0; FRAC Integer; DIFF disabled; 
    AD3CH0CON1 = 0x230006UL;
    //TRG1SRC PWM1 Trigger1; MODE Single sample initiated by TRG1SRC[4:0] trigger; TRG2SRC Triggers are disabled; ACCNUM 4 samples, 13 bits result; SAMC 12.5 TAD; IRQSEL enabled; EIEN disabled; TRG1POL disabled; PINSEL AD3AN2; NINSEL AD3ANN0; FRAC Integer; DIFF disabled; 
    AD3CH1CON1 = 0x2260004UL;
    //ADCMPCNT disabled; CMPMOD NONE; CMPCNTMOD disabled; CMPVAL enabled; ACCBRST disabled; ACCRO disabled; 
    AD3CH0CON2 = 0x20000000UL;
    //ADCMPCNT disabled; CMPMOD NONE; CMPCNTMOD disabled; CMPVAL enabled; ACCBRST disabled; ACCRO disabled; 
    AD3CH1CON2 = 0x20000000UL;
    //
    AD3CH0RES = 0x0UL;
    //
    AD3CH1RES = 0x0UL;
    //CNT 0x0; 
    AD3CH0CNT = 0x0UL;
    //CNT 0x0; 
    AD3CH1CNT = 0x0UL;
    //CMPLO 0x0; 
    AD3CH0CMPLO = 0x0UL;
    //CMPLO 0x0; 
    AD3CH1CMPLO = 0x0UL;
    //CMPHI 0x0; 
    AD3CH0CMPHI = 0x0UL;
    //CMPHI 0x0; 
    AD3CH1CMPHI = 0x0UL;

    ADC3_ChannelCallbackRegister(&ADC3_ChannelCallback);
    ADC3_Result32BitChannelCallbackRegister(&ADC3_Result32BitChannelCallback);
    ADC3_ComparatorCallbackRegister(&ADC3_ComparatorCallback);
    
    
    // ADC Mode change to run mode
    ADC3_SharedCorePowerEnable();
}

void ADC3_Deinitialize (void)
{
    AD3CONbits.ON = 0U;
    
    AD3CON = 0x480000UL;
    AD3DATAOVR = 0x0UL;
    AD3STAT = 0x0UL;
    AD3RSTAT = 0x0UL;
    AD3CMPSTAT = 0x0UL;
    AD3SWTRG = 0x0UL;
    AD3CH0CON1 = 0x0UL;
    AD3CH1CON1 = 0x0UL;
    AD3CH0CON2 = 0x1UL;
    AD3CH1CON2 = 0x1UL;
    AD3CH0RES = 0x0UL;
    AD3CH1RES = 0x0UL;
    AD3CH0CNT = 0x0UL;
    AD3CH1CNT = 0x0UL;
    AD3CH0CMPLO = 0x0UL;
    AD3CH1CMPLO = 0x0UL;
    AD3CH0CMPHI = 0x0UL;
    AD3CH1CMPHI = 0x0UL;
}

void ADC3_SharedCorePowerEnable (void) 
{
    AD3CONbits.ON = 1U;   
    while(AD3CONbits.ADRDY == 0U)
    {
    }
}

void ADC3_SharedCoreCalibration (void)
{
    AD3CONbits.CALREQ = 1U;
    while(AD3CONbits.CALRDY == 0U)
    {
    }
}

static uint16_t ADC3_TriggerSourceValueGet(enum ADC_PWM_INSTANCE pwmInstance, enum ADC_PWM_TRIGGERS triggerNumber)
{
    uint16_t adcTriggerSourceValue = 0x0U;
    switch(pwmInstance)
    {
        case ADC_PWM_GENERATOR_8:
                if(triggerNumber == ADC_PWM_TRIGGER_1)
                {
                    adcTriggerSourceValue = PWM8_TRIGGER1;
                }
                else if(triggerNumber == ADC_PWM_TRIGGER_2)
                {
                    adcTriggerSourceValue = PWM8_TRIGGER2;
                }
                else
                {
                }
                break;
        case ADC_PWM_GENERATOR_7:
                if(triggerNumber == ADC_PWM_TRIGGER_1)
                {
                    adcTriggerSourceValue = PWM7_TRIGGER1;
                }
                else if(triggerNumber == ADC_PWM_TRIGGER_2)
                {
                    adcTriggerSourceValue = PWM7_TRIGGER2;
                }
                else
                {
                }
                break;
        case ADC_PWM_GENERATOR_6:
                if(triggerNumber == ADC_PWM_TRIGGER_1)
                {
                    adcTriggerSourceValue = PWM6_TRIGGER1;
                }
                else if(triggerNumber == ADC_PWM_TRIGGER_2)
                {
                    adcTriggerSourceValue = PWM6_TRIGGER2;
                }
                else
                {
                }
                break;
        case ADC_PWM_GENERATOR_5:
                if(triggerNumber == ADC_PWM_TRIGGER_1)
                {
                    adcTriggerSourceValue = PWM5_TRIGGER1;
                }
                else if(triggerNumber == ADC_PWM_TRIGGER_2)
                {
                    adcTriggerSourceValue = PWM5_TRIGGER2;
                }
                else
                {
                }
                break;
        case ADC_PWM_GENERATOR_4:
                if(triggerNumber == ADC_PWM_TRIGGER_1)
                {
                    adcTriggerSourceValue = PWM4_TRIGGER1;
                }
                else if(triggerNumber == ADC_PWM_TRIGGER_2)
                {
                    adcTriggerSourceValue = PWM4_TRIGGER2;
                }
                else
                {
                }
                break;
        case ADC_PWM_GENERATOR_3:
                if(triggerNumber == ADC_PWM_TRIGGER_1)
                {
                    adcTriggerSourceValue = PWM3_TRIGGER1;
                }
                else if(triggerNumber == ADC_PWM_TRIGGER_2)
                {
                    adcTriggerSourceValue = PWM3_TRIGGER2;
                }
                else
                {
                }
                break;
        case ADC_PWM_GENERATOR_2:
                if(triggerNumber == ADC_PWM_TRIGGER_1)
                {
                    adcTriggerSourceValue = PWM2_TRIGGER1;
                }
                else if(triggerNumber == ADC_PWM_TRIGGER_2)
                {
                    adcTriggerSourceValue = PWM2_TRIGGER2;
                }
                else
                {
                }
                break;
        case ADC_PWM_GENERATOR_1:
                if(triggerNumber == ADC_PWM_TRIGGER_1)
                {
                    adcTriggerSourceValue = PWM1_TRIGGER1;
                }
                else if(triggerNumber == ADC_PWM_TRIGGER_2)
                {
                    adcTriggerSourceValue = PWM1_TRIGGER2;
                }
                else
                {
                }
                break;
         default:
                break;
    }
    return adcTriggerSourceValue;
}

void ADC3_PWMTriggerSourceSet(enum ADC3_CHANNEL channel, enum ADC_PWM_INSTANCE pwmInstance, enum ADC_PWM_TRIGGERS triggerNumber)
{
    uint16_t adcTriggerValue;
    adcTriggerValue= ADC3_TriggerSourceValueGet(pwmInstance, triggerNumber);
    switch(channel)
    {
        case ADC3_Channel0:
                AD3CH0CON1bits.TRG1SRC = adcTriggerValue;
                break;
        case ADC3_Channel1:
                AD3CH1CON1bits.TRG1SRC = adcTriggerValue;
                break;
        default:
                break;
    }
}

void ADC3_ChannelCallbackRegister(void(*callback)(enum ADC3_CHANNEL channel, uint16_t adcVal))
{
    if(NULL != callback)
    {
        ADC3_ChannelHandler = callback;
    }
}

void __attribute__ ( ( weak ) ) ADC3_ChannelCallback (enum ADC3_CHANNEL channel, uint16_t adcVal)
{ 
    (void)channel;
    (void)adcVal;
} 

void ADC3_Result32BitChannelCallbackRegister(void(*callback)(enum ADC3_CHANNEL channel, uint32_t adcVal))
{
    if(NULL != callback)
    {
        ADC3_Result32BitChannelHandler = callback;
    }
}

void __attribute__ ( ( weak ) ) ADC3_Result32BitChannelCallback (enum ADC3_CHANNEL channel, uint32_t adcVal)
{ 
    (void)channel;
    (void)adcVal;
} 


/* cppcheck-suppress misra-c2012-8.4
*
* (Rule 8.4) REQUIRED: A compatible declaration shall be visible when an object or 
* function with external linkage is defined
*
* Reasoning: Interrupt declaration are provided by compiler and are available
* outside the driver folder
*/
void __attribute__ ( ( __interrupt__, weak ) ) _AD3CH0Interrupt ( void )
{
    uint32_t valADC3_Channel0;
    //Read the ADC value from the ADCBUF
    valADC3_Channel0 = AD3CH0DATA;

    if(NULL != ADC3_ChannelHandler)
    {
        (*ADC3_ChannelHandler)(ADC3_Channel0, valADC3_Channel0);
    }
    if(NULL != ADC3_Result32BitChannelHandler)
    {
        (*ADC3_Result32BitChannelHandler)(ADC3_Channel0, valADC3_Channel0);
    }

    //clear the ADC3_Channel0 interrupt flag
    IFS6bits.AD3CH0IF = 0U;
}

/* cppcheck-suppress misra-c2012-8.4
*
* (Rule 8.4) REQUIRED: A compatible declaration shall be visible when an object or 
* function with external linkage is defined
*
* Reasoning: Interrupt declaration are provided by compiler and are available
* outside the driver folder
*/
void __attribute__ ( ( __interrupt__, weak ) ) _AD3CH1Interrupt ( void )
{
    uint32_t valADC3_Channel1;
    //Read the ADC value from the ADCBUF
    valADC3_Channel1 = AD3CH1DATA;

    if(NULL != ADC3_ChannelHandler)
    {
        (*ADC3_ChannelHandler)(ADC3_Channel1, valADC3_Channel1);
    }
    if(NULL != ADC3_Result32BitChannelHandler)
    {
        (*ADC3_Result32BitChannelHandler)(ADC3_Channel1, valADC3_Channel1);
    }

    //clear the ADC3_Channel1 interrupt flag
    IFS6bits.AD3CH1IF = 0U;
}


void __attribute__ ( ( weak ) ) ADC3_ChannelTasks (enum ADC3_CHANNEL channel)
{
    uint32_t adcVal;
    
    switch(channel)
    {   
        case ADC3_Channel0:
            if((bool)AD3STATbits.CH0RDY == 1U)
            {
                //Read the ADC value from the ADCBUF
                adcVal = AD3CH0DATA;

                if(NULL != ADC3_ChannelHandler)
                {
                    (*ADC3_ChannelHandler)(channel, adcVal);
                }
                if(NULL != ADC3_Result32BitChannelHandler)
                {
                    (*ADC3_Result32BitChannelHandler)(channel, adcVal);
                }
            }
            break;
        case ADC3_Channel1:
            if((bool)AD3STATbits.CH1RDY == 1U)
            {
                //Read the ADC value from the ADCBUF
                adcVal = AD3CH1DATA;

                if(NULL != ADC3_ChannelHandler)
                {
                    (*ADC3_ChannelHandler)(channel, adcVal);
                }
                if(NULL != ADC3_Result32BitChannelHandler)
                {
                    (*ADC3_Result32BitChannelHandler)(channel, adcVal);
                }
            }
            break;
        default:
            break;
    }            
}

void ADC3_ComparatorCallbackRegister(void(*callback)(enum ADC3_CMP comparator))
{
    if(NULL != callback)
    {
        ADC3_ComparatorHandler = callback;
    }
}

void __attribute__ ( ( weak ) ) ADC3_ComparatorCallback (enum ADC3_CMP comparator)
{ 
    (void)comparator;
} 



