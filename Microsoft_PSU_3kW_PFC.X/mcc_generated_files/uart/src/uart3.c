/**
 * UART3 Generated Driver Source File
 * 
 * @file      uart3.c
 *            
 * @ingroup   uartdriver
 *            
 * @brief     This is the generated driver source file for the UART3 driver.
 *
 * @skipline @version   PLIB Version 1.1.4
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
#include <stdint.h>
#include <stddef.h>
#include <xc.h>
#include <stddef.h>
#include "../uart3.h"

// Section: Macro Definitions
#define UART3_CLOCK 100000000U
#define UART3_BAUD_TO_BRG_WITH_FRACTIONAL(x) (UART3_CLOCK/(x))
#define UART3_BAUD_TO_BRG_WITH_BRGS_1(x) (UART3_CLOCK/(4U*(x))-1U)
#define UART3_BAUD_TO_BRG_WITH_BRGS_0(x) (UART3_CLOCK/(16U*(x))-1U)
#define UART3_BRG_TO_BAUD_WITH_FRACTIONAL(x) (UART3_CLOCK/(x))
#define UART3_BRG_TO_BAUD_WITH_BRGS_1(x) (UART3_CLOCK/(4U*((x)+1U)))
#define UART3_BRG_TO_BAUD_WITH_BRGS_0(x) (UART3_CLOCK/(16U*((x)+1U)))

#define UART3_MIN_ACHIEVABLE_BAUD_WITH_FRACTIONAL 95U
#define UART3_MIN_ACHIEVABLE_BAUD_WITH_BRGS_1 24U

// Section: Driver Interface

const struct UART_INTERFACE UART3_Drv = {
    .Initialize = &UART3_Initialize,
    .Deinitialize = &UART3_Deinitialize,
    .Read = &UART3_Read,
    .Write = &UART3_Write,
    .IsRxReady = &UART3_IsRxReady,
    .IsTxReady = &UART3_IsTxReady,
    .IsTxDone = &UART3_IsTxDone,
    .TransmitEnable = &UART3_TransmitEnable,
    .TransmitDisable = &UART3_TransmitDisable,
    .TransmitInterruptEnable = NULL,
    .TransmitInterruptDisable = NULL,
    .AutoBaudSet = &UART3_AutoBaudSet,
    .AutoBaudQuery = &UART3_AutoBaudQuery,
    .AutoBaudEventEnableGet = &UART3_AutoBaudEventEnableGet,
    .BRGCountSet = &UART3_BRGCountSet,
    .BRGCountGet = &UART3_BRGCountGet,
    .BaudRateSet = &UART3_BaudRateSet,
    .BaudRateGet = &UART3_BaudRateGet,
    .ErrorGet = &UART3_ErrorGet,
    .RxCompleteCallbackRegister = NULL,
    .TxCompleteCallbackRegister = NULL,
    .TxCollisionCallbackRegister = NULL,
    .FramingErrorCallbackRegister = NULL,
    .OverrunErrorCallbackRegister = NULL,
    .ParityErrorCallbackRegister = NULL,
};

// Section: Private Variable Definitions
static union
{
    struct
    {
        uint16_t frammingError :1;
        uint16_t parityError :1;
        uint16_t overrunError :1;
        uint16_t txCollisionError :1;
        uint16_t autoBaudOverflow :1;
        uint16_t reserved :11;
    };
    size_t status;
} uartError;

// Section: UART3 APIs

void UART3_Initialize(void)
{
/*    
     Set the UART3 module to the options selected in the user interface.
     Make sure to set LAT bit corresponding to TxPin as high before UART initialization
*/
    // MODE Asynchronous 8-bit UART; RXEN ; TXEN ; ABDEN ; BRGS ; SENDB ; BRKOVR ; RXBIMD ; WUE ; SIDL ; ON disabled; FLO ; TXPOL ; C0EN ; STP 1 Stop bit sent, 1 checked at RX; RXPOL ; RUNOVF ; HALFDPLX ; CLKSEL Standard Speed Peripheral Clock; CLKMOD enabled; ACTIVE ; SLPEN ; 
    U3CON = 0x8000000UL;
    // TXCIF ; RXFOIF ; RXBKIF ; CERIF ; ABDOVIF ; TXCIE ; RXFOIE ; RXBKIE ; FERIE ; CERIE ; ABDOVIE ; PERIE ; TXMTIE ; STPMD ; TXWRE ; RXWM ; TXWM ; 
    U3STAT = 0x2E0080UL;
    // BaudRate 115207.37; Frequency 100000000 Hz; BRG 868; 
    U3BRG = 0x364UL;
    
    U3CONbits.ON = 1;   // enabling UART ON bit
    U3CONbits.TXEN = 1;
    U3CONbits.RXEN = 1;
}

