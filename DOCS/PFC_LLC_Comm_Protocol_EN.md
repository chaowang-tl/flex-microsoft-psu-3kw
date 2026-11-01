# PFC ↔ LLC Communication Protocol

| Item | Value |
|---|---|
| File Name | PFC_LLC_Comm_Protocol_EN.md |
| Description | UART protocol, duty-cycle signals and firmware update procedure between the primary-side PFC and the secondary-side LLC |
| Original Author | Chao Wang <chao.wang@microchip.com> |
| Created on | 2026/10/31 |
| Modified by | Chao Wang |
| Modified on | 2026/11/01 |
| Language | [中文](PFC_LLC_Comm_Protocol_CN.md) · English |
| Protocol version | **1.0** (`LLC_PROTO_VERSION_MAJOR = 1`, `LLC_PROTO_VERSION_MINOR = 0`) |
| Applies to | Primary-side PFC (dsPIC33AK256MPS206, slave), secondary-side LLC (dsPIC33AH1024MPS610, master) |
| Shared headers | [`sources/apps/comm/llc_protocol.h`](../sources/apps/comm/llc_protocol.h) + [`sources/common/llc_duty_def.h`](../sources/common/llc_duty_def.h), **byte-for-byte identical on both sides** |
| PFC-side implementation | `llc_link.c` (link/commands), `fw_update.c` (update session), `llc_signal.c` (duty-cycle signals), `uart_link.c` (UART1 registers) |
| Status | Implemented on the PFC side; Flash erase/write/partition switch for the update are placeholders (return `NOT_IMPLEMENTED`); LLC side still to be developed |
| Companion document | [PFC Software Architecture Manual](PFC_Software_Architecture_EN.md) (Chapter 18) |

---

## Contents

