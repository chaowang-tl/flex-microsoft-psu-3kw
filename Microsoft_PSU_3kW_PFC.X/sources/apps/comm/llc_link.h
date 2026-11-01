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
* File Name       : llc_link.h
* Description     : PFC side of the PFC <-> LLC UART1 link (slave)
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/31
* Modified by     :
* Modified on     :
*******************************************************************************/
#ifndef LLC_LINK_H
#define LLC_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Include
#include <xc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "llc_protocol.h"

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions
// Link diagnostics (X2Cscope)
typedef struct
{
    uint32_t rxFrames;          // Valid frames received
    uint32_t txFrames;          // Responses queued
    uint32_t crcErrors;
    uint32_t lenErrors;         // LEN above LLC_PROTO_PAYLOAD_MAX
    uint32_t gapDrops;          // Partial frames dropped on inter-byte timeout
    uint32_t rxOverruns;        // Hardware FIFO or software ring overrun
    uint32_t framingErrors;
    uint32_t txDrops;           // Response did not fit in the TX ring
    uint32_t retries;           // Duplicate requests answered from the cache
} LLC_LINK_STATS_T;

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef LLC_LINK_C
    #define LLC_LINK_C_PUBLIC
    #define LLC_LINK_C_CONST
#else
    #define LLC_LINK_C_PUBLIC extern
    #define LLC_LINK_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
LLC_LINK_C_PUBLIC volatile bool           g_llcLinkUp;       // Valid frame within LLC_PROTO_LINK_LOST_MS
LLC_LINK_C_PUBLIC LLC_PROTO_LLC_STATUS_T  g_llcStatus;       // Last LLC_STATUS payload from the LLC
LLC_LINK_C_PUBLIC volatile bool           g_llcStatusValid;  // g_llcStatus received within LLC_PROTO_LINK_LOST_MS
LLC_LINK_C_PUBLIC LLC_LINK_STATS_T        g_llcLinkStats;

//------------------------------------------------------------------------------
// Public Function Prototypes
LLC_LINK_C_PUBLIC void     LLC_LINK_Init(void);
LLC_LINK_C_PUBLIC void     LLC_LINK_ByteService(void);
LLC_LINK_C_PUBLIC void     LLC_LINK_Task(void);
LLC_LINK_C_PUBLIC void     LLC_LINK_Tick1ms(void);
LLC_LINK_C_PUBLIC bool     LLC_LINK_TxIdle(void);
LLC_LINK_C_PUBLIC uint16_t LLC_LINK_Crc16(const uint8_t *data, uint16_t len, uint16_t crc);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* LLC_LINK_H */
