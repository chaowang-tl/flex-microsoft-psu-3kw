# PFC ↔ LLC 通信协议

| 项 | 内容 |
|---|---|
| 文件名 | PFC_LLC_Comm_Protocol_CN.md |
| 描述 | 原边 PFC 与副边 LLC 之间的 UART 协议、占空比信号和固件升级流程 |
| 原作者 | Chao Wang <chao.wang@microchip.com> |
| 创建日期 | 2026/10/31 |
| 修改人 | Chao Wang |
| 修改日期 | 2026/11/01 |
| 语言 | 中文 · [English](PFC_LLC_Comm_Protocol_EN.md) |
| 协议版本 | **1.0**（`LLC_PROTO_VERSION_MAJOR = 1`，`LLC_PROTO_VERSION_MINOR = 0`） |
| 适用对象 | 原边 PFC（dsPIC33AK256MPS206，从机）、副边 LLC（dsPIC33AH1024MPS610，主机） |
| 共享头文件 | [`sources/apps/comm/llc_protocol.h`](../sources/apps/comm/llc_protocol.h) + [`sources/common/llc_duty_def.h`](../sources/common/llc_duty_def.h)，**两边逐字节相同** |
| PFC 侧实现 | `llc_link.c`（链路/命令）、`fw_update.c`（升级会话）、`llc_signal.c`（占空比信号）、`uart_link.c`（UART1 寄存器） |
| 状态 | PFC 侧已实现；升级的 Flash 擦写/切换为占位（返回 `NOT_IMPLEMENTED`）；LLC 侧待开发 |
| 配套文档 | [PFC 软件架构手册](PFC_Software_Architecture_CN.md)（第 18 章） |

---

## 目录

