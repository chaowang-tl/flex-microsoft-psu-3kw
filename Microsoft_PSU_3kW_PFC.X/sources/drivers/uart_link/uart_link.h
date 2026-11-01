/*
    [2026] Microchip Technology Inc. and its subsidiaries.

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

/*******************************************************************************
* File Name       : uart_link.h
* Description     : Non-blocking UART1 access for the LLC link (polled, no IRQ)
*
*   UART1 is configured and enabled by MCC (115200 8N1, RC8 RX, RC9 TX).
*   These accessors replace the MCC UART1_Read/Write, which busy-wait.
*   Each call touches the hardware once and returns; the caller (T1) bounds
*   the loops with UART_LINK_FIFO_DEPTH.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/11/01
* Modified by     :
* Modified on     :
*******************************************************************************/
#ifndef UART_LINK_H
#define UART_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Include
#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
#define UART_LINK_FIFO_DEPTH        8U          // UART1 RX and TX hardware FIFO depth

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef UART_LINK_C
    #define UART_LINK_C_PUBLIC
    #define UART_LINK_C_CONST
#else
    #define UART_LINK_C_PUBLIC extern
    #define UART_LINK_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables

//------------------------------------------------------------------------------
// Public Function Prototypes
UART_LINK_C_PUBLIC void UART_LINK_Flush(void);

// True while the RX FIFO holds at least one byte
inline static bool UART_LINK_RxReady(void)
{
    return (U1STATbits.RXBE == 0U);
}

// Read one byte; call only when UART_LINK_RxReady()
inline static uint8_t UART_LINK_RxByte(void)
{
    return (uint8_t)U1RXB;
}

// True while the TX FIFO can take one more byte
inline static bool UART_LINK_TxReady(void)
{
    return (U1STATbits.TXBF == 0U);
}

// Write one byte; call only when UART_LINK_TxReady()
inline static void UART_LINK_TxByte(uint8_t byte)
{
    U1TXB = byte;
}

// True when the TX shift register is empty (last stop bit sent)
inline static bool UART_LINK_TxDone(void)
{
    return (U1STATbits.TXMTIF != 0U);
}

// Clear and return the RX overrun flag
inline static bool UART_LINK_RxOverrunTake(void)
{
    if (U1STATbits.RXFOIF == 0U)
    {
        return false;
    }
    U1STATbits.RXFOIF = 0;
    return true;
}

// Clear and return the framing-error flag
inline static bool UART_LINK_FramingErrorTake(void)
{
    if (U1STATbits.FERIF == 0U)
    {
        return false;
    }
    U1STATbits.FERIF = 0;
    return true;
}

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* UART_LINK_H */
