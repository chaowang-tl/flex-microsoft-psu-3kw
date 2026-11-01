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
* File Name       : fw_update.h
* Description     : PFC firmware update session over the LLC link (framework)
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/31
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#ifndef FW_UPDATE_H
#define FW_UPDATE_H

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
#define FW_UPDATE_IMAGE_MAX         0x1F000UL   // One dual-boot partition, 256 KB device (124 KB)
#define FW_UPDATE_SESSION_TIMEOUT_MS 5000U      // No BOOT_* command -> abort

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions
typedef enum
{
    FW_UPDATE_IDLE = 0,
    FW_UPDATE_ACTIVE,           // BOOT_ENTER accepted, PFC held off
    FW_UPDATE_ERASED,           // Inactive partition erased
    FW_UPDATE_WRITING,          // At least one block written
    FW_UPDATE_VERIFIED          // Image CRC matched
} FW_UPDATE_STATE_e;

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef FW_UPDATE_C
    #define FW_UPDATE_C_PUBLIC
    #define FW_UPDATE_C_CONST
#else
    #define FW_UPDATE_C_PUBLIC extern
    #define FW_UPDATE_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
FW_UPDATE_C_PUBLIC volatile FW_UPDATE_STATE_e g_fwUpdateState;
FW_UPDATE_C_PUBLIC volatile uint32_t          g_fwUpdateNextOffset;  // Next expected BOOT_WRITE offset

//------------------------------------------------------------------------------
// Public Function Prototypes
FW_UPDATE_C_PUBLIC void    FW_UPDATE_Init(void);
FW_UPDATE_C_PUBLIC uint8_t FW_UPDATE_Handle(uint8_t cmd, const uint8_t *req, uint8_t reqLen,
                                            bool pfcStopped, uint8_t *rspData, uint8_t *rspDataLen);
FW_UPDATE_C_PUBLIC void    FW_UPDATE_Tick1ms(void);
FW_UPDATE_C_PUBLIC void    FW_UPDATE_KeepAlive(void);
FW_UPDATE_C_PUBLIC bool    FW_UPDATE_IsActive(void);
FW_UPDATE_C_PUBLIC bool    FW_UPDATE_NvmImplemented(void);
FW_UPDATE_C_PUBLIC uint8_t FW_UPDATE_ActivePartition(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* FW_UPDATE_H */
