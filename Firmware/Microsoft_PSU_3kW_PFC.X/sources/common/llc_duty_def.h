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
* File Name       : llc_duty_def.h
* Description     : Duty-coded PFC <-> LLC discrete signal definitions (shared)
*
*   docs/PFC_LLC_Comm_Protocol_EN.md section 2. Included by llc_protocol.h and
*   by the llc_signal driver, so the driver does not depend on the comm app.
*   Like llc_protocol.h it must stay identical in the PFC and LLC projects.
*
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/11/01
* Modified by     :
* Modified on     :
*******************************************************************************/
#ifndef LLC_DUTY_DEF_H
#define LLC_DUTY_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Include

//------------------------------------------------------------------------------
// Public Constants and Macro Definitions
// RC4 PFC -> LLC (bus voltage), RD6 LLC -> PFC (load), both 10 kHz
#define LLC_DUTY_FREQ_HZ            10000.0f
#define LLC_DUTY_FREQ_TOL           0.01f       // +-1 %: checked by the PFC receiver
#define LLC_DUTY_MIN                0.10f       // Duty at zero scale
#define LLC_DUTY_MAX                0.90f       // Duty at full scale
#define LLC_DUTY_VALID_LO           0.05f       // Below: signal invalid
#define LLC_DUTY_VALID_HI           0.95f       // Above: signal invalid
#define LLC_DUTY_TIMEOUT_MS         2U          // No edge for this long: invalid
#define LLC_DUTY_VBUS_FS_V          500.0f      // RC4 full scale [V]
#define LLC_DUTY_LOAD_FS_W          3600.0f     // RD6 full scale [W]

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  /* LLC_DUTY_DEF_H */