void UART3_Deinitialize(void)
{
    U3CON = 0x0UL;
    U3STAT = 0x2E0080UL;
    U3BRG = 0x0UL;
}

uint8_t UART3_Read(void)
{
    while((U3STATbits.RXBE == 1))
    {
        
    }

    if ((U3STATbits.RXFOIF == 1))
    {
        U3STATbits.RXFOIF = 0;
    }
    
    return U3RXB;
}

void UART3_Write(uint8_t txData)
{
    while(U3STATbits.TXBF == 1)
    {
        
    }

    U3TXB = txData;    // Write the data byte to the USART.
}

bool UART3_IsRxReady(void)
{
    return (U3STATbits.RXBE == 0);
}

bool UART3_IsTxReady(void)
{
    return ((!U3STATbits.TXBF) && U3CONbits.TXEN);
}

bool UART3_IsTxDone(void)
{
    return (bool)(U3STATbits.TXMTIF && U3STATbits.TXBE);
}

void UART3_TransmitEnable(void)
{
    U3CONbits.TXEN = 1;
}

void UART3_TransmitDisable(void)
{
    U3CONbits.TXEN = 0;
}

void UART3_AutoBaudSet(bool enable)
{
    U3UIRbits.ABDIF = 0U;
    U3UIRbits.ABDIE = enable;
    U3CONbits.ABDEN = enable;
}

bool UART3_AutoBaudQuery(void)
{
    return U3CONbits.ABDEN;
}

bool UART3_AutoBaudEventEnableGet(void)
{ 
    return U3UIRbits.ABDIE; 
}

size_t UART3_ErrorGet(void)
{
    uartError.status = 0;
    if(U3STATbits.FERIF == 1U)
    {
        uartError.status = uartError.status|(uint16_t)UART_ERROR_FRAMING_MASK;
    }
    if(U3STATbits.PERIF== 1U)
    {
        uartError.status = uartError.status|(uint16_t)UART_ERROR_PARITY_MASK;
    }
    if(U3STATbits.RXFOIF== 1U)
    {
        uartError.status = uartError.status|(uint16_t)UART_ERROR_RX_OVERRUN_MASK;
        U3STATbits.RXFOIF = 0;
    }
    if(U3STATbits.TXCIF== 1U)
    {
        uartError.status = uartError.status|(uint16_t)UART_ERROR_TX_COLLISION_MASK;
        U3STATbits.TXCIF = 0;
    }
    if(U3STATbits.ABDOVIF== 1U)
    {
        uartError.status = uartError.status|(uint16_t)UART_ERROR_AUTOBAUD_OVERFLOW_MASK;
        U3STATbits.ABDOVIF = 0;
    }
    
    return uartError.status;
}

void UART3_BRGCountSet(uint32_t brgValue)
{
    U3BRG = brgValue;
}

uint32_t UART3_BRGCountGet(void)
{
    return U3BRG;
}

void UART3_BaudRateSet(uint32_t baudRate)
{
    uint32_t brgValue;
    
    if((baudRate >= UART3_MIN_ACHIEVABLE_BAUD_WITH_FRACTIONAL) && (baudRate != 0U))
    {
        U3CONbits.CLKMOD = 1;
        U3CONbits.BRGS = 0;
        brgValue = UART3_BAUD_TO_BRG_WITH_FRACTIONAL(baudRate);
    }
    else if(baudRate >= UART3_MIN_ACHIEVABLE_BAUD_WITH_BRGS_1)
    {
        U3CONbits.CLKMOD = 0;
        U3CONbits.BRGS = 1;
        brgValue = UART3_BAUD_TO_BRG_WITH_BRGS_1(baudRate);
    }
    else
    {
        U3CONbits.CLKMOD = 0;
        U3CONbits.BRGS = 0;
        brgValue = UART3_BAUD_TO_BRG_WITH_BRGS_0(baudRate);
    }
    U3BRG = brgValue;
}

uint32_t UART3_BaudRateGet(void)
{
    uint32_t brgValue;
    uint32_t baudRate;
    
    brgValue = UART3_BRGCountGet();
    if((U3CONbits.CLKMOD == 1U) && (brgValue != 0U))
    {
        baudRate = UART3_BRG_TO_BAUD_WITH_FRACTIONAL(brgValue);
    }
    else if(U3CONbits.BRGS == 1)
    {
        baudRate = UART3_BRG_TO_BAUD_WITH_BRGS_1(brgValue);
    }
    else
    {
        baudRate = UART3_BRG_TO_BAUD_WITH_BRGS_0(brgValue);
    }
    return baudRate;
}