1. [概述](#1-概述)（含 [1.0 导读](#10-给-llc-侧开发者的-5-分钟导读)）
2. [占空比信号（RC4 / RD6）](#2-占空比信号rc4--rd6)
3. [UART 帧格式](#3-uart-帧格式)
4. [会话规则](#4-会话规则)
5. [命令](#5-命令)
6. [固件升级](#6-固件升级)
7. [版本兼容](#7-版本兼容)
8. [LLC 侧实现要点](#8-llc-侧实现要点)
9. [附录：CRC 与示例帧](#9-附录crc-与示例帧)

---

## 1. 概述

**最终构建快照**：当前 `APP_PLL_PWM_TEST_ENABLE=1` 强制有效上电使能。测试 ISR 使用虚拟 Vac/Vout、零电流和固定 25 ℃温度，不读取真实 ADC 数据，不启动 CT 校准，也不使用真实 CT/ADC 增益校准结果作启动或 PG 联锁；增益失败诊断仍保留。实际 ADC ISR 心跳、PLL、状态计时、快速锁存、看门狗和硬件 PCI 仍工作。以下校准保护流程适用于测试开关为 0 的真实采样模式。测试 PG/遥测不能用于真实功率级判断，实板前恢复 0。

### 1.0 给 LLC 侧开发者的 5 分钟导读

1. **复制两个头文件**：`llc_protocol.h` 和它包含的 `common/llc_duty_def.h`。保持相对路径，不要修改（第 8 章）。
2. **按下面的顺序实现**，每一步都可以单独和 PFC 联调：
   - **占空比信号**（第 2 章）：输出 RD6，10 kHz，占空比随负载 10 %~90 %；捕获 RC4 得到母线电压。这一步不依赖 UART。
   - **UART 链路**（第 3、4 章）：115200 8N1，CRC16/Modbus。先发 `GET_INFO`（0x01），检查协议主版本号。
   - **周期状态**：每 10 ms 发一次 `LLC_STATUS`（0x12），应答里带回 `PFC_STATUS`。PFC 在 100 ms（`LLC_PROTO_LINK_LOST_MS`）内收不到有效帧就判定链路丢失。
   - **控制**：`SET_ENABLE`（0x20）、`SET_VOUT_REF`（0x21）、`CLEAR_FAULT`（0x22）。
   - **固件升级**（第 6 章）：最后实现。PFC 侧目前只有会话框架。
3. **单元测试**：第 9 章附录的示例帧和 CRC 都已经核对过，可以直接拿来做 LLC 侧编解码的测试用例。

PFC 和 LLC 之间有三条隔离通路：

| 通路 | 方向 | PFC 引脚 | 用途 | 刷新 |
|---|---|---|---|---|
| UART1 | 双向 | RC8 = RX，RC9 = TX | 命令、状态交换、固件升级 | LLC 轮询，10 ms |
| 占空比信号 1 | PFC → LLC | RC4（SCCP3） | 母线电压 | 10 kHz |
| 占空比信号 2 | LLC → PFC | RD6（SCCP1/2 捕获） | LLC 负载功率（PFC 电压环前馈） | 10 kHz |

分工：
- **占空比信号**传递需要连续更新的模拟量，正常负载前馈的延迟目标低于 1 ms。它能检测无边沿和超范围高电平宽度；CPU 挂起时硬件 PWM 可能继续发送旧值，所以合格捕获不证明软件仍在更新。
- **UART** 传递状态、故障、设定值和固件镜像。

```
        原边 PFC（从机）                         副边 LLC（主机）
   ┌──────────────────────┐   隔离   ┌──────────────────────┐
   │ UART1 RX  RC8  ◄─────┼────╫─────┼── TX                 │
   │ UART1 TX  RC9  ──────┼────╫─────┼─► RX                 │
   │ SCCP3     RC4  ──────┼────╫─────┼─► 占空比捕获（Vbus）  │
   │ SCCP1/2   RD6  ◄─────┼────╫─────┼── PWM 输出（负载）     │
   └──────────────────────┘          └──────────────────────┘
```

---

## 2. 占空比信号（RC4 / RD6）

### 2.1 编码

两路信号采用同一种编码：**10 kHz 推挽 PWM，高电平有效，占空比 10%~90% 线性对应 0~满量程。**

```
D = 10 % + 80 % × (X / X_FS)          X = (D − 10 %) / 80 % × X_FS
```

| 信号 | 方向 | 物理量 X | 满量程 X_FS | 分辨率（PFC 侧） |
|---|---|---|---|---|
| RC4 | PFC → LLC | PFC 母线电压 | **500 V** | 0.0625 V（1 计数 / 10000） |
| RD6 | LLC → PFC | LLC 输出功率 | **3600 W**（120 % 额定） | 0.45 W |

| 母线电压 | RC4 占空比 | 负载 | RD6 占空比 |
|---|---|---|---|
| 0 V | 10 % | 0 W | 10 % |
| 250 V | 50 % | 900 W | 30 % |
| 400 V | **74 %** | 1800 W | 50 % |
| 500 V | 90 % | 3600 W | 90 % |

### 2.2 有效性判定（接收端）

PFC 接收 RD6 必须同时满足：占空比 5%–95%、实测频率 10 kHz ±1%、最近 2 ms 有合格下降沿、已获得上升沿周期且连续 8 个完整捕获合格。失效或捕获溢出/积压失配后重新资格计数。

常低/常高判失效，不等同 0 W。5%–10% 与 90%–95% 为保护带，解码限制到 0/满量程。初始化或复位可输出常低；功率级停止后仍发送有效测量：LLC 0 W 编码 10%，PFC RC4 在 STANDBY/STOPPING/FAULT 每 1 ms 按实测母线更新。初始化 RC4 为 0%，首次更新后开始编码。有效硬件 PWM 不能证明发送端 CPU 仍运行，另需系统级新鲜度策略。

### 2.3 测量方法（接收端）

PFC RD6 使用 100 MHz SCCP 时钟：

```text
SCCP2：16 位自由运行，上升沿捕获；连续时间戳的模 65536 差 = period
        ALTSYNC=1 仍在上升沿复位 SCCP1
SCCP1：下降沿捕获 = highTime
duty = highTime / period
frequency = 100000000 / period
```

B 相 ADC ISR 轮询并各最多排空 8 个 FIFO 值，不启用 CCP 中断。首次需要两次上升沿形成周期，随后 8 个连续合格完整周期恢复有效；占空比 α=1/8 滤波。ICOV、一次轮询积压多个同类边沿导致无法可靠配对，或约 2 ms 无下降沿时，清空/失效并重新资格确认，频率诊断清 0。周期测量消除了旧的固定标称周期分母造成的比例误差。

LLC 的 RC4 接收应按本节测量实际周期和高时间。现有 `sec_pkcmc_llc` 的旧接收实现需单独同步，本轮只修改 PFC 项目，不能假定它已具备本轮 PFC 的频率/积压检查。

### 2.4 发送端要求

| 要求 | PFC（RC4） | LLC（RD6） |
|---|---|---|
| 频率 | 10 kHz（CCP3PR=9999） | 10 kHz ±1%，PFC 按实测周期检查 |
| 更新率 | 1 ms（`APP_TASKS_Task1ms`） | ≥ 1 kHz，建议跟随 LLC 控制环 |
| 更新方式 | 缓冲寄存器，在周期边界生效 | 在周期边界更新，不允许出现截断的脉冲 |
| 发送的值 | 实测母线电压（未经陷波滤波） | LLC 输出功率（Vout × Iout） |

---

## 3. UART 帧格式

### 3.1 物理层

| 参数 | 值 |
|---|---|
| 波特率 | 115200 |
| 格式 | 8 数据位，无校验，1 停止位，无流控 |
| 电平 | 3.3 V CMOS，经数字隔离器 |
| 空闲 | 高电平 |

### 3.2 帧结构

```
 字节:   0      1      2      3      4 .. 3+LEN       4+LEN     5+LEN
       ┌──────┬──────┬──────┬──────┬──────────────┬─────────┬─────────┐
       │ SOF  │ LEN  │ SEQ  │ CMD  │ PAYLOAD[LEN] │ CRC16_L │ CRC16_H │
       │ 0xA5 │0..136│      │      │              │         │         │
       └──────┴──────┴──────┴──────┴──────────────┴─────────┴─────────┘
                └───────────── CRC 覆盖范围 ─────────────┘
```

| 字段 | 长度 | 说明 |
|---|---|---|
| SOF | 1 | 固定 `0xA5` |
| LEN | 1 | PAYLOAD 字节数，0~136（`LLC_PROTO_PAYLOAD_MAX`）。超过时丢弃并重新寻找 SOF |
| SEQ | 1 | 序号。主机每发一个**新**请求加 1（0xFF 之后回到 0x00）；重发时不变；应答原样回送 |
| CMD | 1 | 请求：命令号（bit 7 = 0）。应答：请求命令号 \| `0x80` |
| PAYLOAD | LEN | 命令数据，多字节字段一律**小端** |
| CRC16 | 2 | CRC16/Modbus，覆盖 LEN、SEQ、CMD、PAYLOAD，**低字节在前** |

- 帧长 6~142 字节；在 115200 下，最长帧约 12.3 ms。
- CRC16/Modbus：多项式 0xA001（反射形式），初值 0xFFFF，无最终异或。校验值 `"123456789"` → `0x4B37`。与 LLC 工程 Modbus RTU 中的 `MODBUS_RTU_Crc16` 算法相同，可以直接复用。

### 3.3 请求与应答的对应

**每个请求恰好对应一帧应答**，按 `CMD` 和 `SEQ` 配对：

| 字段 | 请求 | 应答 |
|---|---|---|
| CMD | `c`（0x00~0x7F） | `c | 0x80` |
| SEQ | `s` | `s`（原样回送） |
| PAYLOAD | 请求参数 | `STATUS`（1 字节）+ 返回数据 |

- `STATUS = OK` 时，后面跟该命令的返回数据（长度见第 5 节）。
- `STATUS ≠ OK` 时，**只有 STATUS 一个字节**，LEN = 1。

### 3.4 状态码

| 值 | 名称 | 含义 | 主机处理 |
|---|---|---|---|
| 0x00 | `OK` | 成功 | — |
| 0x01 | `BAD_CRC` | 保留。CRC 错误的帧从机**不应答**，主机会因超时重发 | — |
| 0x02 | `BAD_LEN` | LEN 和该命令要求的长度不符 | 检查实现 |
| 0x03 | `UNKNOWN_CMD` | 命令号不存在 | 用 `GET_INFO` 检查版本 |
| 0x04 | `BAD_PARAM` | 参数超范围或魔术字不对 | 检查参数 |
| 0x05 | `BUSY` | 从机正在执行耗时操作（如擦除），稍后再发同一命令 | 用**新的 SEQ** 再发（同一 SEQ 只会拿到缓存的 BUSY 应答，见 4.3） |
| 0x06 | `NOT_ALLOWED` | 当前状态不允许，例如运行中进入升级 | 先让条件满足 |
| 0x07 | `NOT_IMPLEMENTED` | 命令已定义，但这个固件版本尚未实现 | 放弃该功能 |
| 0x08 | `FLASH_ERR` | Flash 擦写失败 | 重新擦除 |
| 0x09 | `VERIFY_ERR` | 镜像 CRC 不符 | 重新擦除并写入 |
| 0x0A | `SEQUENCE_ERR` | 升级命令顺序错误或写入偏移不连续 | 按第 6 节流程重来 |

---

## 4. 会话规则

### 4.1 主从

- **LLC 是主机**，只有主机能发起请求。PFC 是从机，只做应答，不主动发送。
- 同一时刻最多只有**一个**未完成的请求：主机收到应答或超时之后，才能发下一个。

### 4.2 时序

| 参数 | 值 | 宏 | 说明 |
|---|---|---|---|
| 帧内字节间隔 | ≤ 2 ms | `LLC_PROTO_INTERBYTE_MS` | 超过时，接收端丢弃半帧 |
| 从机开始应答 | ≤ 2 ms | `LLC_PROTO_RSP_START_MS` | 从收到请求最后一个字节算起 |
| 主机应答超时 | 20 ms | `LLC_PROTO_RSP_TIMEOUT_MS` | 从请求最后一个字节算起，到收齐应答 |
| 重发次数 | 3 | `LLC_PROTO_RETRY_MAX` | 每次重发 SEQ 不变 |
| 正常轮询周期 | 10 ms | `LLC_PROTO_POLL_PERIOD_MS` | 发送 `LLC_STATUS` |
| 链路丢失 | 100 ms | `LLC_PROTO_LINK_LOST_MS` | 这段时间内没有收到合法帧 |

```
LLC  ──[REQ seq=n]──►                         ──[REQ seq=n+1]──►
PFC              ◄──[RSP seq=n]── (≤2 ms 开始)
     |←───────────────── 10 ms ──────────────────→|
```

### 4.3 重发与去重

主站响应超时后重发完整相同请求（SEQ、CMD、LEN、PAYLOAD 全部一致）。PFC 只保存最近一个响应，缓存从执行时间起有效 100 ms；命中后 `retries++` 并重发，命令不重复执行。相同 SEQ/CMD 但长度或数据不同按新请求执行；缓存过期后也按新请求执行。主站必须在缓存窗口内完成重试，不能依赖无限去重。

合格的缓存 LLC_STATUS 重试刷新状态接收时间；成功 BOOT 命令的缓存重试刷新活跃会话时间。重试不延长缓存本身的 100 ms 寿命。缓存依据当前 T1 时基，主循环延迟补执行不会延长它。

### 4.4 链路丢失

`g_llcLinkUp` 由 CRC 合格的请求帧刷新；CMD bit7=1 的应答帧在刷新前被忽略。`g_llcStatusValid` 仅由长度合格的 LLC_STATUS（含合格缓存重试）刷新。以帧尾字节的 T1 接收时间戳计算，两者 500 ms 到期置 false；已过期标志不会因 16 位时基回绕自动恢复，必须收到新合格帧。

失联仅反映状态，不自动停 PFC；LLC_STATUS 的目标/功率限制需求字段当前也不执行。RD6 有效与 UART 链路有效是独立条件，LOAD_VALID 不能证明 LLC CPU 存活。系统若需要失联停机策略，必须明确并另行实现。

## 5. 命令

### 5.1 命令总表

| CMD | 名称 | 请求 LEN | 应答数据（STATUS 之后） | 实现状态 |
|---|---|---|---|---|
| 0x01 | `GET_INFO` | 0 | `INFO`（16 B） | ✔ |
| 0x10 | `GET_STATUS` | 0 | `PFC_STATUS`（28 B） | ✔ |
| 0x11 | `GET_LOAD_ECHO` | 0 | `LOAD_ECHO`（8 B） | ✔ |
| 0x12 | `LLC_STATUS` | 20（`LLC_STATUS`） | `PFC_STATUS`（28 B） | ✔ 只存储 |
| 0x20 | `SET_ENABLE` | 1 | — | ✔ |
| 0x21 | `SET_VOUT_REF` | 2 | — | ✔ |
| 0x22 | `CLEAR_FAULT` | 0 | — | ✔ |
| 0x30 | `BOOT_ENTER` | 8 | `BOOT_ENTER_RSP`（8 B） | ✔ |
| 0x31 | `BOOT_ERASE` | 0 | `PROGRESS`（4 B） | 框架，返回 `NOT_IMPLEMENTED` |
| 0x32 | `BOOT_WRITE` | 8 + 16~128 | `PROGRESS`（4 B） | 框架；当前返回 `SEQUENCE_ERR`（6.8） |
| 0x33 | `BOOT_VERIFY` | 8 | `PROGRESS`（4 B） | 框架；因无法进入 WRITING 状态，当前返回 `SEQUENCE_ERR`（6.8） |
| 0x34 | `BOOT_COMMIT` | 0 | — | 框架；当前返回 `SEQUENCE_ERR`（6.8） |
| 0x35 | `BOOT_ABORT` | 0 | — | ✔（从机不检查 LEN，任意负载都接受） |

0x00、0x02~0x0F、0x13~0x1F、0x23~0x2F、0x36~0x7F 未定义（保留），从机返回 `UNKNOWN_CMD`。

### 5.2 数据方向

主从关系只决定**由谁发起**，数据可以双向流动：
- **LLC → PFC**：数据放在**请求**的负载里，包括设定值（`SET_*`）、LLC 自身状态（`LLC_STATUS`）和固件镜像（`BOOT_WRITE`）。
- **PFC → LLC**：数据放在**应答**的负载里。

正常运行时 LLC 只需每 10 ms 发一次 `LLC_STATUS`：请求里带上 LLC 的状态，应答里返回 PFC 的状态，**一次往返就完成双向交换**。

### 5.3 定点格式

| 物理量 | 单位 | 类型 | 例 |
|---|---|---|---|
| 电压 | 0.1 V | uint16 | 4000 = 400.0 V |
| LLC 输出电压 | 0.01 V | uint16 | 1200 = 12.00 V |
| 电流（PFC 输入） | 0.01 A | uint16 | 1350 = 13.50 A |
| 电流（LLC 输出） | 0.1 A | uint16 | 2500 = 250.0 A |
| 频率 | 10 kHz（CCP3PR=9999） | 10 kHz ±1%，PFC 按实测周期检查 |
| 功率 | 1 W | uint16 | 3105 = 3105 W |
| 温度 | 0.1 °C | int16 | 650 = 65.0 °C |
| 比例 | ‰ | uint16 | 740 = 74.0 % |

超出类型范围的值饱和（不回绕）。

### 5.4 `GET_INFO`（0x01）

请求：无负载。应答 `LLC_PROTO_INFO_T`（16 字节）：

| 偏移 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 0 | `protoMajor` | u8 | 协议主版本，本文档为 1 |
| 1 | `protoMinor` | u8 | 协议次版本，本文档为 0 |
| 2 | `fwMajor` | u8 | PFC 固件版本（`APP_FW_VERSION_*`） |
| 3 | `fwMinor` | u8 | |
| 4 | `fwBuild` | u16 | |
| 6 | `activePartition` | u8 | 当前运行的 Flash 分区：1 或 2 |
| 7 | `bootCaps` | u8 | 当前 NVM 未实现为 0；实现并启用后为 0x03（bit0 会话，bit1 Flash） |
| 8 | `imageCrc32` | u32 | 当前镜像 CRC32，未计算时为 0 |
| 12 | `reserved` | u32 | 0 |

LLC 上电后应先发 `GET_INFO` 检查 `protoMajor`（见第 7 节）。

### 5.5 `GET_STATUS`（0x10）与 `PFC_STATUS` 结构

请求：无负载。应答 `LLC_PROTO_PFC_STATUS_T`（28 字节）：

| 偏移 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 0 | `state` | u8 | PFC 状态机，见下表 |
| 1 | `reserved0` | u8 | 0 |
| 2 | `flags` | u16 | 见下表 |
| 4 | `faultLatched` | u32 | 锁存的故障位图（PFC `FAULTLOG_FLAGS_e`） |
| 8 | `faultLockedOut` | u32 | 已锁死（需复位）的故障位图 |
| 12 | `voutV10` | u16 | 母线电压，0.1 V |
| 14 | `vacRmsV10` | u16 | 电网电压有效值，0.1 V |
| 16 | `iinRmsA100` | u16 | 输入电流有效值，0.01 A |
| 18 | `freqHz100` | u16 | 电网频率，0.01 Hz |
| 20 | `pinW` | u16 | 输入视在功率估算（Vrms × Irms），W |
| 22 | `tempGanC10` | i16 | GaN 温度，0.1 °C |
| 24 | `tempInletC10` | i16 | 进风温度，0.1 °C |
| 26 | `tempMaxC10` | i16 | 其余温度传感器中的最大值，0.1 °C |

`state`：

| 值 | 状态 | 值 | 状态 |
|---|---|---|---|
| 0 | INIT | 4 | RUN |
| 1 | STANDBY | 5 | STOPPING |
| 2 | PREPARE | 6 | FAULT |
| 3 | SOFTSTART | | |

`flags`：

| 位 | 名称 | 含义 |
|---|---|---|
| 0 | `PLL_LOCKED` | 电网锁相 |
| 1 | `ENABLE_CMD` | PFC 使能命令为 1 |
| 2 | `FAULT_ACTIVE` | 有故障锁存或快速保护待处理 |
| 3 | `FAULT_LOCKED` | 有故障已锁死，必须复位 PFC |
| 4 | `LOAD_VALID` | PFC 收到的 RD6 负载信号有效 |
| 5 | `LOAD_FF_ON` | 编译开关开启、RD6 有效且当前前馈贡献 >0；不单独表示环路运行 |
| 6 | `BOOT_SESSION` | 升级会话进行中 |
| 7 | `PG` | RUN、enable、控制准入、校准与 PLL/健康合格、无故障/快锁存、母线有限且在目标 ±10 V 内；RUN 入口要求连续 5 ms。真实模式要求校准成功；测试模式为虚拟母线并绕过真实校准 |
| 8~15 | — | 保留，为 0 |

故障掩码 bit0–14 含义保持；bit15 ISR_HEALTH、16 CALIBRATION、17 STARTUP_TIMEOUT，bit18–31 保留。定义见 [fault.h](../sources/apps/fault/fault.h)。

状态、标志、故障掩码与原始测量在短暂中断屏蔽期间复制，恢复后定点编码；掩码包含待转写快速故障。温度模型仍为占位，pinW 是视在功率估算；NaN/Inf 编码为 0，不表示测量有效。

### 5.6 `GET_LOAD_ECHO`（0x11）

用于在调试时**闭环校验 RD6 通道**：LLC 比较自己发出的负载值和 PFC 实际收到的值。

请求：无负载。应答 `LLC_PROTO_LOAD_ECHO_T`（8 字节）：

| 偏移 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 0 | `dutyPermille` | u16 | PFC 测得的占空比（滤波后），‰ |
| 2 | `freqHz` | u16 | 实测 RD6 上升沿周期换算 Hz；超时/溢出/积压失配时 0 |
| 4 | `loadW` | u16 | PFC 解码出的负载，W（失效时为 0） |
| 6 | `valid` | u8 | 1 = 有效 |
| 7 | `reserved` | u8 | 0 |

### 5.7 `LLC_STATUS`（0x12）

LLC 的主轮询命令，每 10 ms 一次。请求 `LLC_PROTO_LLC_STATUS_T`（20 字节）：

| 偏移 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 0 | `state` | u8 | LLC 状态码，由 LLC 定义 |
| 1 | `reserved0` | u8 | 0 |
| 2 | `request` | u16 | LLC 对 PFC 的需求，见下表 |
| 4 | `faultFlags` | u32 | LLC 故障位图，由 LLC 定义 |
| 8 | `voutV100` | u16 | LLC 输出电压，0.01 V |
| 10 | `ioutA10` | u16 | LLC 输出电流，0.1 A |
| 12 | `poutW` | u16 | LLC 输出功率，W |
| 14 | `tempMaxC10` | i16 | LLC 最高温度，0.1 °C |
| 16 | `voutRefReq` | u16 | 请求的 PFC 母线电压，0.1 V（`REQ_VOUT_REF_VALID` 时有效） |
| 18 | `powerLimitW` | u16 | 请求的 PFC 功率上限，W（`REQ_PFC_DERATE` 时有效） |

`request`：

| 位 | 名称 | 含义 |
|---|---|---|
| 0 | `PFC_DERATE` | 要求 PFC 限功率到 `powerLimitW` |
| 1 | `PFC_SHUTDOWN` | 要求 PFC 停机 |
| 2 | `VOUT_REF_VALID` | `voutRefReq` 有效 |
| 3~15 | — | 保留，为 0 |

应答：同 `GET_STATUS`（`PFC_STATUS`，28 字节）。

> **协议 1.0 的 PFC 行为**：PFC 保存整帧数据到 `g_llcStatus` 并刷新时间戳（`g_llcStatusValid`），但**不执行 `request` 里的需求**。母线跟随、降额和停机策略留待后续版本定义；在那之前，控制 PFC 请使用 `SET_ENABLE` / `SET_VOUT_REF`。

### 5.8 `SET_ENABLE`（0x20）

请求 1 字节：`0x00` = 关闭，`0x01` = 使能。应答：仅 STATUS。

| 情况 | STATUS |
|---|---|
| 0x00 | `OK`，PFC 进入停机流程 |
| 0x01 | `OK`，PFC 按状态机条件（PLL 锁定、预充电完成）启动 |
| 0x01，且升级会话进行中 | `NOT_ALLOWED` |
| 其他值 | `BAD_PARAM` |

`OK` 表示命令已被接受，不代表 PFC 已经进入 RUN。启动进度请看 `PFC_STATUS.state`。

### 5.9 `SET_VOUT_REF`（0x21）

请求 2 字节：u16 母线电压目标值，单位 0.1 V。范围 3500~4100（350.0~410.0 V），超出返回 `BAD_PARAM`。应答：仅 STATUS。PFC 以 90 V/s 的斜率过渡到新目标值。

### 5.10 `CLEAR_FAULT`（0x22）

请求无负载。仅允许 `state=FAULT/STANDBY` 且 enable=0，否则 NOT_ALLOWED。建议先 SET_ENABLE 0，再查询确认停止状态，排除根因后发 CLEAR_FAULT。

OK 仅表示请求已排队到 T1，不保证锁存已经清除。T1 要求控制关闭、非 lockedOut、慢故障已恢复消抖且当前有限值越过迟滞恢复门限；快电流两路均 <33 A、快母线 <421 V，事件故障 active 已清。保留检测历史与待转写快速故障；未恢复项保持。lockedOut 必须 MCU 复位，硬件根因应先排除。查询 GET_STATUS 确认 faultLatched/faultLockedOut 与状态。

## 6. 固件升级

### 6.1 原理：A/B 双分区

> **实现状态**：以下分区写入/切换描述是升级设计目标。当前 `FwUpdateNvm*()` 仍为未实现桩函数，且分区切换配置禁用；不能把设计机制理解为已验证的断电恢复或升级安全保证。见 6.8。

PFC 的 Flash 配置为**双启动分区**（`FBOOT_BTMODE = DUAL`），每个分区 0x1F000 字节（124 KB，dsPIC33AK256MPS206）。

```
  ┌───────────────────────┐   ┌───────────────────────┐
  │ 分区 1（正在运行）      │   │ 分区 2（非活动）        │
  │ 旧固件，升级中继续运行   │   │ ← 新镜像写到这里        │
  └───────────────────────┘   └───────────────────────┘
          校验通过并 COMMIT 后，下次启动从分区 2 运行
```

- 设计要求新镜像只写入**非活动分区**，保留当前可启动镜像；实际写地址、镜像边界与配置字保护须在 NVM 驱动中实现并验证。
- 设计要求擦写、校验失败或提交前断电时继续使用旧固件；当前尚无 NVM/提交实现和断电故障注入结果，因此不保证任意失败下都可恢复。
- 非活动分区的 RWW 与擦写期间 T1/UART 服务延迟，需结合实际 NVM 驱动、执行位置、缓存及 WDT 行为测量；当前桩函数不能证明通信实时性。

### 6.2 前置条件

| 条件 | 说明 |
|---|---|
| PFC 已停机 | `state` 为 STANDBY 或 FAULT，否则 `BOOT_ENTER` 返回 `NOT_ALLOWED`。LLC 应先发 `SET_ENABLE 0`，等 PFC 进入 STANDBY |
| 魔术字 | `BOOT_ENTER.magic = 0x544F4F42`（"BOOT"） |
| 镜像大小 | 16~0x1F000 字节，必须是 16 的整数倍（0 或不对齐返回 `BAD_PARAM`） |
| 会话超时 | 最近成功 BOOT 命令（含缓存成功重试）起 5 s 无活动即结束；使用实际 T1 时基，非法请求不保活，保持关闭 |
| 运行中保护 | 会话期间 `SET_ENABLE 1` 返回 `NOT_ALLOWED` |

### 6.3 流程

```
LLC                                         PFC
 │── SET_ENABLE 0 ──────────────────────────►│ 停机
 │◄─────────────────────────────────── OK ───│
 │   （轮询 GET_STATUS 直到 state = STANDBY）  │
 │── BOOT_ENTER {magic, imageSize} ─────────►│ IDLE → ACTIVE，锁住使能
 │◄──── OK {targetPartition, 128, 16, max} ──│
 │── BOOT_ERASE ────────────────────────────►│ 擦除非活动分区（可能返回 BUSY，重发直到 OK）
 │◄──────────────────── OK {imageSize} ──────│ ACTIVE → ERASED
 │── BOOT_WRITE {offset=0, data[128]} ──────►│ ERASED → WRITING
 │◄──────────────────── OK {next=128} ───────│
 │── BOOT_WRITE {offset=128, ...} ──────────►│ 偏移必须严格连续
 │        ……（共 imageSize/128 块）……         │
 │── BOOT_VERIFY {imageSize, crc32} ────────►│ 回读计算 CRC32
 │◄──────────────────── OK {crc32} ──────────│ WRITING → VERIFIED
 │── BOOT_COMMIT ───────────────────────────►│ 设置下次从新分区启动
 │◄─────────────────────────────────── OK ───│ 发完应答后复位
 │   （等待 PFC 重启，GET_INFO 检查新版本和 activePartition）
```

任一步失败或需要放弃时，发 `BOOT_ABORT`，或者从 `BOOT_ENTER` 重新开始。

### 6.4 会话状态机（PFC）

```
          BOOT_ENTER                 BOOT_ERASE OK            BOOT_WRITE OK
  IDLE ─────────────► ACTIVE ────────────────────► ERASED ─────────────────► WRITING ─┐
   ▲                    ▲  ▲                                                  │  ▲    │ BOOT_WRITE OK
   │                    │  └──── 擦/写失败（FLASH_ERR）─────────────────────────┘  └────┘
   │                    │                                                     │
   │ BOOT_ABORT /       │                                         BOOT_VERIFY OK
   │ 5 s 超时（任意状态） │                                                     ▼
   └────────────────────┴───────────────────────────────────────────────── VERIFIED
                                                                              │ BOOT_COMMIT OK
                                                                              ▼
                                                                         复位，从新分区启动
```

`BOOT_ENTER` 在任意状态下都可以发送，会重新开始会话（镜像需要重新擦写）。

### 6.5 命令细节

**`BOOT_ENTER`（0x30）** 请求 8 字节 `LLC_PROTO_BOOT_ENTER_T`：

| 偏移 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 0 | `magic` | u32 | `0x544F4F42` |
| 4 | `imageSize` | u32 | 镜像字节数 |

应答 8 字节 `LLC_PROTO_BOOT_ENTER_RSP_T`：

| 偏移 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 0 | `targetPartition` | u8 | 将要写入的分区（1 或 2） |
| 1 | `writeMax` | u8 | 每块最大数据字节数，128 |
| 2 | `writeAlign` | u8 | 偏移和长度的对齐要求，16 |
| 3 | `reserved` | u8 | 0 |
| 4 | `imageMax` | u32 | 最大镜像字节数，0x1F000（124 KB） |

**`BOOT_ERASE`（0x31）** 请求：无负载。应答 `PROGRESS`：`BOOT_ENTER` 声明的镜像大小（即需要擦除覆盖的范围）。擦除耗时超过应答时间时，从机返回 `BUSY`，主机用**新的 SEQ** 重发 `BOOT_ERASE`，直到返回 `OK`。

**`BOOT_WRITE`（0x32）** 请求 = 8 字节头 `LLC_PROTO_BOOT_WRITE_T` + 数据：

| 偏移 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 0 | `offset` | u32 | 在镜像中的字节偏移，必须等于上一次应答的 `next` |
| 4 | `reserved` | u32 | 0 |
| 8 | `data` | 16~128 B | 长度为 16 的倍数 |

应答 `PROGRESS`：下一块应写入的偏移 `next`。

- 偏移不连续时返回 `SEQUENCE_ERR`；写入超出 `imageSize` 时返回 `BAD_PARAM`。
- 写入失败时返回 `FLASH_ERR`，会话回到 ACTIVE，需要重新擦除。
- 应答丢失时，主机用**相同 SEQ** 重发，从机返回缓存的应答（第 4.3 节），不会重复写入。

**`BOOT_VERIFY`（0x33）** 请求 8 字节 `LLC_PROTO_BOOT_VERIFY_T`：

| 偏移 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 0 | `imageSize` | u32 | 必须与 `BOOT_ENTER` 中一致，且已全部写完 |
| 4 | `imageCrc32` | u32 | 整个镜像的 CRC32 |

应答 `PROGRESS`：匹配时返回 PFC 计算出的 CRC32。不匹配时返回 `VERIFY_ERR`，**只有状态字节、不带数据**（按 3.3 节的通用规则，非 OK 应答 LEN 不含数据）。

CRC32 算法为 CRC-32/ISO-HDLC（即 zlib 的 `crc32`）：多项式 0xEDB88320（反射），初值和最终异或值都是 0xFFFFFFFF。校验值 `"123456789"` → `0xCBF43926`。

**`BOOT_COMMIT`（0x34）** 请求：无负载。只能在 VERIFIED 状态下发送，否则返回 `SEQUENCE_ERR`。从机先发完 `OK` 应答，再复位。复位后从新分区启动，约 1 s 后可以重新通信。

**`BOOT_ABORT` (0x35)**：请求严格 0 字节，非空返回 BAD_LEN 且不改变会话；合格请求返回 OK 并回 IDLE，不激活已写数据。

### 6.6 镜像格式

- 镜像是 PFC 工程链接生成的**单分区程序映像**：从分区起始地址开始的连续字节，长度向上补齐到 16 字节，不足部分填 0xFF。
- 由 `.hex` 转换为二进制镜像的工具、镜像头（版本、CRC）的格式，在实现 NVM 驱动时一并确定，届时把协议升为 1.1。

### 6.7 耗时估算

下表仅估算约 60 KB 图像的串口传输时间，不是当前 Flash 驱动的性能测量；实际链接用量见审查记录，单分区图像格式与最终长度仍待定义。

| 项 | 估算 |
|---|---|
| 每块请求帧 142 B + 应答帧 11 B，115200 bps | ≈ 13.3 ms |
| 每块 Flash 写入（约 8 个 quad-word） | < 1 ms |
| 写入合计 | ≈ 6 s |
| 擦除（约 15 页） | < 1 s |
| 校验 + 提交 + 重启 | ≈ 1.5 s |
| **总计** | **约 8~9 s** |

### 6.8 PFC 侧尚未实现的部分

| 项 | 位置 | 说明 |
|---|---|---|
| 擦除非活动分区 | `FwUpdateNvmErase()` | 需要 NVM 驱动；耗时较长时分页执行并返回 `BUSY` |
| 写入 | `FwUpdateNvmWrite()` | 偏移映射到非活动分区地址，按 quad-word 编程并回读校验 |
| CRC32 回读 | `FwUpdateNvmCrc32()` | |
| 切换启动分区 | `FwUpdateNvmCommit()` | 需要打开配置位 `FICD_NOBTSWP`（当前禁用）；写入新分区的启动序列号后复位 |
| 能力位 | `FW_UPDATE_NVM_IMPLEMENTED` | 未实现时 bootCaps=0；驱动与验证全部完成后设置 1，返回 0x03 |

在这之前，`BOOT_ENTER` / `BOOT_ABORT` 和会话规则都已生效：`BOOT_ERASE` 返回 `NOT_IMPLEMENTED`，`BOOT_WRITE` / `BOOT_VERIFY` / `BOOT_COMMIT` 因为状态不对返回 `SEQUENCE_ERR`。LLC 侧可以先据此开发和联调升级流程。

---

## 7. 版本兼容

当前协议版本保持 1.0；帧格式与结构长度未变，原保留故障位 15–18 已有含义，LLC 解码应同步。

- 上电先 GET_INFO；主版本不匹配时仅使用 GET_INFO 和 BOOT 命令（NVM 未实现时仍不能完成在线升级）。
- 当前固定长度请求必须精确匹配：LLC_STATUS 20、SET_ENABLE 1、SET_VOUT_REF 2、BOOT_ENTER/VERIFY 8，BOOT_ERASE/COMMIT/ABORT 0 字节。追加字段返回 BAD_LEN；不能仅提升 minor 就假定兼容。
- 剩余保留位发送 0，接收端忽略未知位；新字段需要兼容解析或新命令。
- 修改共享 `llc_protocol.h` / `llc_duty_def.h` 的线格式时同步两工程，并按实际兼容性升级版本。

## 8. LLC 侧实现要点

1. **复制** `llc_protocol.h` 和它包含的 `common/llc_duty_def.h`（占空比编码常数），保持相对路径，不要改；结构体用 `memcpy` 和字节缓冲区互转，不要直接对未对齐的地址做指针强转。
2. **CRC16** 直接复用 Modbus RTU 的 `MODBUS_RTU_Crc16`。
3. **主循环**：
   ```
   上电 → GET_INFO（检查版本）→ 每 10 ms 发 LLC_STATUS，用应答刷新 PFC 状态
   需要时插入 SET_* / CLEAR_FAULT / GET_LOAD_ECHO
   一次只挂起一个请求；20 ms 内没收齐应答，就用同一 SEQ 重发，最多 3 次
   ```
4. **解析**：和 PFC 一样用逐字节状态机（参考 `llc_link.c` 中的 `LinkParseByte`）。只接受 bit 7 为 1 且 SEQ 与当前请求一致的应答。
5. **RD6 输出**：10 kHz，占空比按第 2.1 节编码 LLC 输出功率，在周期边界更新。LLC 未运行时也要输出有效的占空比（0 W 对应 10 %），只有在初始化之前和复位时才输出常低。
6. **RC4 接收**：按 2.3 同时测实际周期和高时间，资格合格后解码母线，勿把旧 BUFL/10000 方法当成本轮 PFC 实现。
7. **升级**：按第 6.3 节流程实现，先用当前的 PFC 固件联调到 `BOOT_ERASE` 返回 `NOT_IMPLEMENTED` 这一步。
8. **单核还是双核**：LLC 是双核芯片。UART 主机放在哪个核由 LLC 工程决定：Main 核有外部串口和空闲算力；RD6 的负载值和 RC4 的捕获在 Sec 核，可以经 IPC 转发。

---

## 9. 附录：CRC 与示例帧

### 9.1 CRC16/Modbus 参考实现

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

| 输入 | 结果 |
|---|---|
| ASCII `"123456789"` | `0x4B37` |
| CRC32 `"123456789"` | `0xCBF43926` |

### 9.2 示例帧（十六进制，可直接用作单元测试向量）

| 说明 | 字节 |
|---|---|
| `GET_INFO` 请求，SEQ 01 | `A5 00 01 01 B1 90` |
| `GET_STATUS` 请求，SEQ 17 | `A5 00 17 10 7F FC` |
| `SET_VOUT_REF` 400.0 V，SEQ 18 | `A5 02 18 21 A0 0F 33 6E` |
| 上一条的应答 OK | `A5 01 18 A1 00 F8 4F` |
| `SET_ENABLE` ON，SEQ 19 | `A5 01 19 20 01 08 1F` |
| `CLEAR_FAULT`，SEQ 1A | `A5 00 1A 22 FA B9` |
| 对未知命令（如 0x7F）的应答 `UNKNOWN_CMD`，SEQ 1B | `A5 01 1B FF 03 70 2E` |
| `BOOT_ENTER` {magic, 57344 B}，SEQ 30 | `A5 08 30 30 42 4F 4F 54 00 E0 00 00 7D 52` |
| 上一条在 PFC 运行中时的应答 `NOT_ALLOWED` | `A5 01 30 B0 06 F4 15` |

`LLC_STATUS` 请求，SEQ 20。内容：state = 4，request = `VOUT_REF_VALID`，faultFlags = 0，Vout = 12.00 V，Iout = 250.0 A，Pout = 3000 W，Tmax = 45.0 °C，voutRefReq = 400.0 V，powerLimit = 0：

```
A5 14 20 12 04 00 04 00 00 00 00 00 B0 04 C4 09 B8 0B C2 01 A0 0F 00 00 E0 5C
```

它的应答 `PFC_STATUS`。内容：RUN，flags = 0x00B3（PLL_LOCKED | ENABLE_CMD | LOAD_VALID | LOAD_FF_ON | PG），无故障，Vout = 400.0 V，Vac = 230.0 V，Iin = 13.50 A，50.00 Hz，Pin = 3105 W，65.0 / 35.0 / 55.0 °C：

```
A5 1D 20 92 00 04 00 B3 00 00 00 00 00 00 00 00 00 A0 0F FC 08 46 05 88 13 21 0C 8A 02 5E 01 26 02 C6 D0
```

### 9.3 解析器的边界情况（PFC 实现已覆盖）

| 输入 | 当前行为 |
|---|---|
| SOF 前噪声 | 丢弃到 A5 |
| LEN>136 | lenErrors 增加并重新找 SOF |
| CRC 错 | crcErrors 增加，不应答 |
| 相邻历史 RX 时间戳间隔 ≥2 ms，或环空后半帧闲置 ≥2 ms | gapDrops 增加并重置解析器 |
| 100 ms 内完整请求一致 | retries 增加，重发缓存，不重复执行 |
| 相同 SEQ/CMD 但 LEN/PAYLOAD 不同，或缓存过期 | 执行新请求 |
| CMD bit7=1 | 忽略且不刷新链路 |
| RX 硬件溢出/帧错误或软件环满 | 记录错误，丢弃积压并重置半帧 |
| TX 环满 | 整帧丢弃、txDrops 增加，保留缓存供窗口内重试 |

RX 时间戳来自 100 µs T1 字节服务，同次 FIFO 服务的字节共用时间戳，因此不能还原精确线级到达时间；需实测阻塞/积压下 2 ms 判定精度。见 [审查记录](PFC_Code_Review_CN.md)。

## 文档维护规则

- 修改 `llc_protocol.h` 或 `llc_duty_def.h` 时，PFC 和 LLC 两个工程必须同时更新，并按第 7 章的规则升级版本号。
- **中文版与[英文版](PFC_LLC_Comm_Protocol_EN.md)章节编号一一对应，必须同时修改**，并更新文件头的"修改日期"。

## 免责声明

本文档及其描述的软件由 Microchip Technology Inc. 提供，按"现状"提供，不作任何明示、暗示或法定的保证，包括但不限于对不侵权、适销性和特定用途适用性的暗示保证。使用条款以源文件头中的英文声明为准。
