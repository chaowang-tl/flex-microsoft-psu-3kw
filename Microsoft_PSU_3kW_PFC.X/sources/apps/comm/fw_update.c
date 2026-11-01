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
* File Name       : fw_update.c
* Description     : PFC firmware update session over the LLC link (framework)
*
*   Session (docs/PFC_LLC_Comm_Protocol_EN.md, section 6):
*     IDLE --BOOT_ENTER--> ACTIVE --BOOT_ERASE--> ERASED --BOOT_WRITE--> WRITING
*     WRITING --BOOT_WRITE (in order)--> WRITING --BOOT_VERIFY--> VERIFIED
*     VERIFIED --BOOT_COMMIT--> switch boot partition and reset
*     any --BOOT_ABORT or 5 s without BOOT_*--> IDLE
*   The new image goes to the inactive dual-boot partition, so the running
*   firmware is never touched and a failed update leaves it in place.
*
*   This version implements the session rules (preconditions, ordering,
*   offsets, sizes). The flash primitives FwUpdateNvm* return
*   LLC_STS_NOT_IMPLEMENTED until the NVM driver and FICD_NOBTSWP are done.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/31
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#define FW_UPDATE_C

//------------------------------------------------------------------------------
// Include
#include <string.h>
#include "fw_update.h"
#include "../../scheduler/scheduler.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions
#define FW_UPDATE_NVM_IMPLEMENTED   0           // Set to 1 once FwUpdateNvm* are real

//------------------------------------------------------------------------------
// Public Variables
volatile FW_UPDATE_STATE_e g_fwUpdateState;
volatile uint32_t          g_fwUpdateNextOffset;

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes
static uint8_t FwUpdateEnter(const uint8_t *req, uint8_t reqLen, bool pfcStopped,
                      uint8_t *rspData, uint8_t *rspDataLen);
static uint8_t FwUpdateWrite(const uint8_t *req, uint8_t reqLen, uint8_t *rspData, uint8_t *rspDataLen);
static uint8_t FwUpdateVerify(const uint8_t *req, uint8_t reqLen, uint8_t *rspData, uint8_t *rspDataLen);
static void    FwUpdateProgress(uint32_t value, uint8_t *rspData, uint8_t *rspDataLen);
static uint8_t FwUpdateNvmErase(void);
static uint8_t FwUpdateNvmWrite(uint32_t offset, const uint8_t *data, uint8_t len);
static uint8_t FwUpdateNvmCrc32(uint32_t size, uint32_t *crc);
static uint8_t FwUpdateNvmCommit(void);
//------------------------------------------------------------------------------
// Private Variables
uint32_t fwImageSize;
uint16_t fwLastActivityTick;              // Since the last BOOT_* command

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : FW_UPDATE_Init
* Description     : Reset the update session
* Purpose         : Start in IDLE after power-up
* Input           : None
* Return          : None
* Calls           : None
* Called by       : LLC_LINK_Init
* Note            : None
*******************************************************************************/
void FW_UPDATE_Init(void)
{
    g_fwUpdateState      = FW_UPDATE_IDLE;
    g_fwUpdateNextOffset = 0UL;
    fwImageSize          = 0UL;
    fwLastActivityTick   = g_schedulerTickCount;
}

