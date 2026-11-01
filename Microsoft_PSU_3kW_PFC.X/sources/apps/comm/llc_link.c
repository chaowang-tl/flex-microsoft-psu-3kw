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
* File Name       : llc_link.c
* Description     : PFC side of the PFC <-> LLC UART1 link (slave)
*
*   Specification: docs/PFC_LLC_Comm_Protocol_EN.md (_CN for Chinese).
*   LLC is the master and sends requests; the PFC answers each one with
*   CMD | 0x80 and the same SEQ. UART1: 115200 8N1, RC8 = RX, RC9 = TX,
*   no UART interrupts; the register access is in drivers/uart_link.
*
*   Contexts:
*     T1 ISR (100 us)  LLC_LINK_ByteService: moves bytes between the 8-deep
*                      hardware FIFOs and two software rings. Never waits.
*     Main loop        LLC_LINK_Task: frame parser, CRC, command handling,
*                      response framing into the TX ring.
*     APP_TASKS_Task1ms         LLC_LINK_Tick1ms: inter-byte and link timeouts.
*   Each ring has one writer per index (RX: head by T1, tail by main loop;
*   TX the other way round). Indexes are single 32-bit words, so no lock.
*
*   Duplicate requests (identical SEQ, CMD, LEN and payload within 100 ms, i.e. a master
*   retry after a lost response) are answered from a cached copy of the
*   last response and are not executed again.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/31
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#define LLC_LINK_C

//------------------------------------------------------------------------------
// Include
#include <string.h>
#include "llc_link.h"
#include "fw_update.h"
#include "../control/smps_sm.h"
#include "../control/smps_control.h"
#include "../control/pfc_loop.h"
#include "../fault/fault.h"
#include "../meas/grid_meas.h"
#include "../pll/sogi_pll.h"
#include "../../drivers/llc_signal/llc_signal.h"
#include "../../config/app_config.h"
#include "../../drivers/uart_link/uart_link.h"
#include "../../scheduler/scheduler.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions
#define LLC_LINK_RX_RING_SIZE       256U        // Power of 2
#define LLC_LINK_TX_RING_SIZE       256U        // Power of 2, >= one full frame
#define LLC_LINK_RX_MASK            (LLC_LINK_RX_RING_SIZE - 1U)
#define LLC_LINK_CACHE_TICKS (100U * BOARD_SLOW_TICKS_PER_MS)
#define LLC_LINK_LOST_TICKS (LLC_PROTO_LINK_LOST_MS * BOARD_SLOW_TICKS_PER_MS)
#define LLC_LINK_GAP_TICKS (LLC_PROTO_INTERBYTE_MS * BOARD_SLOW_TICKS_PER_MS)
#define LLC_LINK_TX_MASK            (LLC_LINK_TX_RING_SIZE - 1U)

// Compiler barrier: keeps the non-volatile ring stores ahead of the volatile
// head store that publishes them to the other context
#define LINK_BARRIER()              __asm__ volatile ("" ::: "memory")

// Wire format of the payloads and of the state code (llc_protocol.h)
_Static_assert(sizeof(LLC_PROTO_INFO_T)           == 16U, "INFO payload");
_Static_assert(sizeof(LLC_PROTO_PFC_STATUS_T)     == 28U, "PFC_STATUS payload");
_Static_assert(sizeof(LLC_PROTO_LLC_STATUS_T)     == 20U, "LLC_STATUS payload");
_Static_assert(sizeof(LLC_PROTO_LOAD_ECHO_T)      == 8U,  "LOAD_ECHO payload");
_Static_assert(sizeof(LLC_PROTO_BOOT_ENTER_T)     == 8U,  "BOOT_ENTER payload");
_Static_assert(sizeof(LLC_PROTO_BOOT_ENTER_RSP_T) == 8U,  "BOOT_ENTER response");
_Static_assert(sizeof(LLC_PROTO_BOOT_WRITE_T)     == 8U,  "BOOT_WRITE header");
_Static_assert(sizeof(LLC_PROTO_BOOT_VERIFY_T)    == 8U,  "BOOT_VERIFY payload");
_Static_assert(LLC_PROTO_PAYLOAD_MAX >= sizeof(LLC_PROTO_BOOT_WRITE_T) + LLC_BOOT_WRITE_DATA_MAX, "payload max");
_Static_assert(LLC_PROTO_PAYLOAD_MAX >= 1U + sizeof(LLC_PROTO_PFC_STATUS_T), "status response fits");
_Static_assert((uint8_t)SMPS_STATE_INIT      == LLC_PFC_STATE_INIT,      "state code");
_Static_assert((uint8_t)SMPS_STATE_STANDBY   == LLC_PFC_STATE_STANDBY,   "state code");
_Static_assert((uint8_t)SMPS_STATE_PREPARE   == LLC_PFC_STATE_PREPARE,   "state code");
_Static_assert((uint8_t)SMPS_STATE_SOFTSTART == LLC_PFC_STATE_SOFTSTART, "state code");
_Static_assert((uint8_t)SMPS_STATE_RUN       == LLC_PFC_STATE_RUN,       "state code");
_Static_assert((uint8_t)SMPS_STATE_STOPPING  == LLC_PFC_STATE_STOPPING,  "state code");
_Static_assert((uint8_t)SMPS_STATE_FAULT     == LLC_PFC_STATE_FAULT,     "state code");

