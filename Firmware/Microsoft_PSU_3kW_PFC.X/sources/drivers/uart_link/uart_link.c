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
* File Name       : uart_link.c
* Description     : Non-blocking UART1 access for the LLC link (polled, no IRQ)
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/11/01
* Modified by     :
* Modified on     :
*******************************************************************************/
#define UART_LINK_C

//------------------------------------------------------------------------------
// Include
#include "uart_link.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions

//------------------------------------------------------------------------------
// Public Variables

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes

//------------------------------------------------------------------------------
// Private Variables

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : UART_LINK_Flush
* Description     : Drop stale RX bytes and clear the error flags
* Purpose         : Start the link from a clean receiver
* Input           : None
* Return          : None
* Calls           : UART_LINK_RxReady, UART_LINK_RxByte
* Called by       : LLC_LINK_Init
* Note            : Bounded to two FIFO depths so a noisy line cannot hang
*                   start-up
*******************************************************************************/
void UART_LINK_Flush(void)
{
    uint8_t n;

    for (n = 0U; (n < 2U * UART_LINK_FIFO_DEPTH) && UART_LINK_RxReady(); n++)
    {
        (void)UART_LINK_RxByte();
    }
    U1STATbits.RXFOIF = 0;
    U1STATbits.FERIF  = 0;
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation

/******************************************************************************/
#undef UART_LINK_C