1. [Overview](#1-overview) (incl. [1.0 Guide](#10-five-minute-guide-for-llc-side-developers))
2. [Duty-Cycle Signals (RC4 / RD6)](#2-duty-cycle-signals-rc4--rd6)
3. [UART Frame Format](#3-uart-frame-format)
4. [Session Rules](#4-session-rules)
5. [Commands](#5-commands)
6. [Firmware Update](#6-firmware-update)
7. [Version Compatibility](#7-version-compatibility)
8. [LLC-Side Implementation Notes](#8-llc-side-implementation-notes)
9. [Appendix: CRC and Example Frames](#9-appendix-crc-and-example-frames)

---

## 1. Overview

**Final build snapshot**: APP_PLL_PWM_TEST_ENABLE=1 forces effective startup enable. Test ISRs use synthetic Vac/Vout, zero currents and fixed 25 °C temperatures, without reading physical ADC data, starting CT calibration or requiring CT/ADC gain calibration for startup/PG. Gain-failure diagnostics remain. Actual ADC ISR heartbeat, PLL, state timing, fast latches, watchdog and hardware PCI remain active. Calibration procedures below apply to real sensing with test=0. Test PG/telemetry do not establish real power-stage readiness; restore zero before power operation.

### 1.0 Five-Minute Guide for LLC-Side Developers

1. **Copy the two header files**: `llc_protocol.h` and the `common/llc_duty_def.h` it includes. Keep the relative path and do not modify them (Chapter 8).
2. **Implement in the order below**; each step can be tested against the PFC on its own:
   - **Duty-cycle signals** (Chapter 2): output RD6 at 10 kHz with a duty cycle of 10 %~90 % following the load; capture RC4 to get the bulk bus voltage. This step does not depend on the UART.
   - **UART link** (Chapters 3 and 4): 115200 8N1, CRC16/Modbus. Send `GET_INFO` (0x01) first and check the protocol major version.
   - **Periodic status**: send `LLC_STATUS` (0x12) every 10 ms; the response carries `PFC_STATUS`. If the PFC receives no valid frame within 100 ms (`LLC_PROTO_LINK_LOST_MS`), it declares the link lost.
   - **Control**: `SET_ENABLE` (0x20), `SET_VOUT_REF` (0x21), `CLEAR_FAULT` (0x22).
   - **Firmware update** (Chapter 6): implement last. The PFC side currently has only the session framework.
3. **Unit tests**: the example frames and CRCs in the Chapter 9 appendix have all been verified and can be used directly as test cases for the LLC-side encoder/decoder.

There are three isolated paths between the PFC and the LLC:

| Path | Direction | PFC pin | Purpose | Refresh |
|---|---|---|---|---|
| UART1 | Bidirectional | RC8 = RX, RC9 = TX | Commands, status exchange, firmware update | LLC polling, 10 ms |
| Duty-cycle signal 1 | PFC → LLC | RC4 (SCCP3) | Bulk bus voltage | 10 kHz |
| Duty-cycle signal 2 | LLC → PFC | RD6 (SCCP1/2 capture) | LLC load power (PFC voltage loop feed-forward) | 10 kHz |

Division of roles:
- The **duty-cycle signals** carry quantities requiring continuous updates; the normal load feed-forward latency target is below 1 ms. They detect missing edges and out-of-range high times. Hardware PWM may continue sending an old value after a CPU stall, so good captures do not prove that software is updating.
- The **UART** carries status, faults, setpoints and firmware images.

```
        Primary PFC (slave)                      Secondary LLC (master)
   ┌──────────────────────┐ isolation┌──────────────────────┐
   │ UART1 RX  RC8  ◄─────┼────╫─────┼── TX                 │
   │ UART1 TX  RC9  ──────┼────╫─────┼─► RX                 │
   │ SCCP3     RC4  ──────┼────╫─────┼─► Duty capture (Vbus)│
   │ SCCP1/2   RD6  ◄─────┼────╫─────┼── PWM output (load)  │
   └──────────────────────┘          └──────────────────────┘
```

---

## 2. Duty-Cycle Signals (RC4 / RD6)

### 2.1 Encoding

Both signals use the same encoding: **10 kHz push-pull PWM, active high, duty cycle 10%~90% maps linearly to 0~full scale.**

```
D = 10 % + 80 % × (X / X_FS)          X = (D − 10 %) / 80 % × X_FS
```

| Signal | Direction | Quantity X | Full scale X_FS | Resolution (PFC side) |
|---|---|---|---|---|
| RC4 | PFC → LLC | PFC bulk bus voltage | **500 V** | 0.0625 V (1 count / 10000) |
| RD6 | LLC → PFC | LLC output power | **3600 W** (120 % of rated) | 0.45 W |

| Bulk bus voltage | RC4 duty | Load | RD6 duty |
|---|---|---|---|
| 0 V | 10 % | 0 W | 10 % |
| 250 V | 50 % | 900 W | 30 % |
| 400 V | **74 %** | 1800 W | 50 % |
| 500 V | 90 % | 3600 W | 90 % |

### 2.2 Validity Check (Receiver)

PFC admits RD6 only when duty is 5%–95%, measured frequency is 10 kHz ±1%, a qualified falling edge arrived within 2 ms, a rising-edge period is available and eight consecutive complete captures qualify. Invalid input, overflow or mismatched capture backlog restarts qualification.

Constant low/high is invalid rather than zero watts. The 5%–10% and 90%–95% guard bands decode to zero/full scale. Initialization/reset may drive low; stopping the power stage retains valid measured reporting: LLC zero watts is 10%, and PFC RC4 updates measured bus every 1 ms in STANDBY/STOPPING/FAULT. RC4 starts at 0% until its first update. Valid hardware PWM does not prove sender CPU liveness; a separate system freshness policy is needed.

### 2.3 Measurement Method (Receiver)

PFC RD6 uses a 100 MHz SCCP clock:

```text
SCCP2: free-running 16-bit rising-edge capture; timestamp delta modulo 65536 = period
       ALTSYNC=1 still resets SCCP1 on each rising edge
SCCP1: falling-edge capture = highTime
duty = highTime / period
frequency = 100000000 / period
```

The phase B ADC ISR polls and drains at most eight values per FIFO; CCP interrupts remain disabled. Two initial rising edges establish a period, then eight consecutive qualified complete cycles restore validity. Duty filtering uses α=1/8. ICOV, multiple same-edge captures per poll that cannot be paired reliably, or about 2 ms without a falling edge invalidate and restart qualification with frequency diagnostic zero. Actual-period division removes the former nominal-period proportional error.

LLC RC4 reception should measure actual period and high time as above. The older receiver in `sec_pkcmc_llc` requires separate synchronization; this pass modifies only PFC and does not establish that the LLC already has these frequency/backlog checks.

### 2.4 Transmitter Requirements

| Requirement | PFC (RC4) | LLC (RD6) |
|---|---|---|
| Frequency | 10 kHz (CCP3PR=9999) | 10 kHz ±1%, checked by PFC from measured period |
| Update rate | 1 ms (`APP_TASKS_Task1ms`) | ≥ 1 kHz, preferably following the LLC control loop |
| Update method | Buffered register, takes effect at the period boundary | Update at the period boundary; truncated pulses are not allowed |
| Value sent | Measured bulk bus voltage (not notch-filtered) | LLC output power (Vout × Iout) |

---

## 3. UART Frame Format

### 3.1 Physical Layer

| Parameter | Value |
|---|---|
| Baud rate | 115200 |
| Format | 8 data bits, no parity, 1 stop bit, no flow control |
| Levels | 3.3 V CMOS, through a digital isolator |
| Idle | High |

### 3.2 Frame Structure

```
 Byte:   0      1      2      3      4 .. 3+LEN       4+LEN     5+LEN
       ┌──────┬──────┬──────┬──────┬──────────────┬─────────┬─────────┐
       │ SOF  │ LEN  │ SEQ  │ CMD  │ PAYLOAD[LEN] │ CRC16_L │ CRC16_H │
       │ 0xA5 │0..136│      │      │              │         │         │
       └──────┴──────┴──────┴──────┴──────────────┴─────────┴─────────┘
                └───────────── CRC coverage ─────────────┘
```

| Field | Length | Description |
|---|---|---|
| SOF | 1 | Fixed `0xA5` |
| LEN | 1 | Number of PAYLOAD bytes, 0~136 (`LLC_PROTO_PAYLOAD_MAX`). If exceeded, the frame is discarded and the receiver searches for SOF again |
| SEQ | 1 | Sequence number. The master increments it by 1 for every **new** request (0xFF wraps to 0x00); unchanged on retransmission; echoed unchanged in the response |
| CMD | 1 | Request: command number (bit 7 = 0). Response: request command number \| `0x80` |
| PAYLOAD | LEN | Command data; all multi-byte fields are **little-endian** |
| CRC16 | 2 | CRC16/Modbus over LEN, SEQ, CMD, PAYLOAD, **low byte first** |

- Frame length is 6~142 bytes; at 115200 the longest frame takes about 12.3 ms.
- CRC16/Modbus: polynomial 0xA001 (reflected form), initial value 0xFFFF, no final XOR. Check value `"123456789"` → `0x4B37`. This is the same algorithm as `MODBUS_RTU_Crc16` in the LLC project's Modbus RTU and can be reused directly.

### 3.3 Request/Response Matching

**Each request gets exactly one response frame**, matched by `CMD` and `SEQ`:

| Field | Request | Response |
|---|---|---|
| CMD | `c` (0x00~0x7F) | `c | 0x80` |
| SEQ | `s` | `s` (echoed unchanged) |
| PAYLOAD | Request parameters | `STATUS` (1 byte) + returned data |

- When `STATUS = OK`, it is followed by the command's returned data (length given in Section 5).
- When `STATUS ≠ OK`, there is **only the single STATUS byte**, LEN = 1.

### 3.4 Status Codes

| Value | Name | Meaning | Master action |
|---|---|---|---|
| 0x00 | `OK` | Success | — |
| 0x01 | `BAD_CRC` | Reserved. The slave **does not respond** to frames with a CRC error; the master retransmits on timeout | — |
| 0x02 | `BAD_LEN` | LEN does not match the length required by the command | Check the implementation |
| 0x03 | `UNKNOWN_CMD` | Command number does not exist | Check the version with `GET_INFO` |
| 0x04 | `BAD_PARAM` | Parameter out of range or wrong magic word | Check the parameters |
| 0x05 | `BUSY` | The slave is executing a long operation (e.g. erase); send the same command again later | Resend with a **new SEQ** (the same SEQ only returns the cached BUSY response, see 4.3) |
| 0x06 | `NOT_ALLOWED` | Not allowed in the current state, e.g. entering update while running | Satisfy the condition first |
| 0x07 | `NOT_IMPLEMENTED` | Command is defined but not yet implemented in this firmware version | Give up on the feature |
| 0x08 | `FLASH_ERR` | Flash erase/write failed | Erase again |
| 0x09 | `VERIFY_ERR` | Image CRC mismatch | Erase and write again |
| 0x0A | `SEQUENCE_ERR` | Wrong update command order or non-contiguous write offset | Restart following the Section 6 procedure |

---

## 4. Session Rules

### 4.1 Master/Slave

- **The LLC is the master**; only the master can initiate requests. The PFC is the slave; it only responds and never transmits on its own.
- At most **one** request may be outstanding at any time: the master may send the next one only after it has received the response or timed out.

### 4.2 Timing

| Parameter | Value | Macro | Description |
|---|---|---|---|
| Inter-byte gap within a frame | ≤ 2 ms | `LLC_PROTO_INTERBYTE_MS` | If exceeded, the receiver discards the partial frame |
| Slave response start | ≤ 2 ms | `LLC_PROTO_RSP_START_MS` | Measured from the last byte of the request |
| Master response timeout | 20 ms | `LLC_PROTO_RSP_TIMEOUT_MS` | From the last byte of the request until the full response is received |
| Retries | 3 | `LLC_PROTO_RETRY_MAX` | SEQ unchanged on each retry |
| Normal polling period | 10 ms | `LLC_PROTO_POLL_PERIOD_MS` | Send `LLC_STATUS` |
| Link lost | 100 ms | `LLC_PROTO_LINK_LOST_MS` | No valid frame received during this time |

```
LLC  ──[REQ seq=n]──►                         ──[REQ seq=n+1]──►
PFC              ◄──[RSP seq=n]── (starts ≤2 ms)
     |←───────────────── 10 ms ──────────────────→|
```

### 4.3 Retransmission and De-duplication

After response timeout the master resends the entire identical request: SEQ, CMD, LEN and PAYLOAD. PFC caches only the latest response for 100 ms from execution. A hit increments retries and resends without executing again. Equal SEQ/CMD with different length/data is a new request; an expired cache also executes anew. Retries must finish within the cache window; de-duplication is not indefinite.

A qualified cached LLC_STATUS retry refreshes status receive time. A cached successful BOOT retry refreshes an active session. Neither extends the response cache's own 100 ms lifetime. Cache age follows the actual T1 time base despite delayed main-loop catch-up.

### 4.4 Link Loss

A CRC-valid request refreshes `g_llcLinkUp`; received response frames (CMD bit7=1) are ignored before refreshing it. Only a correctly sized LLC_STATUS, including a qualified cached retry, refreshes `g_llcStatusValid`. Ages use the final byte's T1 receive timestamp; both expire at 500 ms. Once expired, a flag cannot revive on 16-bit clock wrap without a fresh qualified frame.

Link loss changes status without automatically stopping PFC; LLC_STATUS target/power-limit demand fields are not executed. RD6 validity and UART validity are independent, and LOAD_VALID does not prove LLC CPU liveness. A system stop-on-loss policy requires separate definition and implementation.

## 5. Commands

### 5.1 Command Summary

| CMD | Name | Request LEN | Response data (after STATUS) | Implementation status |
|---|---|---|---|---|
| 0x01 | `GET_INFO` | 0 | `INFO` (16 B) | ✔ |
| 0x10 | `GET_STATUS` | 0 | `PFC_STATUS` (28 B) | ✔ |
| 0x11 | `GET_LOAD_ECHO` | 0 | `LOAD_ECHO` (8 B) | ✔ |
| 0x12 | `LLC_STATUS` | 20 (`LLC_STATUS`) | `PFC_STATUS` (28 B) | ✔ stored only |
| 0x20 | `SET_ENABLE` | 1 | — | ✔ |
| 0x21 | `SET_VOUT_REF` | 2 | — | ✔ |
| 0x22 | `CLEAR_FAULT` | 0 | — | ✔ |
| 0x30 | `BOOT_ENTER` | 8 | `BOOT_ENTER_RSP` (8 B) | ✔ |
| 0x31 | `BOOT_ERASE` | 0 | `PROGRESS` (4 B) | Framework, returns `NOT_IMPLEMENTED` |
| 0x32 | `BOOT_WRITE` | 8 + 16~128 | `PROGRESS` (4 B) | Framework; currently returns `SEQUENCE_ERR` (6.8) |
| 0x33 | `BOOT_VERIFY` | 8 | `PROGRESS` (4 B) | Framework; currently returns `SEQUENCE_ERR` because the WRITING state cannot be reached (6.8) |
| 0x34 | `BOOT_COMMIT` | 0 | — | Framework; currently returns `SEQUENCE_ERR` (6.8) |
| 0x35 | `BOOT_ABORT` | 0 | — | ✔ (the slave does not check LEN; any payload is accepted) |

0x00, 0x02~0x0F, 0x13~0x1F, 0x23~0x2F, 0x36~0x7F are undefined (reserved); the slave returns `UNKNOWN_CMD`.

### 5.2 Data Direction

The master/slave relationship only determines **who initiates**; data can flow in both directions:
- **LLC → PFC**: data is carried in the **request** payload, including setpoints (`SET_*`), the LLC's own status (`LLC_STATUS`) and the firmware image (`BOOT_WRITE`).
- **PFC → LLC**: data is carried in the **response** payload.

In normal operation the LLC only needs to send `LLC_STATUS` every 10 ms: the request carries the LLC status and the response returns the PFC status, so **one round trip completes the bidirectional exchange**.

### 5.3 Fixed-Point Formats

| Quantity | Unit | Type | Example |
|---|---|---|---|
| Voltage | 0.1 V | uint16 | 4000 = 400.0 V |
| LLC output voltage | 0.01 V | uint16 | 1200 = 12.00 V |
| Current (PFC input) | 0.01 A | uint16 | 1350 = 13.50 A |
| Current (LLC output) | 0.1 A | uint16 | 2500 = 250.0 A |
| Frequency | 10 kHz (CCP3PR=9999) | 10 kHz ±1%, checked by PFC from measured period |
| Power | 1 W | uint16 | 3105 = 3105 W |
| Temperature | 0.1 °C | int16 | 650 = 65.0 °C |
| Ratio | ‰ | uint16 | 740 = 74.0 % |

Values outside the type range saturate (no wrap-around).

### 5.4 `GET_INFO` (0x01)

Request: no payload. Response `LLC_PROTO_INFO_T` (16 bytes):

| Offset | Field | Type | Description |
|---|---|---|---|
| 0 | `protoMajor` | u8 | Protocol major version, 1 for this document |
| 1 | `protoMinor` | u8 | Protocol minor version, 0 for this document |
| 2 | `fwMajor` | u8 | PFC firmware version (`APP_FW_VERSION_*`) |
| 3 | `fwMinor` | u8 | |
| 4 | `fwBuild` | u16 | |
| 6 | `activePartition` | u8 | Flash partition currently running: 1 or 2 |
| 7 | `bootCaps` | u8 | Zero while NVM is unimplemented; 0x03 when implemented/enabled (bit0 session, bit1 Flash) |
| 8 | `imageCrc32` | u32 | CRC32 of the current image, 0 if not calculated |
| 12 | `reserved` | u32 | 0 |

After power-up the LLC should first send `GET_INFO` and check `protoMajor` (see Section 7).

### 5.5 `GET_STATUS` (0x10) and the `PFC_STATUS` Structure

Request: no payload. Response `LLC_PROTO_PFC_STATUS_T` (28 bytes):

| Offset | Field | Type | Description |
|---|---|---|---|
| 0 | `state` | u8 | PFC state machine, see table below |
| 1 | `reserved0` | u8 | 0 |
| 2 | `flags` | u16 | See table below |
| 4 | `faultLatched` | u32 | Latched fault bitmap (PFC `FAULTLOG_FLAGS_e`) |
| 8 | `faultLockedOut` | u32 | Bitmap of locked-out faults (reset required) |
| 12 | `voutV10` | u16 | Bulk bus voltage, 0.1 V |
| 14 | `vacRmsV10` | u16 | Grid voltage RMS, 0.1 V |
| 16 | `iinRmsA100` | u16 | Input current RMS, 0.01 A |
| 18 | `freqHz100` | u16 | Grid frequency, 0.01 Hz |
| 20 | `pinW` | u16 | Estimated input apparent power (Vrms × Irms), W |
| 22 | `tempGanC10` | i16 | GaN temperature, 0.1 °C |
| 24 | `tempInletC10` | i16 | Inlet air temperature, 0.1 °C |
| 26 | `tempMaxC10` | i16 | Maximum of the remaining temperature sensors, 0.1 °C |

`state`:

| Value | State | Value | State |
|---|---|---|---|
| 0 | INIT | 4 | RUN |
| 1 | STANDBY | 5 | STOPPING |
| 2 | PREPARE | 6 | FAULT |
| 3 | SOFTSTART | | |

`flags`:

| Bit | Name | Meaning |
|---|---|---|
| 0 | `PLL_LOCKED` | Locked to the grid |
| 1 | `ENABLE_CMD` | PFC enable command is 1 |
| 2 | `FAULT_ACTIVE` | A fault is latched or a fast protection is pending |
| 3 | `FAULT_LOCKED` | A fault is locked out; the PFC must be reset |
| 4 | `LOAD_VALID` | The RD6 load signal received by the PFC is valid |
| 5 | `LOAD_FF_ON` | Compile switch on, RD6 valid and present feed-forward >0; not alone a running-loop indication |
| 6 | `BOOT_SESSION` | Update session in progress |
| 7 | `PG` | RUN, enable, admitted control, calibration/PLL/health successful, no faults/fast latch, finite bus within target ±10 V; RUN entry requires 5 ms settling. Real mode requires calibration; bench mode uses synthetic bus and bypasses physical calibration |
| 8~15 | — | Reserved, 0 |

Fault bits 0–14 retain their meanings; bit15 ISR_HEALTH, 16 CALIBRATION, 17 STARTUP_TIMEOUT; bits18–31 reserved. See [fault.h](../sources/apps/fault/fault.h).

State, flags, fault masks and raw measurements are copied with interrupts briefly masked, then encoded after restoration. The mask includes pending fast faults. Temperature modeling remains a placeholder and pinW estimates apparent power; NaN/Inf encode as zero rather than proving valid measurement.

### 5.6 `GET_LOAD_ECHO` (0x11)

Used during debugging to **verify the RD6 channel in a closed loop**: the LLC compares the load value it sent with the value the PFC actually received.

Request: no payload. Response `LLC_PROTO_LOAD_ECHO_T` (8 bytes):

| Offset | Field | Type | Description |
|---|---|---|---|
| 0 | `dutyPermille` | u16 | Duty cycle measured by the PFC (filtered), ‰ |
| 2 | `freqHz` | u16 | Measured RD6 rising-edge period converted to Hz; zero on timeout/overflow/mismatched backlog |
| 4 | `loadW` | u16 | Load decoded by the PFC, W (0 when invalid) |
| 6 | `valid` | u8 | 1 = valid |
| 7 | `reserved` | u8 | 0 |

### 5.7 `LLC_STATUS` (0x12)

The LLC's main polling command, once every 10 ms. Request `LLC_PROTO_LLC_STATUS_T` (20 bytes):

| Offset | Field | Type | Description |
|---|---|---|---|
| 0 | `state` | u8 | LLC state code, defined by the LLC |
| 1 | `reserved0` | u8 | 0 |
| 2 | `request` | u16 | LLC requests to the PFC, see table below |
| 4 | `faultFlags` | u32 | LLC fault bitmap, defined by the LLC |
| 8 | `voutV100` | u16 | LLC output voltage, 0.01 V |
| 10 | `ioutA10` | u16 | LLC output current, 0.1 A |
| 12 | `poutW` | u16 | LLC output power, W |
| 14 | `tempMaxC10` | i16 | LLC maximum temperature, 0.1 °C |
| 16 | `voutRefReq` | u16 | Requested PFC bulk bus voltage, 0.1 V (valid when `REQ_VOUT_REF_VALID`) |
| 18 | `powerLimitW` | u16 | Requested PFC power limit, W (valid when `REQ_PFC_DERATE`) |

`request`:

| Bit | Name | Meaning |
|---|---|---|
| 0 | `PFC_DERATE` | Requests the PFC to limit power to `powerLimitW` |
| 1 | `PFC_SHUTDOWN` | Requests the PFC to shut down |
| 2 | `VOUT_REF_VALID` | `voutRefReq` is valid |
| 3~15 | — | Reserved, 0 |

Response: same as `GET_STATUS` (`PFC_STATUS`, 28 bytes).

> **PFC behavior in protocol 1.0**: the PFC stores the whole frame in `g_llcStatus` and refreshes the timestamp (`g_llcStatusValid`), but **does not act on the requests in `request`**. Bus tracking, derating and shutdown policies will be defined in a later version; until then, use `SET_ENABLE` / `SET_VOUT_REF` to control the PFC.

### 5.8 `SET_ENABLE` (0x20)

Request 1 byte: `0x00` = disable, `0x01` = enable. Response: STATUS only.

| Case | STATUS |
|---|---|
| 0x00 | `OK`, the PFC enters the shutdown sequence |
| 0x01 | `OK`, the PFC starts according to the state machine conditions (PLL locked, pre-charge complete) |
| 0x01, with an update session in progress | `NOT_ALLOWED` |
| Other values | `BAD_PARAM` |

`OK` means the command has been accepted, not that the PFC has already entered RUN. Check `PFC_STATUS.state` for startup progress.

### 5.9 `SET_VOUT_REF` (0x21)

Request 2 bytes: u16 bulk bus voltage target in units of 0.1 V. Range 3500~4100 (350.0~410.0 V); values outside return `BAD_PARAM`. Response: STATUS only. The PFC ramps to the new target at 90 V/s.

### 5.10 `CLEAR_FAULT` (0x22)

Request has no payload. It requires `state=FAULT/STANDBY` and enable=0, otherwise NOT_ALLOWED. Send SET_ENABLE 0, confirm a stopped state, remove the cause, then request CLEAR_FAULT.

OK only acknowledges queuing to T1, not completed clearing. T1 requires control disabled, no lockout, recovered/debounced slow detection and a finite present value beyond recovery hysteresis. Both fast-current channels must be <33 A, fast bus <421 V and event active state cleared. Detector history and pending fast faults are retained; unrecovered entries remain. Lockout requires MCU reset after addressing the hardware cause. Query GET_STATUS to confirm masks and state.

## 6. Firmware Update

### 6.1 Principle: A/B Dual Partitions

> **Implementation status**: partition writing/swapping below describes the update design target. `FwUpdateNvm*()` remains unimplemented and partition swapping is disabled. This design is not a verified guarantee of power-loss recovery or update safety; see 6.8.

The PFC Flash is configured for **dual boot partitions** (`FBOOT_BTMODE = DUAL`), each partition 0x1F000 bytes (124 KB, dsPIC33AK256MPS206).

```
  ┌───────────────────────┐   ┌───────────────────────┐
  │ Partition 1 (running) │   │ Partition 2 (inactive)│
  │ Old FW, keeps running │   │ ← new image goes here │
  └───────────────────────┘   └───────────────────────┘
   After verification passes and COMMIT, next boot runs from partition 2
```

- The design requires writing only the **inactive partition** and preserving the current bootable image. Write addresses, image bounds and configuration-word protection must be implemented and verified in the NVM driver.
- The design requires retaining the old firmware after erase/verify failure or power loss before commit. NVM/commit implementation and power-loss fault-injection results are absent, so recovery under arbitrary failures is not guaranteed.
- Verify inactive-partition RWW and T1/UART latency with the actual NVM driver, execution location, cache and WDT behavior. Current stubs do not establish communication timing during programming.

### 6.2 Preconditions

| Condition | Description |
|---|---|
| PFC stopped | `state` is STANDBY or FAULT, otherwise `BOOT_ENTER` returns `NOT_ALLOWED`. The LLC should first send `SET_ENABLE 0` and wait for the PFC to enter STANDBY |
| Magic word | `BOOT_ENTER.magic = 0x544F4F42` ("BOOT") |
| Image size | 16~0x1F000 bytes, must be a multiple of 16 (0 or unaligned returns `BAD_PARAM`) |
| Session timeout | Ends after 5 s without a successful BOOT command, including cached successful retry; actual T1 clock, invalid requests do not keep alive, PFC stays disabled |
| Run protection | During the session `SET_ENABLE 1` returns `NOT_ALLOWED` |

### 6.3 Procedure

```
LLC                                         PFC
 │── SET_ENABLE 0 ──────────────────────────►│ Shut down
 │◄─────────────────────────────────── OK ───│
 │   (poll GET_STATUS until state = STANDBY)   │
 │── BOOT_ENTER {magic, imageSize} ─────────►│ IDLE → ACTIVE, enable locked
 │◄──── OK {targetPartition, 128, 16, max} ──│
 │── BOOT_ERASE ────────────────────────────►│ Erase inactive partition (may return BUSY; resend until OK)
 │◄──────────────────── OK {imageSize} ──────│ ACTIVE → ERASED
 │── BOOT_WRITE {offset=0, data[128]} ──────►│ ERASED → WRITING
 │◄──────────────────── OK {next=128} ───────│
 │── BOOT_WRITE {offset=128, ...} ──────────►│ Offsets must be strictly contiguous
 │      ... (imageSize/128 blocks in total) ...│
 │── BOOT_VERIFY {imageSize, crc32} ────────►│ Read back and calculate CRC32
 │◄──────────────────── OK {crc32} ──────────│ WRITING → VERIFIED
 │── BOOT_COMMIT ───────────────────────────►│ Set next boot from the new partition
 │◄─────────────────────────────────── OK ───│ Reset after sending the response
 │   (wait for PFC restart; check new version and activePartition with GET_INFO)
```

If any step fails or the update must be abandoned, send `BOOT_ABORT`, or start again from `BOOT_ENTER`.

### 6.4 Session State Machine (PFC)

```
          BOOT_ENTER                 BOOT_ERASE OK            BOOT_WRITE OK
  IDLE ─────────────► ACTIVE ────────────────────► ERASED ─────────────────► WRITING ─┐
   ▲                    ▲  ▲                                                  │  ▲    │ BOOT_WRITE OK
   │                    │  └──── erase/write fail (FLASH_ERR) ───────────────────┘  └────┘
   │                    │                                                     │
   │ BOOT_ABORT /       │                                         BOOT_VERIFY OK
   │ 5 s timeout        │ (any state)                                         ▼
   └────────────────────┴───────────────────────────────────────────────── VERIFIED
                                                                              │ BOOT_COMMIT OK
                                                                              ▼
                                                                   Reset, boot from new partition
```

`BOOT_ENTER` can be sent in any state and restarts the session (the image must be erased and written again).

### 6.5 Command Details

**`BOOT_ENTER` (0x30)** request 8 bytes `LLC_PROTO_BOOT_ENTER_T`:

| Offset | Field | Type | Description |
|---|---|---|---|
| 0 | `magic` | u32 | `0x544F4F42` |
| 4 | `imageSize` | u32 | Image size in bytes |

Response 8 bytes `LLC_PROTO_BOOT_ENTER_RSP_T`:

| Offset | Field | Type | Description |
|---|---|---|---|
| 0 | `targetPartition` | u8 | Partition to be written (1 or 2) |
| 1 | `writeMax` | u8 | Maximum data bytes per block, 128 |
| 2 | `writeAlign` | u8 | Alignment requirement for offset and length, 16 |
| 3 | `reserved` | u8 | 0 |
| 4 | `imageMax` | u32 | Maximum image size in bytes, 0x1F000 (124 KB) |

**`BOOT_ERASE` (0x31)** request: no payload. Response `PROGRESS`: the image size declared in `BOOT_ENTER` (i.e. the range the erase must cover). If the erase takes longer than the response time, the slave returns `BUSY` and the master resends `BOOT_ERASE` with a **new SEQ** until `OK` is returned.

**`BOOT_WRITE` (0x32)** request = 8-byte header `LLC_PROTO_BOOT_WRITE_T` + data:

| Offset | Field | Type | Description |
|---|---|---|---|
| 0 | `offset` | u32 | Byte offset in the image; must equal `next` from the previous response |
| 4 | `reserved` | u32 | 0 |
| 8 | `data` | 16~128 B | Length is a multiple of 16 |

Response `PROGRESS`: the offset `next` at which the next block should be written.

- A non-contiguous offset returns `SEQUENCE_ERR`; writing beyond `imageSize` returns `BAD_PARAM`.
- A write failure returns `FLASH_ERR`; the session returns to ACTIVE and must be erased again.
- If a response is lost, the master resends with the **same SEQ**; the slave returns the cached response (Section 4.3) and does not write again.

**`BOOT_VERIFY` (0x33)** request 8 bytes `LLC_PROTO_BOOT_VERIFY_T`:

| Offset | Field | Type | Description |
|---|---|---|---|
| 0 | `imageSize` | u32 | Must match `BOOT_ENTER` and must have been completely written |
| 4 | `imageCrc32` | u32 | CRC32 of the whole image |

Response `PROGRESS`: on a match, returns the CRC32 calculated by the PFC. On a mismatch, returns `VERIFY_ERR` with **only the status byte and no data** (per the general rule in Section 3.3, a non-OK response LEN contains no data).

The CRC32 algorithm is CRC-32/ISO-HDLC (i.e. zlib's `crc32`): polynomial 0xEDB88320 (reflected), initial value and final XOR value both 0xFFFFFFFF. Check value `"123456789"` → `0xCBF43926`.

**`BOOT_COMMIT` (0x34)** request: no payload. May only be sent in the VERIFIED state, otherwise returns `SEQUENCE_ERR`. The slave first finishes sending the `OK` response, then resets. After the reset it boots from the new partition; communication is possible again after about 1 s.

**`BOOT_ABORT` (0x35)**: request strictly zero bytes. Nonempty payload returns BAD_LEN without changing the session; valid request returns OK and IDLE without activating written data.

### 6.6 Image Format

- The image is the **single-partition program image** produced by the PFC project link: contiguous bytes starting from the partition start address, length padded up to 16 bytes, with the padding filled with 0xFF.
- The tool that converts `.hex` into a binary image and the format of the image header (version, CRC) will be defined together with the NVM driver implementation, at which point the protocol will be raised to 1.1.

### 6.7 Time Estimate

The following estimates serial transfer of an approximately 60 KB image, not measured Flash-driver performance. See the review record for current linker usage; single-partition image format and final length remain undefined.

| Item | Estimate |
|---|---|
| Per block: 142 B request frame + 11 B response frame, 115200 bps | ≈ 13.3 ms |
| Per-block Flash write (about 8 quad-words) | < 1 ms |
| Total write | ≈ 6 s |
| Erase (about 15 pages) | < 1 s |
| Verify + commit + restart | ≈ 1.5 s |
| **Total** | **About 8~9 s** |

### 6.8 Parts Not Yet Implemented on the PFC Side

| Item | Location | Description |
|---|---|---|
| Erase inactive partition | `FwUpdateNvmErase()` | Needs the NVM driver; if it takes long, run page by page and return `BUSY` |
| Write | `FwUpdateNvmWrite()` | Map the offset to the inactive partition address, program by quad-word and verify by read-back |
| CRC32 read-back | `FwUpdateNvmCrc32()` | |
| Switch boot partition | `FwUpdateNvmCommit()` | Requires enabling configuration bit `FICD_NOBTSWP` (currently disabled); write the new partition's boot sequence number, then reset |
| Capability bits | `FW_UPDATE_NVM_IMPLEMENTED` | bootCaps=0 until implemented; set to 1 only after driver completion/validation, then report 0x03 |

Until then, `BOOT_ENTER` / `BOOT_ABORT` and the session rules are already in effect: `BOOT_ERASE` returns `NOT_IMPLEMENTED`, and `BOOT_WRITE` / `BOOT_VERIFY` / `BOOT_COMMIT` return `SEQUENCE_ERR` because the state is wrong. The LLC side can already develop and test the update procedure against this behavior.

---

## 7. Version Compatibility

Protocol remains 1.0 with unchanged framing and structure sizes. Formerly reserved fault bits 15–18 now have meanings; LLC decoding should be synchronized.

- Query GET_INFO at startup. With a different major version, restrict use to GET_INFO and BOOT commands; online update still cannot complete without NVM implementation.
- Current fixed-length requests require exact sizes: LLC_STATUS 20, SET_ENABLE 1, SET_VOUT_REF 2, BOOT_ENTER/VERIFY 8 and BOOT_ERASE/COMMIT/ABORT zero bytes. Trailing fields return BAD_LEN. A minor-version bump alone cannot establish compatibility.
- Send remaining reserved bits as zero and ignore unknown received bits. New fields need compatible parsing or a new command.
- Changes to the shared wire format in `llc_protocol.h` / `llc_duty_def.h` require both projects and an appropriate version change.

## 8. LLC-Side Implementation Notes

1. **Copy** `llc_protocol.h` and the `common/llc_duty_def.h` it includes (duty-cycle encoding constants), keep the relative path and do not modify them; convert between structures and byte buffers with `memcpy`, and do not cast pointers directly onto unaligned addresses.
2. **CRC16**: reuse `MODBUS_RTU_Crc16` from the Modbus RTU directly.
3. **Main loop**:
   ```
   Power-up → GET_INFO (check version) → send LLC_STATUS every 10 ms, refresh PFC status from the response
   Insert SET_* / CLEAR_FAULT / GET_LOAD_ECHO when needed
   Only one request outstanding at a time; if the full response is not received within 20 ms, resend with the same SEQ, up to 3 times
   ```
4. **Parsing**: use a byte-by-byte state machine as on the PFC (see `LinkParseByte` in `llc_link.c`). Accept only responses with bit 7 = 1 and a SEQ matching the current request.
5. **RD6 output**: 10 kHz, duty cycle encodes the LLC output power per Section 2.1, updated at the period boundary. Even when the LLC is not running it must output a valid duty cycle (0 W corresponds to 10 %); output a constant low only before initialization and during reset.
6. **RC4 reception**: measure actual period and high time per 2.3 before qualified bus decoding; the former BUFL/10000 receiver is not the current PFC implementation.
7. **Update**: implement per the Section 6.3 procedure; first test with the current PFC firmware up to the step where `BOOT_ERASE` returns `NOT_IMPLEMENTED`.
8. **Single core or dual core**: the LLC is a dual-core device. Which core hosts the UART master is up to the LLC project: the Main core has the external serial port and spare processing capacity; the RD6 load value and the RC4 capture are on the Sec core and can be forwarded via IPC.

---

## 9. Appendix: CRC and Example Frames

### 9.1 CRC16/Modbus Reference Implementation

```c
uint16_t Crc16(const uint8_t *data, uint16_t len, uint16_t crc)   // crc = 0xFFFF to start
{
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0xA001U) : (uint16_t)(crc >> 1);
        }
    }
    return crc;   // send low byte first
}
```

| Input | Result |
|---|---|
| ASCII `"123456789"` | `0x4B37` |
| CRC32 `"123456789"` | `0xCBF43926` |

### 9.2 Example Frames (hex, usable directly as unit-test vectors)

| Description | Bytes |
|---|---|
| `GET_INFO` request, SEQ 01 | `A5 00 01 01 B1 90` |
| `GET_STATUS` request, SEQ 17 | `A5 00 17 10 7F FC` |
| `SET_VOUT_REF` 400.0 V, SEQ 18 | `A5 02 18 21 A0 0F 33 6E` |
| Response OK to the previous one | `A5 01 18 A1 00 F8 4F` |
| `SET_ENABLE` ON, SEQ 19 | `A5 01 19 20 01 08 1F` |
| `CLEAR_FAULT`, SEQ 1A | `A5 00 1A 22 FA B9` |
| Response `UNKNOWN_CMD` to an unknown command (e.g. 0x7F), SEQ 1B | `A5 01 1B FF 03 70 2E` |
| `BOOT_ENTER` {magic, 57344 B}, SEQ 30 | `A5 08 30 30 42 4F 4F 54 00 E0 00 00 7D 52` |
| Response `NOT_ALLOWED` to the previous one while the PFC is running | `A5 01 30 B0 06 F4 15` |

`LLC_STATUS` request, SEQ 20. Content: state = 4, request = `VOUT_REF_VALID`, faultFlags = 0, Vout = 12.00 V, Iout = 250.0 A, Pout = 3000 W, Tmax = 45.0 °C, voutRefReq = 400.0 V, powerLimit = 0:

```
A5 14 20 12 04 00 04 00 00 00 00 00 B0 04 C4 09 B8 0B C2 01 A0 0F 00 00 E0 5C
```

Its response `PFC_STATUS`. Content: RUN, flags = 0x00B3 (PLL_LOCKED | ENABLE_CMD | LOAD_VALID | LOAD_FF_ON | PG), no faults, Vout = 400.0 V, Vac = 230.0 V, Iin = 13.50 A, 50.00 Hz, Pin = 3105 W, 65.0 / 35.0 / 55.0 °C:

```
A5 1D 20 92 00 04 00 B3 00 00 00 00 00 00 00 00 00 A0 0F FC 08 46 05 88 13 21 0C 8A 02 5E 01 26 02 C6 D0
```

### 9.3 Parser Edge Cases (covered by the PFC implementation)

| Input | Current behavior |
|---|---|
| Noise before SOF | Skip to A5 |
| LEN>136 | Increment lenErrors and resynchronize |
| CRC error | Increment crcErrors; no response |
| Historical adjacent RX stamps ≥2 ms apart, or partial frame idle ≥2 ms with empty ring | Increment gapDrops and reset parser |
| Identical complete request within 100 ms | Increment retries; resend cache without execution |
| Same SEQ/CMD but different LEN/PAYLOAD, or expired cache | Execute new request |
| CMD bit7=1 | Ignore without refreshing link |
| Hardware RX overrun/framing error or software ring full | Count error, discard backlog and reset partial frame |
| TX ring full | Drop whole response, increment txDrops and retain cache for retry within its window |

Receive timestamps come from the 100 µs T1 byte service. Bytes drained in one FIFO service share a timestamp, so exact wire arrival times cannot be reconstructed. Measure 2 ms accuracy under service blocking/backlog. See the [review record](PFC_Code_Review_EN.md).

## Document Maintenance

- When `llc_protocol.h` or `llc_duty_def.h` is changed, both the PFC and the LLC projects must be updated at the same time and the version number raised according to the rules in Chapter 7.
- **The Chinese version ([中文](PFC_LLC_Comm_Protocol_CN.md)) and this English version have matching section numbers and must be changed together**; also update "Modified on" in the file header.

## Disclaimer

This document and the software it describes are provided by Microchip Technology Inc. "AS IS". No warranties, whether express, implied or statutory, apply, including any implied warranties of non-infringement, merchantability, or fitness for a particular purpose. The licence terms in the comment block at the top of this file govern.