//------------------------------------------------------------------------------
// Public Variables
volatile bool          g_llcLinkUp;
LLC_PROTO_LLC_STATUS_T g_llcStatus;
volatile bool          g_llcStatusValid;
LLC_LINK_STATS_T       g_llcLinkStats;

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions
typedef enum
{
    LINK_RX_SOF = 0,
    LINK_RX_LEN,
    LINK_RX_SEQ,
    LINK_RX_CMD,
    LINK_RX_PAYLOAD,
    LINK_RX_CRC_L,
    LINK_RX_CRC_H
} LINK_RX_STATE_e;

//------------------------------------------------------------------------------
// Private Function Prototypes
static void    LinkParseByte(uint8_t byte);
static void    LinkHandleFrame(void);
static uint8_t LinkDispatch(uint8_t cmd, const uint8_t *req, uint8_t reqLen,
                     uint8_t *rspData, uint8_t *rspDataLen);
static void    LinkSendResponse(uint8_t seq, uint8_t cmd, uint8_t status,
                         const uint8_t *data, uint8_t dataLen);
static void    LinkSendCached(void);
static bool    LinkTxPut(const uint8_t *data, uint16_t len);
static void    LinkFillInfo(uint8_t *rspData, uint8_t *rspDataLen);
static void    LinkFillPfcStatus(uint8_t *rspData, uint8_t *rspDataLen);
static void    LinkFillLoadEcho(uint8_t *rspData, uint8_t *rspDataLen);
static bool    LinkPfcStopForBoot(uint8_t cmd);
static uint16_t LinkToU16(float value);
static int16_t  LinkToS16(float value);
//------------------------------------------------------------------------------
// Private Variables
// Rings (T1 <-> main loop)
uint8_t           linkRxRing[LLC_LINK_RX_RING_SIZE];
uint16_t          linkRxTicks[LLC_LINK_RX_RING_SIZE];
volatile bool     linkRxError;
uint16_t          linkParsedTick;
uint16_t          linkLastRxTick;
uint16_t          linkLastStatusTick;
uint16_t          linkLastCacheTick;
uint8_t           linkLastReqLen;
uint8_t           linkLastReq[LLC_PROTO_PAYLOAD_MAX];
volatile uint32_t linkRxHead;               // Written by T1
volatile uint32_t linkRxTail;               // Written by main loop
uint8_t           linkTxRing[LLC_LINK_TX_RING_SIZE];
volatile uint32_t linkTxHead;               // Written by main loop
volatile uint32_t linkTxTail;               // Written by T1

// Parser (main loop)
LINK_RX_STATE_e linkRxState;
uint8_t         linkRxLen;
uint8_t         linkRxSeq;
uint8_t         linkRxCmd;
uint8_t         linkRxIdx;
uint16_t        linkRxCrc;
uint8_t         linkRxPayload[LLC_PROTO_PAYLOAD_MAX];

// Last response, resent on a duplicate request
uint8_t  linkLastFrame[LLC_PROTO_FRAME_MAX];
uint16_t linkLastFrameLen;
uint8_t  linkLastSeq;
uint8_t  linkLastCmd;
bool     linkLastValid;

// Timers (APP_TASKS_Task1ms)

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation
/*******************************************************************************
* Public Function : LLC_LINK_Init
* Description     : Reset the link state and flush UART1
* Purpose         : Start the PFC <-> LLC link
* Input           : None
* Return          : None
* Calls           : UART_LINK_Flush, FW_UPDATE_Init
* Called by       : main (before global interrupts are enabled)
* Note            : UART1 itself is configured and enabled by MCC
*                   (UART1_Initialize); only the FIFOs and flags are cleared.
*                   The RX flush is bounded so a noisy line cannot hang start-up
*******************************************************************************/
void LLC_LINK_Init(void)
{
    UART_LINK_Flush();

    linkRxHead = 0UL;
    linkRxTail = 0UL;
    linkTxHead = 0UL;
    linkTxTail = 0UL;

    linkRxError = false;
    linkParsedTick = g_schedulerTickCount;
    linkLastRxTick = (uint16_t)(g_schedulerTickCount - LLC_LINK_LOST_TICKS);
    linkLastStatusTick = linkLastRxTick;
    linkRxState   = LINK_RX_SOF;
    linkLastValid = false;


    g_llcLinkUp      = false;
    g_llcStatusValid = false;
    memset(&g_llcStatus, 0, sizeof(g_llcStatus));
    memset(&g_llcLinkStats, 0, sizeof(g_llcLinkStats));

    FW_UPDATE_Init();
}