/*******************************************************************************
* Public Function : FW_UPDATE_Handle
* Description     : Execute one BOOT_* request
* Purpose         : Enforce the update session rules
* Input           : cmd        - LLC_CMD_BOOT_*
*                   req        - request payload
*                   reqLen     - request payload length
*                   pfcStopped - PFC is stopped and held off (BOOT_ENTER
*                                precondition, ensured by the caller)
*                   rspData    - response data buffer (after the status byte)
*                   rspDataLen - out: response data length
* Return          : LLC_STS_* status code
* Calls           : FwUpdateEnter, FwUpdateWrite, FwUpdateVerify,
*                   FwUpdateNvmErase, FwUpdateNvmCommit, FwUpdateProgress
* Called by       : LinkDispatch (llc_link.c, main loop)
* Note            : A failed erase or write drops back to ACTIVE, so the
*                   image must be erased and written again
*******************************************************************************/
uint8_t FW_UPDATE_Handle(uint8_t cmd, const uint8_t *req, uint8_t reqLen,
                         bool pfcStopped, uint8_t *rspData, uint8_t *rspDataLen)
{
    uint8_t sts;

    *rspDataLen = 0U;

    if (cmd == LLC_CMD_BOOT_ENTER)
    {
        return FwUpdateEnter(req, reqLen, pfcStopped, rspData, rspDataLen);
    }

    if (cmd == LLC_CMD_BOOT_ABORT)
    {
        if (reqLen != 0U) { return LLC_STS_BAD_LEN; }
        FW_UPDATE_Init();
        return LLC_STS_OK;
    }

    if (g_fwUpdateState == FW_UPDATE_IDLE)
    {
        return LLC_STS_SEQUENCE_ERR;
    }

    switch (cmd)
    {
        case LLC_CMD_BOOT_ERASE:
            if (reqLen != 0U)
            {
                return LLC_STS_BAD_LEN;
            }
            sts = FwUpdateNvmErase();
            g_fwUpdateNextOffset = 0UL;
            g_fwUpdateState = (sts == LLC_STS_OK) ? FW_UPDATE_ERASED : FW_UPDATE_ACTIVE;
            if (sts == LLC_STS_OK)
            {
                FwUpdateProgress(fwImageSize, rspData, rspDataLen);
            }
            return sts;

        case LLC_CMD_BOOT_WRITE:
            return FwUpdateWrite(req, reqLen, rspData, rspDataLen);

        case LLC_CMD_BOOT_VERIFY:
            return FwUpdateVerify(req, reqLen, rspData, rspDataLen);

        case LLC_CMD_BOOT_COMMIT:
            if (reqLen != 0U)
            {
                return LLC_STS_BAD_LEN;
            }
            if (g_fwUpdateState != FW_UPDATE_VERIFIED)
            {
                return LLC_STS_SEQUENCE_ERR;
            }
            return FwUpdateNvmCommit();

        default:
            return LLC_STS_UNKNOWN_CMD;
    }
}

/*******************************************************************************
* Public Function : FW_UPDATE_Tick1ms
* Description     : Session inactivity timeout
* Purpose         : Leave a stale session so the PFC can be enabled again
* Input           : None
* Return          : None
* Calls           : FW_UPDATE_Init
* Called by       : LLC_LINK_Tick1ms
* Note            : The PFC stays disabled after the timeout; the LLC must
*                   send SET_ENABLE again
*******************************************************************************/
void FW_UPDATE_Tick1ms(void)
{
    if (g_fwUpdateState == FW_UPDATE_IDLE)
    {
        return;
    }

    if ((uint16_t)(g_schedulerTickCount - fwLastActivityTick)
        >= FW_UPDATE_SESSION_TIMEOUT_MS * BOARD_SLOW_TICKS_PER_MS)
    {
        FW_UPDATE_Init();
    }
}

/*******************************************************************************
* Public Function : FW_UPDATE_IsActive
* Description     : True while an update session is open
* Purpose         : Block SET_ENABLE ON during an update
* Input           : None
* Return          : true if not IDLE
* Calls           : None
* Called by       : LinkDispatch, LinkFillPfcStatus (llc_link.c)
* Note            : None
*******************************************************************************/
bool FW_UPDATE_IsActive(void)
{
    return (g_fwUpdateState != FW_UPDATE_IDLE);
}

