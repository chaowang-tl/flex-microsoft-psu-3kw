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
* File Name       : llc_protocol.h
* Description     : PFC <-> LLC link protocol definitions (shared by both sides)
*
*   Specification: docs/PFC_LLC_Comm_Protocol_EN.md (_CN for Chinese).
*   This header must stay byte-identical in the PFC and LLC projects; change both together and
*   bump LLC_PROTO_VERSION_MINOR (compatible) or _MAJOR (incompatible).
*   No MCU-specific includes: only <stdint.h> and llc_duty_def.h (copy
*   sources/common/llc_duty_def.h along with this file).
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/31
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#ifndef LLC_PROTOCOL_H
#define LLC_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Include
#include <stdint.h>
#include "../../common/llc_duty_def.h"

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
#define LLC_PROTO_VERSION_MAJOR     1U
#define LLC_PROTO_VERSION_MINOR     0U

// Frame: SOF | LEN | SEQ | CMD | PAYLOAD[LEN] | CRC16_L | CRC16_H
// CRC16/Modbus (poly 0xA001 reflected, init 0xFFFF) over LEN..PAYLOAD
#define LLC_PROTO_SOF               0xA5U
#define LLC_PROTO_HEADER_LEN        4U          // SOF, LEN, SEQ, CMD
#define LLC_PROTO_CRC_LEN           2U
#define LLC_PROTO_PAYLOAD_MAX       136U        // BOOT_WRITE: 4 addr + 4 rsvd + 128 data
#define LLC_PROTO_FRAME_MAX         (LLC_PROTO_HEADER_LEN + LLC_PROTO_PAYLOAD_MAX + LLC_PROTO_CRC_LEN)
#define LLC_PROTO_CRC_INIT          0xFFFFU
#define LLC_PROTO_CRC_POLY          0xA001U
#define LLC_PROTO_RSP_FLAG          0x80U       // Response CMD = request CMD | 0x80

// Session timing [ms]
#define LLC_PROTO_INTERBYTE_MS      2U          // Gap that discards a partial frame
#define LLC_PROTO_RSP_START_MS      2U          // Slave starts the response within this time
#define LLC_PROTO_RSP_TIMEOUT_MS    20U         // Master: complete response not received -> retry
#define LLC_PROTO_RETRY_MAX         3U          // Master retries with the same SEQ
#define LLC_PROTO_POLL_PERIOD_MS    10U         // Nominal LLC_STATUS period
#define LLC_PROTO_LINK_LOST_MS      100U        // No valid frame -> link lost

// Commands (requests, master LLC -> slave PFC)
#define LLC_CMD_GET_INFO            0x01U
#define LLC_CMD_GET_STATUS          0x10U
#define LLC_CMD_GET_LOAD_ECHO       0x11U
#define LLC_CMD_LLC_STATUS          0x12U
#define LLC_CMD_SET_ENABLE          0x20U
#define LLC_CMD_SET_VOUT_REF        0x21U
#define LLC_CMD_CLEAR_FAULT         0x22U
#define LLC_CMD_BOOT_ENTER          0x30U
#define LLC_CMD_BOOT_ERASE          0x31U
#define LLC_CMD_BOOT_WRITE          0x32U
#define LLC_CMD_BOOT_VERIFY         0x33U
#define LLC_CMD_BOOT_COMMIT         0x34U
#define LLC_CMD_BOOT_ABORT          0x35U

// Response status (first payload byte of every response)
#define LLC_STS_OK                  0x00U
#define LLC_STS_BAD_CRC             0x01U       // Reserved: frames with a bad CRC are not answered
#define LLC_STS_BAD_LEN             0x02U
#define LLC_STS_UNKNOWN_CMD         0x03U
#define LLC_STS_BAD_PARAM           0x04U
#define LLC_STS_BUSY                0x05U
#define LLC_STS_NOT_ALLOWED         0x06U
#define LLC_STS_NOT_IMPLEMENTED     0x07U
#define LLC_STS_FLASH_ERR           0x08U
#define LLC_STS_VERIFY_ERR          0x09U
#define LLC_STS_SEQUENCE_ERR        0x0AU

// Fixed-point scaling of the payload fields
#define LLC_SCALE_V                 10.0f       // V   x 10   -> 0.1 V
#define LLC_SCALE_A                 100.0f      // A   x 100  -> 0.01 A
#define LLC_SCALE_DEGC              10.0f       // degC x 10  -> 0.1 degC
#define LLC_SCALE_HZ                100.0f      // Hz  x 100  -> 0.01 Hz
#define LLC_SCALE_W                 1.0f        // W
#define LLC_SCALE_PERMILLE          1000.0f     // ratio x 1000

// PFC_STATUS.flags
#define LLC_PFC_FLAG_PLL_LOCKED     0x0001U
#define LLC_PFC_FLAG_ENABLE_CMD     0x0002U
#define LLC_PFC_FLAG_FAULT_ACTIVE   0x0004U
#define LLC_PFC_FLAG_FAULT_LOCKED   0x0008U
#define LLC_PFC_FLAG_LOAD_VALID     0x0010U     // RD6 load duty valid
#define LLC_PFC_FLAG_LOAD_FF_ON     0x0020U     // Load feed-forward in use
#define LLC_PFC_FLAG_BOOT_SESSION   0x0040U
#define LLC_PFC_FLAG_PG             0x0080U     // Bulk voltage in regulation (RUN)

// PFC_STATUS.state (matches SMPS_STATE_e)
#define LLC_PFC_STATE_INIT          0U
#define LLC_PFC_STATE_STANDBY       1U
#define LLC_PFC_STATE_PREPARE       2U
#define LLC_PFC_STATE_SOFTSTART     3U
#define LLC_PFC_STATE_RUN           4U
#define LLC_PFC_STATE_STOPPING      5U
#define LLC_PFC_STATE_FAULT         6U