/*******************************************************************************
* Public Function : LLC_LINK_ByteService
* Description     : Move bytes between the UART1 FIFOs and the software rings
* Purpose         : Polled UART driver, no UART interrupts
* Input           : None
* Return          : None
* Calls           : UART_LINK_* (uart_link.h)
* Called by       : _T1Interrupt (100 us)
* Note            : Non-blocking: each loop stops as soon as the hardware FIFO
*                   is empty (RX) or full (TX), at most 8 iterations each.
*                   115200 baud moves ~1.15 bytes per 100 us, so the 8-byte
*                   FIFOs give ~7 ticks of margin on RX and never run dry on TX
*******************************************************************************/
void LLC_LINK_ByteService(void)
{
    uint32_t head = linkRxHead;
    uint32_t tail = linkTxTail;
    uint8_t  n;

    if (UART_LINK_RxOverrunTake())
    {
        linkRxError = true;
        g_llcLinkStats.rxOverruns++;
    }
    if (UART_LINK_FramingErrorTake())
    {
        linkRxError = true;
        g_llcLinkStats.framingErrors++;
    }

    for (n = 0U; (n < UART_LINK_FIFO_DEPTH) && UART_LINK_RxReady(); n++)
    {
        uint8_t byte = UART_LINK_RxByte();

        if ((head - linkRxTail) < LLC_LINK_RX_RING_SIZE)
        {
            linkRxTicks[head & LLC_LINK_RX_MASK] = g_schedulerTickCount;
            linkRxRing[head & LLC_LINK_RX_MASK] = byte;
            head++;
        }
        else
        {
            linkRxError = true;
        g_llcLinkStats.rxOverruns++;
        }
    }
    LINK_BARRIER();                 // Ring data stored before the head is published
    linkRxHead = head;

    for (n = 0U; (n < UART_LINK_FIFO_DEPTH) && UART_LINK_TxReady() && (tail != linkTxHead); n++)
    {
        UART_LINK_TxByte(linkTxRing[tail & LLC_LINK_TX_MASK]);
        tail++;
    }
    linkTxTail = tail;
}

/*******************************************************************************
* Public Function : LLC_LINK_Task
* Description     : Parse received bytes and answer complete requests
* Purpose         : Protocol layer of the link
* Input           : None
* Return          : None
* Calls           : LinkParseByte
* Called by       : APP_TASKS_Always (main loop)
* Note            : Runs to the end of the RX ring each call
*******************************************************************************/
void LLC_LINK_Task(void)
{
    uint32_t head = linkRxHead;
    LLC_LINK_Tick1ms(); // Real elapsed ticks; also expire sessions before retries.
    if (linkRxError)
    {
        __builtin_disable_interrupts();
        linkRxTail = linkRxHead;
        linkRxError = false;
        __builtin_enable_interrupts();
        linkRxState = LINK_RX_SOF;
        return;
    }
    LINK_BARRIER();
    while (linkRxTail != head)
    {
        uint32_t index = linkRxTail & LLC_LINK_RX_MASK;
        uint8_t byte = linkRxRing[index];
        uint16_t tick = linkRxTicks[index];
        if (linkRxState != LINK_RX_SOF
            && (uint16_t)(tick - linkParsedTick) >= LLC_LINK_GAP_TICKS)
        {
            linkRxState = LINK_RX_SOF;
            g_llcLinkStats.gapDrops++;
        }
        linkParsedTick = tick;
        LINK_BARRIER(); // Finish reading the slot before T1 can reuse it.
        linkRxTail++;
        LinkParseByte(byte);
    }
}

/*******************************************************************************
* Public Function : LLC_LINK_Tick1ms
* Description     : Inter-byte, link-lost and LLC_STATUS age timers
* Purpose         : Drop partial frames and publish g_llcLinkUp / g_llcStatusValid
* Input           : None
* Return          : None
* Calls           : FW_UPDATE_Tick1ms
* Called by       : APP_TASKS_Task1ms (main loop)
* Note            : Same context as LLC_LINK_Task, so the parser state is safe
*                   to reset here
*******************************************************************************/
void LLC_LINK_Tick1ms(void)
{
    uint16_t now = g_schedulerTickCount;
    if (linkRxTail == linkRxHead && linkRxState != LINK_RX_SOF
        && (uint16_t)(now - linkParsedTick) >= LLC_LINK_GAP_TICKS)
    {
        linkRxState = LINK_RX_SOF;
        g_llcLinkStats.gapDrops++;
    }
    g_llcLinkUp = g_llcLinkUp && (uint16_t)(now - linkLastRxTick) < LLC_LINK_LOST_TICKS;
    g_llcStatusValid = g_llcStatusValid && (uint16_t)(now - linkLastStatusTick) < LLC_LINK_LOST_TICKS;
    if (linkLastValid && (uint16_t)(now - linkLastCacheTick) >= LLC_LINK_CACHE_TICKS)
    {
        linkLastValid = false;
    }
    FW_UPDATE_Tick1ms();
}

