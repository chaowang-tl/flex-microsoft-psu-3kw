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
* File Name       : fault.h
* Description     : PFC fault management with auto-recovery and fast trip
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/22
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#ifndef FAULT_H
#define FAULT_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Include
#include <xc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../control/smps_control.h"

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
#define FLT_MAX_COUNT               32

// Slow (T1, debounced) thresholds, PU = physical value / base
// TODO: set from the product specification
#define FLT_VAC_OVP_THRESHOLD       (380.0f / VAC_BASE)     // Vpk
#define FLT_VAC_OVP_HYSTERESIS      (12.0f / VAC_BASE)
#define FLT_VAC_UVP_THRESHOLD       (128.0f / VAC_BASE)     // Vpk (~90 Vrms)
#define FLT_VAC_UVP_HYSTERESIS      (12.0f / VAC_BASE)
#define FLT_VOUT_OVP_THRESHOLD      (430.0f / VOUT_BASE)    // V, 20 V above the 410 V ref limit
#define FLT_VOUT_OVP_HYSTERESIS     (9.0f / VOUT_BASE)
#define FLT_VOUT_UVP_THRESHOLD      (315.0f / VOUT_BASE)    // V, RUN only
#define FLT_VOUT_UVP_HYSTERESIS     (13.5f / VOUT_BASE)
// CT range is +-50 A (BOARD_FB_IL_RANGE_A); fault.c checks the fast OCP fits
#define FLT_IL_OCP_THRESHOLD        (30.0f / IL_BASE)       // A, filtered per-phase |iL| (90 Vac full load: 23.6 A)
#define FLT_IL_OCP_HYSTERESIS       (2.0f / IL_BASE)
#define FLT_FREQ_HI_THRESHOLD       1.30f       // 65 Hz with FREQ_BASE 50 Hz
#define FLT_FREQ_HI_HYSTERESIS      0.02f
#define FLT_FREQ_LO_THRESHOLD       0.90f       // 45 Hz
#define FLT_FREQ_LO_HYSTERESIS      0.02f
#define FLT_OTP_THRESHOLD           4.00f       // 100 degC with TEMP_BASE 25 degC
#define FLT_OTP_HYSTERESIS          0.40f

#define FLT_DEBOUNCE_SET_DEFAULT    5           // x 100 us
#define FLT_DEBOUNCE_CLR_DEFAULT    10

// Fast (65 kHz ISR, instantaneous) thresholds, PU = physical value / base
#define FLT_FAST_IL_OCP_THRESHOLD   (35.0f / IL_BASE)       // A, per switch (90 Vac full load peak with ripple: ~26 A)
#define FLT_FAST_VOUT_OVP_THRESHOLD (440.0f / VOUT_BASE)    // V, 10 V below the 450 V bulk cap rating

//------------------------------------------------------------------------------
// Public Enumerated and Structure Definitions
typedef enum {
    FLT_VAC_OVP = 0,        //  0 - Grid voltage OVP
    FLT_VAC_UVP,            //  1 - Grid voltage UVP / brown-out
    FLT_VOUT_OVP,           //  2 - Output voltage OVP (slow)
    FLT_VOUT_UVP,           //  3 - Output voltage UVP (RUN only)
    FLT_IA_OCP,             //  4 - Phase-A filtered OCP
    FLT_IB_OCP,             //  5 - Phase-B filtered OCP
    FLT_FREQ_HI,            //  6 - Grid frequency high
    FLT_FREQ_LO,            //  7 - Grid frequency low
    FLT_OTP_GAN,            //  8 - GaN heatsink over-temperature
    FLT_OTP_INLET,          //  9 - Inlet over-temperature
    FLT_OTP_LLCA,           // 10 - LLC-A over-temperature
    FLT_OTP_LLCB,           // 11 - LLC-B over-temperature
    FLT_FAST_IA_OCP,        // 12 - Phase-A instantaneous OCP (ISR)
    FLT_FAST_IB_OCP,        // 13 - Phase-B instantaneous OCP (ISR)
    FLT_FAST_VOUT_OVP,      // 14 - Output instantaneous OVP (ISR)
    FLT_ISR_HEALTH,         // 15 - ADC ISR heartbeat failure
    FLT_CALIBRATION,        // 16 - ADC/CT calibration failed
    FLT_STARTUP_TIMEOUT,    // 17 - PREPARE/SOFTSTART timed out
    FLT_RESERVED_18,        // 18
    FLT_RESERVED_19,        // 19
    FLT_RESERVED_20,        // 20
    FLT_RESERVED_21,        // 21
    FLT_RESERVED_22,        // 22
    FLT_RESERVED_23,        // 23
    FLT_RESERVED_24,        // 24
    FLT_RESERVED_25,        // 25
    FLT_RESERVED_26,        // 26
    FLT_RESERVED_27,        // 27
    FLT_RESERVED_28,        // 28
    FLT_RESERVED_29,        // 29
    FLT_RESERVED_30,        // 30
    FLT_RESERVED_31,        // 31
    FLT_COUNT               // 32
} FAULTLOG_FLAGS_e;

typedef enum {
    FLT_FAST_ID_IA_OCP = 0,
    FLT_FAST_ID_IB_OCP,
    FLT_FAST_ID_VOUT_OVP,
    FLT_FAST_ID_COUNT
} FAULT_FAST_ID_e;

typedef enum {
    FLT_RECOVERY_NONE = 0,
    FLT_RECOVERY_LIMITED,
    FLT_RECOVERY_UNLIMITED
} FAULT_RECOVERY_MODE_e;

typedef struct {
    FAULT_RECOVERY_MODE_e recoveryMode;
    uint16_t maxRetries;
} FAULT_CONFIG_T;

typedef struct {
    bool     active;
    bool     latched;
    bool     lockedOut;
    uint16_t retryCount;
} FAULT_STATE_T;

typedef struct {
    FAULT_CONFIG_T config[FLT_MAX_COUNT];
    FAULT_STATE_T  state[FLT_MAX_COUNT];
    uint32_t       activeFlags;
    uint32_t       latchedFlags;
    uint32_t       lockedOutFlags;
} FAULT_OBJ_T;

//------------------------------------------------------------------------------
// Conditional Definitions
#ifdef FAULT_C
    #define FAULT_C_PUBLIC
    #define FAULT_C_CONST
#else
    #define FAULT_C_PUBLIC extern
    #define FAULT_C_CONST  const
#endif

//------------------------------------------------------------------------------
// Public Variables
FAULT_C_PUBLIC volatile FAULT_OBJ_T g_fault;

// Written true only by the 65 kHz ISRs, cleared only by T1 (byte access, no lock)
FAULT_C_PUBLIC volatile bool g_faultFastTrip[FLT_FAST_ID_COUNT];

//------------------------------------------------------------------------------
// Public Function Prototypes
FAULT_C_PUBLIC void     FAULT_Init(void);
FAULT_C_PUBLIC void     FAULT_Set(FAULTLOG_FLAGS_e id);
FAULT_C_PUBLIC void     FAULT_Clear(FAULTLOG_FLAGS_e id);
FAULT_C_PUBLIC bool     FAULT_ClearAll(void);
FAULT_C_PUBLIC void     FAULT_Evaluate(bool monitorVoutUvp);
FAULT_C_PUBLIC bool     FAULT_IsActive(void);
FAULT_C_PUBLIC uint32_t FAULT_GetLatchedFlags(void);
FAULT_C_PUBLIC uint32_t FAULT_GetLockedOutFlags(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* FAULT_H */
