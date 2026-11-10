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
* File Name       : board_config.h
* Description     : Board hardware constants: timing, sensing, power stage
*
*   One place for everything that follows from the hardware (not from
*   tuning): ISR and task rates, ADC sensing scales, CT range, power-stage
*   L and C. Change these when the board, the MCC clock or the sensing
*   circuit changes. Software switches live in app_config.h; control and
*   protection tuning stays next to the code that uses it.
*   All values are plain literals so they can be used in #if and in
*   static initialisers.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/11/01
* Modified by     :
* Modified on     :
*******************************************************************************/
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Include

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
// Execution rates (must match MCC: PG3PER 65 kHz, TMR1 PR1 100 us)
#define BOARD_FAST_LOOP_HZ      65000U      // Phase A / B ADC ISR rate
#define BOARD_VLOOP_DIVIDER     8U          // Voltage loop every N phase-B ISRs (8.125 kHz)
#define BOARD_SLOW_TICK_US      100U        // T1 period
#define BOARD_SLOW_TICKS_PER_MS (1000U / BOARD_SLOW_TICK_US)

// ADC sensing (12-bit, 3.3 V, single sample)
// TODO: replace with values from the sensing design and calibration
#define BOARD_FB_VAC_SCALE      0.2000f     // V/count per line, (3.3/4096) / divider gain
#define BOARD_FB_VAC_L_OFFSET   2048        // counts
#define BOARD_FB_VAC_N_OFFSET   2048        // counts
#define BOARD_FB_VOUT_SCALE     0.1221f     // V/count, 500 V full scale
#define BOARD_FB_VOUT_OFFSET    0           // counts
#define BOARD_FB_IL_RANGE_A     50.0f       // A, +- full-scale current of the bipolar CT
#define BOARD_FB_IL_SCALE       (BOARD_FB_IL_RANGE_A / 2048.0f) // A/count around mid-scale, 0.02441
#define BOARD_FB_TEMP_SCALE     0.0806f     // degC/count, linearised NTC placeholder
#define BOARD_FB_TEMP_OFFSET    0           // counts

// Power stage (controller gains are derived from these)
#define BOARD_PFC_L_H           300e-6f     // Boost inductor per phase (at peak current)
#define BOARD_PFC_COUT_F        1000e-6f    // Bulk capacitance

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* BOARD_CONFIG_H */