/*******************************************************************************
* Public Function : LLC_LINK_TxIdle
* Description     : True when the TX ring and the UART shift register are empty
* Purpose         : Let BOOT_COMMIT wait for its response before resetting
* Input           : None
* Return          : true if nothing is left to send
* Calls           : UART_LINK_TxDone
* Called by       : Reserved for the BOOT_COMMIT reset (FwUpdateNvmCommit TODO)
* Note            : UART_LINK_TxDone covers the shift register
*******************************************************************************/
bool LLC_LINK_TxIdle(void)
{
    return (linkTxTail == linkTxHead) && UART_LINK_TxDone();
}

/*******************************************************************************
* Public Function : LLC_LINK_Crc16
* Description     : CRC16/Modbus, bitwise
* Purpose         : Frame check (same algorithm as the LLC Modbus RTU stack)
* Input           : data - bytes
*                   len  - number of bytes
*                   crc  - running CRC, LLC_PROTO_CRC_INIT to start
* Return          : updated CRC
* Calls           : None
* Called by       : LinkParseByte, LinkSendResponse
* Note            : Check value: "123456789" -> 0x4B37
*******************************************************************************/
uint16_t LLC_LINK_Crc16(const uint8_t *data, uint16_t len, uint16_t crc)
{
    uint16_t i;
    uint8_t  bit;

    for (i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            if (crc & 0x0001U)
            {
                crc = (uint16_t)((crc >> 1) ^ LLC_PROTO_CRC_POLY);
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation
/*******************************************************************************
* Private Function : LinkParseByte
* Description     : Frame parser state machine, one byte at a time
* Purpose         : Assemble SOF | LEN | SEQ | CMD | PAYLOAD | CRC16
* Input           : byte - received byte
* Return          : None
* Calls           : LLC_LINK_Crc16, LinkHandleFrame
* Called by       : LLC_LINK_Task
* Note            : A frame with a bad CRC is dropped silently (the master
*                   retries on timeout); a too-long LEN resyncs on the next SOF
*******************************************************************************/
static void LinkParseByte(uint8_t byte)
{
    switch (linkRxState)
    {
        case LINK_RX_SOF:
            if (byte == LLC_PROTO_SOF)
            {
                linkRxCrc   = LLC_PROTO_CRC_INIT;
                linkRxState = LINK_RX_LEN;
            }
            break;

        case LINK_RX_LEN:
            if (byte > LLC_PROTO_PAYLOAD_MAX)
            {
                g_llcLinkStats.lenErrors++;
                linkRxState = (byte == LLC_PROTO_SOF) ? LINK_RX_LEN : LINK_RX_SOF;
                break;
            }
            linkRxLen   = byte;
            linkRxCrc   = LLC_LINK_Crc16(&byte, 1U, linkRxCrc);
            linkRxState = LINK_RX_SEQ;
            break;

        case LINK_RX_SEQ:
            linkRxSeq   = byte;
            linkRxCrc   = LLC_LINK_Crc16(&byte, 1U, linkRxCrc);
            linkRxState = LINK_RX_CMD;
            break;

        case LINK_RX_CMD:
            linkRxCmd   = byte;
            linkRxCrc   = LLC_LINK_Crc16(&byte, 1U, linkRxCrc);
            linkRxIdx   = 0U;
            linkRxState = (linkRxLen == 0U) ? LINK_RX_CRC_L : LINK_RX_PAYLOAD;
            break;

        case LINK_RX_PAYLOAD:
            linkRxPayload[linkRxIdx++] = byte;
            linkRxCrc = LLC_LINK_Crc16(&byte, 1U, linkRxCrc);
            if (linkRxIdx >= linkRxLen)
            {
                linkRxState = LINK_RX_CRC_L;
            }
            break;

        case LINK_RX_CRC_L:
            linkRxCrc  ^= byte;                 // Low byte must cancel out
            linkRxState = LINK_RX_CRC_H;
            break;

        case LINK_RX_CRC_H:
        default:
            linkRxState = LINK_RX_SOF;
            if ((linkRxCrc ^ ((uint16_t)byte << 8)) == 0U)
            {
                LinkHandleFrame();
            }
            else
            {
                g_llcLinkStats.crcErrors++;
            }
            break;
    }
}

/*******************************************************************************
* Private Function : LinkHandleFrame
* Description     : Act on one CRC-checked request
* Purpose         : Duplicate detection, dispatch, response
* Input           : None (linkRx* parser fields)
* Return          : None
* Calls           : LinkSendCached, LinkDispatch, LinkSendResponse
* Called by       : LinkParseByte
* Note            : Responses (CMD bit 7 set) from another node are ignored
*******************************************************************************/
static void LinkHandleFrame(void)
{
    uint8_t rspData[LLC_PROTO_PAYLOAD_MAX - 1U];
    uint8_t rspDataLen = 0U;
    uint8_t status;

    g_llcLinkStats.rxFrames++;

    if (linkRxCmd & LLC_PROTO_RSP_FLAG)
    {
        return;
    }

    linkLastRxTick = linkParsedTick;
    g_llcLinkUp = (uint16_t)(g_schedulerTickCount - linkLastRxTick) < LLC_LINK_LOST_TICKS;
    if (linkLastValid && (linkRxSeq == linkLastSeq) && (linkRxCmd == linkLastCmd)
        && linkRxLen == linkLastReqLen && memcmp(linkRxPayload, linkLastReq, linkRxLen) == 0)
    {
        g_llcLinkStats.retries++;
        if (linkLastFrame[4] == LLC_STS_OK)
        {
            if (linkRxCmd == LLC_CMD_LLC_STATUS)
            {
                linkLastStatusTick = linkParsedTick;
                g_llcStatusValid = (uint16_t)(g_schedulerTickCount - linkLastStatusTick) < LLC_LINK_LOST_TICKS;
            }
            if (linkRxCmd >= LLC_CMD_BOOT_ENTER && linkRxCmd <= LLC_CMD_BOOT_ABORT)
            {
                FW_UPDATE_KeepAlive();
            }
        }
        LinkSendCached();
        return;
    }

    status = LinkDispatch(linkRxCmd, linkRxPayload, linkRxLen, rspData, &rspDataLen);
    if (status != LLC_STS_OK)
    {
        rspDataLen = 0U;
    }

    LinkSendResponse(linkRxSeq, linkRxCmd, status, rspData, rspDataLen);
}

/*******************************************************************************
* Private Function : LinkDispatch
* Description     : Execute one request
* Purpose         : Command table of docs/PFC_LLC_Comm_Protocol_EN.md, section 5
* Input           : cmd        - request command
*                   req        - request payload
*                   reqLen     - request payload length
*                   rspData    - response data (after the status byte)
*                   rspDataLen - out: response data length
* Return          : LLC_STS_* status code
* Calls           : SMPS_SM_*, FW_UPDATE_*, LinkFill*
* Called by       : LinkHandleFrame
* Note            : SET_ENABLE ON is refused during an update session
*******************************************************************************/
static uint8_t LinkDispatch(uint8_t cmd, const uint8_t *req, uint8_t reqLen,
                     uint8_t *rspData, uint8_t *rspDataLen)
{
    uint16_t value;

    *rspDataLen = 0U;

    switch (cmd)
    {
        case LLC_CMD_GET_INFO:
            if (reqLen != 0U)
            {
                return LLC_STS_BAD_LEN;
            }
            LinkFillInfo(rspData, rspDataLen);
            return LLC_STS_OK;

        case LLC_CMD_GET_STATUS:
            if (reqLen != 0U)
            {
                return LLC_STS_BAD_LEN;
            }
            LinkFillPfcStatus(rspData, rspDataLen);
            return LLC_STS_OK;

        case LLC_CMD_GET_LOAD_ECHO:
            if (reqLen != 0U)
            {
                return LLC_STS_BAD_LEN;
            }
            LinkFillLoadEcho(rspData, rspDataLen);
            return LLC_STS_OK;

        case LLC_CMD_LLC_STATUS:
            if (reqLen != sizeof(LLC_PROTO_LLC_STATUS_T))
            {
                return LLC_STS_BAD_LEN;
            }
            memcpy(&g_llcStatus, req, sizeof(g_llcStatus));
            linkLastStatusTick = linkParsedTick;
            g_llcStatusValid = (uint16_t)(g_schedulerTickCount - linkLastStatusTick) < LLC_LINK_LOST_TICKS;
            LinkFillPfcStatus(rspData, rspDataLen);
            return LLC_STS_OK;

        case LLC_CMD_SET_ENABLE:
            if (reqLen != 1U)
            {
                return LLC_STS_BAD_LEN;
            }
            if (req[0] == LLC_ENABLE_OFF)
            {
                SMPS_SM_Enable(false);
                return LLC_STS_OK;
            }
            if (req[0] != LLC_ENABLE_ON)
            {
                return LLC_STS_BAD_PARAM;
            }
            if (FW_UPDATE_IsActive())
            {
                return LLC_STS_NOT_ALLOWED;
            }
            SMPS_SM_Enable(true);
            return LLC_STS_OK;

        case LLC_CMD_SET_VOUT_REF:
            if (reqLen != 2U)
            {
                return LLC_STS_BAD_LEN;
            }
            value = (uint16_t)req[0] | ((uint16_t)req[1] << 8);
            if ((value < (uint16_t)(SMPS_VOUT_REF_MIN_PU * VOUT_BASE * LLC_SCALE_V + 0.5f))
                || (value > (uint16_t)(SMPS_VOUT_REF_MAX_PU * VOUT_BASE * LLC_SCALE_V + 0.5f)))
            {
                return LLC_STS_BAD_PARAM;
            }
            SMPS_SM_SetVoutRef(((float)value / LLC_SCALE_V) / VOUT_BASE);
            return LLC_STS_OK;

        case LLC_CMD_CLEAR_FAULT:
            if (reqLen != 0U)
            {
                return LLC_STS_BAD_LEN;
            }
            if ((SMPS_SM_GetState() != SMPS_STATE_FAULT && SMPS_SM_GetState() != SMPS_STATE_STANDBY)
                || SMPS_SM_IsEnabled()) { return LLC_STS_NOT_ALLOWED; }
            SMPS_SM_RequestFaultClear();
            return LLC_STS_OK;

        case LLC_CMD_BOOT_ENTER:
        case LLC_CMD_BOOT_ERASE:
        case LLC_CMD_BOOT_WRITE:
        case LLC_CMD_BOOT_VERIFY:
        case LLC_CMD_BOOT_COMMIT:
        case LLC_CMD_BOOT_ABORT:
        {
            uint8_t result = FW_UPDATE_Handle(cmd, req, reqLen, LinkPfcStopForBoot(cmd),
                                             rspData, rspDataLen);
            if (result == LLC_STS_OK) { FW_UPDATE_KeepAlive(); }
            return result;
        }

        default:
            return LLC_STS_UNKNOWN_CMD;
    }
}

/*******************************************************************************
* Private Function : LinkSendResponse
* Description     : Frame a response, queue it and cache it
* Purpose         : SOF | LEN | SEQ | CMD|0x80 | STATUS | DATA | CRC16
* Input           : seq     - request SEQ (echoed)
*                   cmd     - request CMD
*                   status  - LLC_STS_*
*                   data    - response data
*                   dataLen - response data length
* Return          : None
* Calls           : LLC_LINK_Crc16, LinkTxPut
* Called by       : LinkHandleFrame
* Note            : The cache is kept even if the TX ring was full, so the
*                   master's retry still gets an answer
*******************************************************************************/
static void LinkSendResponse(uint8_t seq, uint8_t cmd, uint8_t status,
                      const uint8_t *data, uint8_t dataLen)
{
    uint16_t len = 0U;
    uint16_t crc;

    linkLastFrame[len++] = LLC_PROTO_SOF;
    linkLastFrame[len++] = (uint8_t)(dataLen + 1U);
    linkLastFrame[len++] = seq;
    linkLastFrame[len++] = (uint8_t)(cmd | LLC_PROTO_RSP_FLAG);
    linkLastFrame[len++] = status;
    if (dataLen > 0U)
    {
        memcpy(&linkLastFrame[len], data, dataLen);
        len += dataLen;
    }

    crc = LLC_LINK_Crc16(&linkLastFrame[1], (uint16_t)(len - 1U), LLC_PROTO_CRC_INIT);
    linkLastFrame[len++] = (uint8_t)(crc & 0xFFU);
    linkLastFrame[len++] = (uint8_t)(crc >> 8);

    linkLastFrameLen = len;
    linkLastSeq      = seq;
    linkLastCmd      = cmd;
    linkLastValid    = true;
    linkLastCacheTick = g_schedulerTickCount;
    linkLastReqLen = linkRxLen;
    memcpy(linkLastReq, linkRxPayload, linkRxLen);

    LinkSendCached();
}

/*******************************************************************************
* Private Function : LinkSendCached
* Description     : Queue the cached last response
* Purpose         : Send a fresh response or repeat it on a master retry
* Input           : None
* Return          : None
* Calls           : LinkTxPut
* Called by       : LinkSendResponse, LinkHandleFrame
* Note            : None
*******************************************************************************/
static void LinkSendCached(void)
{
    if (LinkTxPut(linkLastFrame, linkLastFrameLen))
    {
        g_llcLinkStats.txFrames++;
    }
    else
    {
        g_llcLinkStats.txDrops++;
    }
}

/*******************************************************************************
* Private Function : LinkTxPut
* Description     : Copy a whole frame into the TX ring
* Purpose         : Feed LLC_LINK_ByteService
* Input           : data - frame bytes
*                   len  - frame length
* Return          : false if the frame does not fit (nothing queued)
* Calls           : None
* Called by       : LinkSendCached
* Note            : All-or-nothing, so a partial frame is never sent. The
*                   head is published after the copy
*******************************************************************************/
static bool LinkTxPut(const uint8_t *data, uint16_t len)
{
    uint32_t head = linkTxHead;
    uint16_t i;

    if ((LLC_LINK_TX_RING_SIZE - (head - linkTxTail)) < len)
    {
        return false;
    }

    for (i = 0U; i < len; i++)
    {
        linkTxRing[(head + i) & LLC_LINK_TX_MASK] = data[i];
    }
    LINK_BARRIER();                 // Ring data stored before the head is published
    linkTxHead = head + len;

    return true;
}

/*******************************************************************************
* Private Function : LinkFillInfo
* Description     : GET_INFO response data
* Purpose         : Versions and boot capability
* Input           : rspData, rspDataLen - response buffer and out length
* Return          : None
* Calls           : FW_UPDATE_ActivePartition, FW_UPDATE_NvmImplemented
* Called by       : LinkDispatch
* Note            : imageCrc32 = 0 until the NVM driver can read it back
*******************************************************************************/
static void LinkFillInfo(uint8_t *rspData, uint8_t *rspDataLen)
{
    LLC_PROTO_INFO_T info;

    info.protoMajor      = LLC_PROTO_VERSION_MAJOR;
    info.protoMinor      = LLC_PROTO_VERSION_MINOR;
    info.fwMajor         = APP_FW_VERSION_MAJOR;
    info.fwMinor         = APP_FW_VERSION_MINOR;
    info.fwBuild         = APP_FW_VERSION_BUILD;
    info.activePartition = FW_UPDATE_ActivePartition();
    info.bootCaps        = FW_UPDATE_NvmImplemented() ? 0x03U : 0x00U;
    info.imageCrc32      = 0UL;
    info.reserved        = 0UL;

    memcpy(rspData, &info, sizeof(info));
    *rspDataLen = sizeof(info);
}

/*******************************************************************************
* Private Function : LinkFillPfcStatus
* Description     : GET_STATUS / LLC_STATUS response data
* Purpose         : Snapshot of the PFC state for the LLC
* Input           : rspData, rspDataLen - response buffer and out length
* Return          : None
* Calls           : FAULT_*, SOGI_PLL_IsLocked, FW_UPDATE_IsActive, LinkTo*
* Called by       : LinkDispatch
* Note            : Main context only. Copy raw state/measurements in a short
*                   IRQ-masked snapshot; encode and frame after restoring IRQs.
*                   Fault masks include fast events pending transfer to T1.
*******************************************************************************/
static void LinkFillPfcStatus(uint8_t *rspData, uint8_t *rspDataLen)
{
    LLC_PROTO_PFC_STATUS_T st;
    uint16_t flags = 0U;
    SMPS_STATE_e state;
    uint32_t latched, lockedOut;
    float vout, vacRms, iinRms, freq, tempGan, tempInlet, tempA, tempB;
    // Main context only. Copy raw state while interrupts are masked; encode later.
    __builtin_disable_interrupts();
    LINK_BARRIER();
    state = SMPS_SM_GetState();
    latched = FAULT_GetLatchedFlags();
    lockedOut = FAULT_GetLockedOutFlags();
    if (SOGI_PLL_IsLocked(&g_pll)) { flags |= LLC_PFC_FLAG_PLL_LOCKED; }
    if (SMPS_SM_IsEnabled()) { flags |= LLC_PFC_FLAG_ENABLE_CMD; }
    if (latched != 0UL) { flags |= LLC_PFC_FLAG_FAULT_ACTIVE; }
    if (lockedOut != 0UL) { flags |= LLC_PFC_FLAG_FAULT_LOCKED; }
    if (g_llcLoadValid) { flags |= LLC_PFC_FLAG_LOAD_VALID; }
    if (APP_PFC_LOAD_FF_ENABLE && g_llcLoadValid && g_pfcLoadFfPU > 0.0f) { flags |= LLC_PFC_FLAG_LOAD_FF_ON; }
    if (SMPS_SM_IsPowerGood()) { flags |= LLC_PFC_FLAG_PG; }
    vout = g_pfcVoutPU;
    vacRms = g_pfcVacRms;
    iinRms = g_pfcIinRms;
    freq = g_pfcFreqPU;
    tempGan = g_pfcTempGanPU;
    tempInlet = g_pfcTempInletPU;
    tempA = g_pfcTempLlcAPU;
    tempB = g_pfcTempLlcBPU;
    LINK_BARRIER();
    __builtin_enable_interrupts();
    if (FW_UPDATE_IsActive()) { flags |= LLC_PFC_FLAG_BOOT_SESSION; }
    st.state = (uint8_t)state;
    st.reserved0 = 0U;
    st.flags = flags;
    st.faultLatched = latched;
    st.faultLockedOut = lockedOut;
    st.voutV10 = LinkToU16(vout * VOUT_BASE * LLC_SCALE_V);
    st.vacRmsV10 = LinkToU16(vacRms * LLC_SCALE_V);
    st.iinRmsA100 = LinkToU16(iinRms * LLC_SCALE_A);
    st.freqHz100 = LinkToU16(freq * FREQ_BASE * LLC_SCALE_HZ);
    st.pinW = LinkToU16(vacRms * iinRms * LLC_SCALE_W);
    st.tempGanC10 = LinkToS16(tempGan * TEMP_BASE * LLC_SCALE_DEGC);
    st.tempInletC10 = LinkToS16(tempInlet * TEMP_BASE * LLC_SCALE_DEGC);
    st.tempMaxC10 = LinkToS16((tempA > tempB ? tempA : tempB) * TEMP_BASE * LLC_SCALE_DEGC);
    memcpy(rspData, &st, sizeof(st));
    *rspDataLen = sizeof(st);
}

/*******************************************************************************
* Private Function : LinkFillLoadEcho
* Description     : GET_LOAD_ECHO response data
* Purpose         : Let the LLC check the RD6 duty channel end to end
* Input           : rspData, rspDataLen - response buffer and out length
* Return          : None
* Calls           : LinkToU16
* Called by       : LinkDispatch
* Note            : None
*******************************************************************************/
static void LinkFillLoadEcho(uint8_t *rspData, uint8_t *rspDataLen)
{
    LLC_PROTO_LOAD_ECHO_T echo;
    float duty, freq, power;
    bool valid;
    __builtin_disable_interrupts();
    LINK_BARRIER();
    duty = g_llcLoadDuty;
    freq = g_llcLoadFreqHz;
    power = g_llcLoadPowerW;
    valid = g_llcLoadValid;
    LINK_BARRIER();
    __builtin_enable_interrupts();

    echo.dutyPermille = LinkToU16(duty * LLC_SCALE_PERMILLE);
    echo.freqHz       = LinkToU16(freq);
    echo.loadW        = LinkToU16(power);
    echo.valid        = valid ? 1U : 0U;
    echo.reserved     = 0U;

    memcpy(rspData, &echo, sizeof(echo));
    *rspDataLen = sizeof(echo);
}

/*******************************************************************************
* Private Function : LinkToU16
* Description     : Round and saturate a float to uint16_t
* Purpose         : Fixed-point payload fields
* Input           : value - scaled value
* Return          : 0..65535
* Calls           : None
* Called by       : LinkFillPfcStatus, LinkFillLoadEcho
* Note            : None
*******************************************************************************/
static uint16_t LinkToU16(float value)
{
    if (!isfinite(value)) { return 0U; }
    if (value <= 0.0f)
    {
        return 0U;
    }
    if (value >= 65535.0f)
    {
        return 65535U;
    }
    return (uint16_t)(value + 0.5f);
}

/*******************************************************************************
* Private Function : LinkToS16
* Description     : Round and saturate a float to int16_t
* Purpose         : Signed fixed-point payload fields (temperatures)
* Input           : value - scaled value
* Return          : -32768..32767
* Calls           : None
* Called by       : LinkFillPfcStatus
* Note            : None
*******************************************************************************/
static int16_t LinkToS16(float value)
{
    if (!isfinite(value)) { return 0; }
    if (value <= -32768.0f)
    {
        return -32768;
    }
    if (value >= 32767.0f)
    {
        return 32767;
    }
    return (int16_t)((value >= 0.0f) ? (value + 0.5f) : (value - 0.5f));
}

/*******************************************************************************
* Private Function : LinkPfcStopForBoot
* Description     : Precondition of BOOT_ENTER: PFC stopped and held off
* Purpose         : Keep fw_update independent of the state machine
* Input           : cmd - LLC_CMD_BOOT_*
* Return          : true if the PFC is in STANDBY or FAULT (BOOT_ENTER only)
* Calls           : SMPS_SM_Enable, SMPS_SM_GetState
* Called by       : LinkDispatch
* Note            : A running PFC is never stopped here (NOT_ALLOWED, the LLC
*                   must send SET_ENABLE 0 first). When already stopped, the
*                   enable command is cleared and the state read again, so a
*                   PREPARE started by T1 in between is caught.
*                   Other BOOT_* commands do not use the result
*******************************************************************************/
static bool LinkPfcStopForBoot(uint8_t cmd)
{
    SMPS_STATE_e state;

    if (cmd != LLC_CMD_BOOT_ENTER)
    {
        return false;
    }

    state = SMPS_SM_GetState();
    if ((state != SMPS_STATE_STANDBY) && (state != SMPS_STATE_FAULT))
    {
        return false;
    }

    SMPS_SM_Enable(false);
    state = SMPS_SM_GetState();
    return (state == SMPS_STATE_STANDBY) || (state == SMPS_STATE_FAULT);
}

/******************************************************************************/
#undef LLC_LINK_C