/*******************************************************************************
* Public Function : FW_UPDATE_NvmImplemented
* Description     : True once flash erase/write/commit are implemented
* Purpose         : Report the update capability in GET_INFO
* Input           : None
* Return          : FW_UPDATE_NVM_IMPLEMENTED
* Calls           : None
* Called by       : LinkFillInfo (llc_link.c)
* Note            : None
*******************************************************************************/
bool FW_UPDATE_NvmImplemented(void)
{
    return (FW_UPDATE_NVM_IMPLEMENTED != 0);
}

/*******************************************************************************
* Public Function : FW_UPDATE_ActivePartition
* Description     : Dual-boot partition the firmware is running from
* Purpose         : Report in GET_INFO; the image is written to the other one
* Input           : None
* Return          : 1 or 2
* Calls           : None
* Called by       : LinkFillInfo (llc_link.c), FwUpdateEnter
* Note            : NVMCON.P2ACTIV = 1 when partition 2 is active
*******************************************************************************/
uint8_t FW_UPDATE_ActivePartition(void)
{
    return NVMCONbits.P2ACTIV ? 2U : 1U;
}

/*******************************************************************************
* Public Function : FW_UPDATE_KeepAlive
* Description     : Refresh an active session after a successful command or retry
* Purpose         : Refresh an active session after a successful command or retry
* Input           : None
* Return          : None
* Calls           : FW_UPDATE_IsActive
* Called by       : LinkDispatch, LinkHandleFrame
* Note            : User application code; preserves MCC ownership
*******************************************************************************/
void FW_UPDATE_KeepAlive(void)
{
    if (FW_UPDATE_IsActive()) { fwLastActivityTick = g_schedulerTickCount; }
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation
/*******************************************************************************
* Private Function : FwUpdateEnter
* Description     : BOOT_ENTER: open a session
* Purpose         : Check magic, image size and that the PFC is stopped
* Input           : req, reqLen, pfcStopped, rspData, rspDataLen - see
*                   FW_UPDATE_Handle
* Return          : LLC_STS_* status code
* Calls           : FW_UPDATE_ActivePartition
* Called by       : FW_UPDATE_Handle
* Note            : The "PFC stopped" precondition is checked by the caller
*                   (llc_link stops the PFC and passes pfcStopped), so this
*                   module does not depend on the state machine.
*                   Re-entering restarts the session
*******************************************************************************/
static uint8_t FwUpdateEnter(const uint8_t *req, uint8_t reqLen, bool pfcStopped,
                      uint8_t *rspData, uint8_t *rspDataLen)
{
    LLC_PROTO_BOOT_ENTER_T     enter;
    LLC_PROTO_BOOT_ENTER_RSP_T rsp;

    if (reqLen != sizeof(enter))
    {
        return LLC_STS_BAD_LEN;
    }
    memcpy(&enter, req, sizeof(enter));

    if (enter.magic != LLC_BOOT_ENTER_MAGIC)
    {
        return LLC_STS_BAD_PARAM;
    }

    if ((enter.imageSize == 0UL) || (enter.imageSize > FW_UPDATE_IMAGE_MAX)
        || ((enter.imageSize % LLC_BOOT_WRITE_ALIGN) != 0UL))
    {
        return LLC_STS_BAD_PARAM;
    }

    if (!pfcStopped)
    {
        return LLC_STS_NOT_ALLOWED;
    }

    fwImageSize          = enter.imageSize;
    g_fwUpdateNextOffset = 0UL;
    g_fwUpdateState      = FW_UPDATE_ACTIVE;

    rsp.targetPartition = (FW_UPDATE_ActivePartition() == 1U) ? 2U : 1U;
    rsp.writeMax        = LLC_BOOT_WRITE_DATA_MAX;
    rsp.writeAlign      = LLC_BOOT_WRITE_ALIGN;
    rsp.reserved        = 0U;
    rsp.imageMax        = FW_UPDATE_IMAGE_MAX;
    memcpy(rspData, &rsp, sizeof(rsp));
    *rspDataLen = sizeof(rsp);

    return LLC_STS_OK;
}

/*******************************************************************************
* Private Function : FwUpdateWrite
* Description     : BOOT_WRITE: program one block at the next offset
* Purpose         : Enforce in-order, aligned, in-range blocks
* Input           : req, reqLen, rspData, rspDataLen - see FW_UPDATE_Handle
* Return          : LLC_STS_* status code
* Calls           : FwUpdateNvmWrite, FwUpdateProgress
* Called by       : FW_UPDATE_Handle
* Note            : Retransmissions are answered from the link response
*                   cache, so an offset behind the expected one is an error
*******************************************************************************/
static uint8_t FwUpdateWrite(const uint8_t *req, uint8_t reqLen, uint8_t *rspData, uint8_t *rspDataLen)
{
    LLC_PROTO_BOOT_WRITE_T hdr;
    uint8_t dataLen;
    uint8_t sts;

    if ((reqLen <= sizeof(hdr)) || (reqLen > (sizeof(hdr) + LLC_BOOT_WRITE_DATA_MAX)))
    {
        return LLC_STS_BAD_LEN;
    }
    dataLen = (uint8_t)(reqLen - sizeof(hdr));
    if ((dataLen % LLC_BOOT_WRITE_ALIGN) != 0U)
    {
        return LLC_STS_BAD_LEN;
    }
    memcpy(&hdr, req, sizeof(hdr));

    if ((g_fwUpdateState != FW_UPDATE_ERASED) && (g_fwUpdateState != FW_UPDATE_WRITING))
    {
        return LLC_STS_SEQUENCE_ERR;
    }
    if (hdr.offset != g_fwUpdateNextOffset)
    {
        return LLC_STS_SEQUENCE_ERR;
    }
    if ((hdr.offset + dataLen) > fwImageSize)
    {
        return LLC_STS_BAD_PARAM;
    }

    sts = FwUpdateNvmWrite(hdr.offset, &req[sizeof(hdr)], dataLen);
    if (sts != LLC_STS_OK)
    {
        g_fwUpdateState = FW_UPDATE_ACTIVE;
        return sts;
    }

    g_fwUpdateNextOffset += dataLen;
    g_fwUpdateState = FW_UPDATE_WRITING;
    FwUpdateProgress(g_fwUpdateNextOffset, rspData, rspDataLen);
    return LLC_STS_OK;
}

/*******************************************************************************
* Private Function : FwUpdateVerify
* Description     : BOOT_VERIFY: compare the written image CRC32
* Purpose         : Accept the image only if it is complete and intact
* Input           : req, reqLen, rspData, rspDataLen - see FW_UPDATE_Handle
* Return          : LLC_STS_* status code
* Calls           : FwUpdateNvmCrc32, FwUpdateProgress
* Called by       : FW_UPDATE_Handle
* Note            : The response carries the CRC the PFC computed
*******************************************************************************/
static uint8_t FwUpdateVerify(const uint8_t *req, uint8_t reqLen, uint8_t *rspData, uint8_t *rspDataLen)
{
    LLC_PROTO_BOOT_VERIFY_T verify;
    uint32_t crc = 0UL;
    uint8_t  sts;

    if (reqLen != sizeof(verify))
    {
        return LLC_STS_BAD_LEN;
    }
    memcpy(&verify, req, sizeof(verify));

    if (g_fwUpdateState != FW_UPDATE_WRITING)
    {
        return LLC_STS_SEQUENCE_ERR;
    }
    if ((verify.imageSize != fwImageSize) || (g_fwUpdateNextOffset != fwImageSize))
    {
        return LLC_STS_BAD_PARAM;
    }

    sts = FwUpdateNvmCrc32(fwImageSize, &crc);
    if (sts != LLC_STS_OK)
    {
        return sts;
    }

    FwUpdateProgress(crc, rspData, rspDataLen);
    if (crc != verify.imageCrc32)
    {
        return LLC_STS_VERIFY_ERR;
    }

    g_fwUpdateState = FW_UPDATE_VERIFIED;
    return LLC_STS_OK;
}

/*******************************************************************************
* Private Function : FwUpdateProgress
* Description     : Put a 4-byte progress value into the response data
* Purpose         : Common BOOT_ERASE / WRITE / VERIFY response layout
* Input           : value      - LLC_PROTO_BOOT_PROGRESS_T.progress
*                   rspData    - response data buffer
*                   rspDataLen - out: response data length
* Return          : None
* Calls           : None
* Called by       : FW_UPDATE_Handle, FwUpdateWrite, FwUpdateVerify
* Note            : None
*******************************************************************************/
static void FwUpdateProgress(uint32_t value, uint8_t *rspData, uint8_t *rspDataLen)
{
    LLC_PROTO_BOOT_PROGRESS_T progress;

    progress.progress = value;
    memcpy(rspData, &progress, sizeof(progress));
    *rspDataLen = sizeof(progress);
}

/*******************************************************************************
* Private Function : FwUpdateNvmErase
* Description     : Erase the inactive dual-boot partition
* Purpose         : Flash primitive for BOOT_ERASE
* Input           : None
* Return          : LLC_STS_NOT_IMPLEMENTED
* Calls           : None
* Called by       : FW_UPDATE_Handle
* Note            : TODO: page-erase the inactive partition with NVMCON;
*                   inactive-partition erase runs while the PFC code keeps
*                   executing (RWW), but must finish within the link
*                   response timeout or answer LLC_STS_BUSY and continue
*******************************************************************************/
static uint8_t FwUpdateNvmErase(void)
{
    return LLC_STS_NOT_IMPLEMENTED;
}

/*******************************************************************************
* Private Function : FwUpdateNvmWrite
* Description     : Program one block of the inactive partition
* Purpose         : Flash primitive for BOOT_WRITE
* Input           : offset - byte offset in the image
*                   data   - block data
*                   len    - block length (multiple of LLC_BOOT_WRITE_ALIGN)
* Return          : LLC_STS_NOT_IMPLEMENTED
* Calls           : None
* Called by       : FwUpdateWrite
* Note            : TODO: map offset to the inactive-partition address and
*                   program quad-words, read back to check
*******************************************************************************/
static uint8_t FwUpdateNvmWrite(uint32_t offset, const uint8_t *data, uint8_t len)
{
    (void)offset;
    (void)data;
    (void)len;
    return LLC_STS_NOT_IMPLEMENTED;
}

/*******************************************************************************
* Private Function : FwUpdateNvmCrc32
* Description     : CRC32 of the image written to the inactive partition
* Purpose         : Flash primitive for BOOT_VERIFY
* Input           : size - image size [bytes]
*                   crc  - out: CRC-32/ISO-HDLC of the image
* Return          : LLC_STS_NOT_IMPLEMENTED
* Calls           : None
* Called by       : FwUpdateVerify
* Note            : TODO: read the inactive partition back and compute the CRC
*******************************************************************************/
static uint8_t FwUpdateNvmCrc32(uint32_t size, uint32_t *crc)
{
    (void)size;
    *crc = 0UL;
    return LLC_STS_NOT_IMPLEMENTED;
}

/*******************************************************************************
* Private Function : FwUpdateNvmCommit
* Description     : Make the new partition the boot partition and reset
* Purpose         : Flash primitive for BOOT_COMMIT
* Input           : None
* Return          : LLC_STS_NOT_IMPLEMENTED
* Calls           : None
* Called by       : FW_UPDATE_Handle
* Note            : TODO: needs FICD_NOBTSWP enabled. Program the inactive
*                   partition's boot sequence number higher than the active
*                   one, send the OK response, wait for UART TX to drain, then
*                   reset (or BOOTSWP)
*******************************************************************************/
static uint8_t FwUpdateNvmCommit(void)
{
    return LLC_STS_NOT_IMPLEMENTED;
}

/******************************************************************************/
#undef FW_UPDATE_C