// LLC_STATUS.request flags (LLC -> PFC needs)
#define LLC_REQ_PFC_DERATE          0x0001U     // LLC asks PFC to limit power to powerLimitW
#define LLC_REQ_PFC_SHUTDOWN        0x0002U     // LLC asks PFC to stop
#define LLC_REQ_VOUT_REF_VALID      0x0004U     // voutRefReq field is meaningful

// SET_ENABLE / BOOT_ENTER magic
#define LLC_ENABLE_OFF              0x00U
#define LLC_ENABLE_ON               0x01U
#define LLC_BOOT_ENTER_MAGIC        0x544F4F42UL  // "BOOT" little-endian

// Firmware image granularity
#define LLC_BOOT_WRITE_DATA_MAX     128U        // Bytes per BOOT_WRITE
#define LLC_BOOT_WRITE_ALIGN        16U         // Address and length alignment

// Duty-coded discrete signals (RC4 / RD6): LLC_DUTY_* in llc_duty_def.h

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions
// Payload layouts, little-endian, packed (no padding by construction).
// Response payloads below start after the status byte.

// GET_INFO response, 16 bytes
typedef struct __attribute__((packed))
{
    uint8_t  protoMajor;
    uint8_t  protoMinor;
    uint8_t  fwMajor;
    uint8_t  fwMinor;
    uint16_t fwBuild;
    uint8_t  activePartition;       // 1 or 2
    uint8_t  bootCaps;              // bit0: in-app update, bit1: NVM implemented
    uint32_t imageCrc32;            // 0 if not computed
    uint32_t reserved;
} LLC_PROTO_INFO_T;

// GET_STATUS / LLC_STATUS response, 28 bytes
typedef struct __attribute__((packed))
{
    uint8_t  state;                 // LLC_PFC_STATE_*
    uint8_t  reserved0;
    uint16_t flags;                 // LLC_PFC_FLAG_*
    uint32_t faultLatched;          // FAULTLOG_FLAGS_e bitmask
    uint32_t faultLockedOut;
    uint16_t voutV10;               // Bulk voltage [0.1 V]
    uint16_t vacRmsV10;             // Grid RMS [0.1 V]
    uint16_t iinRmsA100;            // Input RMS current [0.01 A]
    uint16_t freqHz100;             // Grid frequency [0.01 Hz]
    uint16_t pinW;                  // Input power estimate [W]
    int16_t  tempGanC10;            // [0.1 degC]
    int16_t  tempInletC10;
    int16_t  tempMaxC10;            // Max of the remaining sensors
} LLC_PROTO_PFC_STATUS_T;

// LLC_STATUS request, 20 bytes
typedef struct __attribute__((packed))
{
    uint8_t  state;                 // LLC-defined state code
    uint8_t  reserved0;
    uint16_t request;               // LLC_REQ_*
    uint32_t faultFlags;            // LLC-defined fault bitmask
    uint16_t voutV100;              // LLC output voltage [0.01 V]
    uint16_t ioutA10;               // LLC output current [0.1 A]
    uint16_t poutW;                 // LLC output power [W]
    int16_t  tempMaxC10;            // [0.1 degC]
    uint16_t voutRefReq;            // Requested PFC bulk [0.1 V], if LLC_REQ_VOUT_REF_VALID
    uint16_t powerLimitW;           // Requested PFC power limit [W], if LLC_REQ_PFC_DERATE
} LLC_PROTO_LLC_STATUS_T;

// GET_LOAD_ECHO response, 8 bytes
typedef struct __attribute__((packed))
{
    uint16_t dutyPermille;          // Measured RD6 duty x 1000
    uint16_t freqHz;                // Measured RD6 frequency
    uint16_t loadW;                 // Decoded load power
    uint8_t  valid;
    uint8_t  reserved;
} LLC_PROTO_LOAD_ECHO_T;

// BOOT_ENTER request, 8 bytes
typedef struct __attribute__((packed))
{
    uint32_t magic;                 // LLC_BOOT_ENTER_MAGIC
    uint32_t imageSize;             // Bytes, multiple of LLC_BOOT_WRITE_ALIGN
} LLC_PROTO_BOOT_ENTER_T;

// BOOT_ENTER response, 8 bytes
typedef struct __attribute__((packed))
{
    uint8_t  targetPartition;       // Partition that will receive the image (1 or 2)
    uint8_t  writeMax;              // LLC_BOOT_WRITE_DATA_MAX
    uint8_t  writeAlign;            // LLC_BOOT_WRITE_ALIGN
    uint8_t  reserved;
    uint32_t imageMax;              // Largest accepted image [bytes]
} LLC_PROTO_BOOT_ENTER_RSP_T;

// BOOT_ERASE / BOOT_WRITE / BOOT_VERIFY response, 4 bytes
typedef struct __attribute__((packed))
{
    uint32_t progress;              // ERASE: bytes erased; WRITE: next expected offset; VERIFY: CRC computed
} LLC_PROTO_BOOT_PROGRESS_T;

// BOOT_WRITE request header, followed by data[LLC_BOOT_WRITE_DATA_MAX max]
typedef struct __attribute__((packed))
{
    uint32_t offset;                // Byte offset into the image
    uint32_t reserved;
} LLC_PROTO_BOOT_WRITE_T;

// BOOT_VERIFY request, 8 bytes
typedef struct __attribute__((packed))
{
    uint32_t imageSize;
    uint32_t imageCrc32;            // CRC-32/ISO-HDLC (poly 0xEDB88320 reflected, init/xorout 0xFFFFFFFF)
} LLC_PROTO_BOOT_VERIFY_T;

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* LLC_PROTOCOL_H */
