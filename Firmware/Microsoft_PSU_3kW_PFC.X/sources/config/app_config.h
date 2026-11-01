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
* File Name       : app_config.h
* Description     : PFC build-time application configuration
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/10/28
* Modified by     : Chao Wang
* Modified on     : 2026/11/01
*******************************************************************************/
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions

// All switches below use 0 / 1. Do not write true / false: inside #if they are
// not defined without <stdbool.h> and silently evaluate to 0.

// Converter enable command at power-up. Keep 0 on a new board so PWM stays
// off until g_sm.enableCommand is set from X2Cscope.
#define APP_PFC_ENABLE_AT_STARTUP     0

// PLL / PWM bench test: 1 = in the ADC ISRs replace the measured grid voltage
// with an internal 50 Hz, 1.0 PU sine, hold g_pfcVoutPU at 1.0 PU (400 V) and
// force the four inductor currents to 0 (no power stage, CTs float).
// Temperatures are fixed at 25 C. Physical ADC calibration admission is
// bypassed; real ADC ISR cadence, PLL, state timing, watchdog and PCI stay active.
// TEST ONLY - PLL, polarity, current loop, feed-forward and faults all see the
// synthetic values; bulk OVP/UVP can never trip. Keep 0 on real hardware.
// Also forces APP_PFC_ENABLE_AT_STARTUP to 1.
#define APP_PLL_PWM_TEST_ENABLE       1

#if APP_PLL_PWM_TEST_ENABLE
#warning "APP_PLL_PWM_TEST_ENABLE = 1: synthetic vac / Vout, PFC enabled at power-up, bench test only"
// The bench test needs PWM running without X2Cscope: force the enable on
#undef  APP_PFC_ENABLE_AT_STARTUP
#define APP_PFC_ENABLE_AT_STARTUP     1
#endif

// Voltage-loop load feed-forward from the LLC load signal (RD6): 1 = add the
// decoded LLC load to the voltage-loop power demand, 0 = PI only. The
// feed-forward is zero whenever the RD6 signal is invalid.
#define APP_PFC_LOAD_FF_ENABLE        1

// Application safety settings (100 us T1 tick, 800 MHz PWM clock)
#define APP_PFC_PWM_DEADTIME_NS       200U
#define APP_PFC_PG_WINDOW_V           10.0f
#define APP_PFC_STARTUP_TIMEOUT_MS    10000UL
#define APP_PFC_RELAY_SETTLE_MS       10U

// Analog outputs [V], 3.3 V reference, usable range ~0.165..3.13 V
// (DACDAT 0x0CD..0xF32); applied at start-up by CmpInitExt (mcc_ext.c)
#define APP_DAC_VREF_RA8_V            3.135f  // RA8 = DACOUT2 <- DAC8 (CMP8): external VREF (DACDAT 3890)
#define APP_DAC_DEBUG_RA9_V           3.135f  // RA9 = UREF <- DAC7 (CMP7): debug output (DACDAT 3890)

// X-capacitor discharge on RA4 (high = discharge), grid_meas.c. AC removed =
// no zero crossing for APP_XCAP_NO_CROSS_MS while |vac| > APP_XCAP_VAC_ON_V.
// TODO: check against the X-cap value, bleeder resistor and safety standard
#define APP_XCAP_ENABLE               1
#define APP_XCAP_NO_CROSS_MS          40U     // ~2 half-cycles at 47 Hz
#define APP_XCAP_VAC_ON_V             30.0f   // Start only above this |vac|
#define APP_XCAP_VAC_OFF_V            30.0f   // Stop once |vac| is below this
#define APP_XCAP_ZC_HYST_V            10.0f   // Zero-crossing hysteresis (+-)
#define APP_XCAP_MAX_MS               1000U   // Longest discharge (bleeder rating)

// Firmware version reported to the LLC (GET_INFO)
#define APP_FW_VERSION_MAJOR          0
#define APP_FW_VERSION_MINOR          1
#define APP_FW_VERSION_BUILD          1

// X2Cscope debug interface: 1 = build it in (UART3 monitoring and scope
// sampling in the phase-B ISR), 0 = all X2Cscope calls compile to nothing.
// Turn off for a release build once debugging is finished.
#define APP_X2CSCOPE_ENABLE           1

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* APP_CONFIG_H */
