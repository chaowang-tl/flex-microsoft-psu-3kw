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
* File Name       : mcc_ext.c
* Description     : MCC extended driver implementation
* Original Author : Chao Wang <chao.wang@microchip.com>
* Created on      : 2026/04/10
* Modified by     : Chao Wang
* Modified on     : 2026/11/10
*******************************************************************************/
#define MCC_EXT_C

//------------------------------------------------------------------------------
// Include
#include "mcc_ext.h"
#include "../../../mcc_generated_files/cmp/cmp7.h"
#include "../../../mcc_generated_files/cmp/cmp8.h"
#include "../../../mcc_generated_files/pwm_hs/pwm.h"
#include "../pfc_pwm/pfc_pwm.h"
#include "../pfc_io/pfc_io.h"
#include "../../config/app_config.h"

//------------------------------------------------------------------------------
// Constants and Macro Definitions
#define ADC_CAL_TIMEOUT     100000UL    // Timeout for ADC calibration busy-wait
#define ADC_UNITY_GAIN      (1UL << 18) // Unity gain fallback coefficient
#define ADC_REF_VOLTAGE     61439.0     // Expected reference voltage (oversampled 16-bit)

#define DAC_VREF_V          3.3f        // DAC reference (AVDD)
#define DAC_FULL_SCALE      4095.0f     // 12-bit DAC
#define DAC_DAT_MIN         0x0CDU      // Lowest valid DACDAT (data sheet Eq. 19-1)
#define DAC_DAT_MAX         0xF32U      // Highest valid DACDAT

#define PWM_CLK_NS          1.25f       // PWM clock 800 MHz (Clock Generator 5)
#define LEB_TIME_NS         200.0f      // CBC current-limit leading-edge blanking
// PGxLEBbits.LEB = LEB[19:4], one count = one PWM clock; blanking = (LEB + 1) * TPWMCLK
#define PERIOD_LEB          ((uint16_t)(LEB_TIME_NS / PWM_CLK_NS + 0.5f) - 1U)   // 159

// DS70005591C 16.4.38: DTH/DTL use 1/16 PWM-clock fine counts.
#define PWM_DEADTIME_COUNT ((uint32_t)(APP_PFC_PWM_DEADTIME_NS * 16.0f / PWM_CLK_NS + 0.5f))
_Static_assert(PWM_DEADTIME_COUNT > 0U && PWM_DEADTIME_COUNT <= 0x7FFFU,
               "PWM dead time must fit DTH/DTL");

//------------------------------------------------------------------------------
// Public Variables

//------------------------------------------------------------------------------
// Private Enumerated and Structure Definitions

//------------------------------------------------------------------------------
// Private Function Prototypes
static void Adc1SharedCoreGainErrorCal(void);
static void Adc2SharedCoreGainErrorCal(void);
static void Adc3SharedCoreGainErrorCal(void);
static void Adc4SharedCoreGainErrorCal(void);
static void Adc5SharedCoreGainErrorCal(void);
static void AdcInitExt(void);
static void PinsInitExt(void);
static void PwmInitExt(void);
static void CmpInitExt(void);
static void DmaInitExt(void);
static void InterruptInitExt(void);
static uint16_t CmpDacCount(float volts);
//------------------------------------------------------------------------------
// Private Variables

////////////////////////////////////////////////////////////////////////////////
// Public Function Implementation

/*******************************************************************************
* Public Function : MCC_EXT_SystemInitialize
* Description     : Extended system initialization for peripherals not
*                   covered by MCC-generated code
* Purpose         : Configure ADC calibration, PPS, comparator/DAC, PWM,
*                   DMA, interrupts (SCCP1/2/3 are
*                   set up by LLC_SIGNAL_Init).
*                   Global interrupts are masked during setup to prevent
*                   partially-configured peripherals from triggering ISRs
* Input           : None
* Return          : None
* Calls           : __builtin_disable_interrupts, AdcInitExt,
*                   PinsInitExt, CmpInitExt,
*                   PwmInitExt, DmaInitExt,
*                   InterruptInitExt
* Called by       : main (after SYSTEM_Initialize)
* Note            : Global interrupts are NOT re-enabled here; the caller
*                   is responsible for enabling them at the appropriate time.
*                   The PWM generators are started at the end of PwmInitExt
*******************************************************************************/
void MCC_EXT_SystemInitialize(void)
{
    __builtin_disable_interrupts();
    AdcInitExt();
    PinsInitExt();
    CmpInitExt();
    PwmInitExt();
    DmaInitExt();
    InterruptInitExt();
}

////////////////////////////////////////////////////////////////////////////////
// Private Function Implementation

/*******************************************************************************
* ADCx Shared-Core Gain Error Calibration (macro-generated)
*
* Each AdcNSharedCoreGainErrorCal() function:
*   1. Configures CHx for 256-sample oversampling on AN6 (single-ended)
*   2. Triggers SW conversion and busy-waits for result
*   3. Computes fixed-point (18-bit fractional) gain coefficient
*
* Parameters:
*   n       - ADC module number (1..5)
*   ch      - Channel number (7 for ADC1-4, 15 for ADC5)
*   core    - Index into g_adcxGainCoeff[] (ADC_SHARED_CORE_x)
*******************************************************************************/
#define ADC_GAIN_CAL_IMPL(n, ch, core)                                        \
static void Adc##n##SharedCoreGainErrorCal(void)                                     \
{                                                                             \
    /* Oversampling conversion mode */                                        \
    AD##n##CH##ch##CON1bits.MODE    = 3;                                      \
    /* 256 samples, 16 bits result */                                         \
    AD##n##CH##ch##CON1bits.ACCNUM  = 3;                                      \
    /* Cannot be interrupted by higher priority channels */                   \
    AD##n##CH##ch##CON2bits.ACCBRST = 1;                                      \
    /* Software trigger */                                                    \
    AD##n##CH##ch##CON1bits.TRG1SRC = 0b000001;                               \
    /* Re-trigger back to back */                                             \
    AD##n##CH##ch##CON1bits.TRG2SRC = 2;                                      \
    /* Single-ended input */                                                  \
    AD##n##CH##ch##CON1bits.DIFF    = 0;                                      \
    /* AN6 positive input */                                                  \
    AD##n##CH##ch##CON1bits.PINSEL  = 6;                                      \
    /* ANN0 negative input */                                                 \
    AD##n##CH##ch##CON1bits.NINSEL  = 0;                                      \
    /* Sampling time: 6.5 TAD (81.25 ns) */                                   \
    AD##n##CH##ch##CON1bits.SAMC    = 0b011;                                  \
    /* Interrupt when data is updated */                                      \
    AD##n##CH##ch##CON1bits.IRQSEL  = 0;                                      \
                                                                              \
    /* Trigger oversampling conversion */                                     \
    AD##n##SWTRGbits.CH##ch##TRG = 1;                                         \
    /* Wait for result with timeout */                                        \
    uint32_t timeout = ADC_CAL_TIMEOUT;                                       \
    while (AD##n##STATbits.CH##ch##RDY == 0 && --timeout > 0) { }             \
    /* Fall back to unity gain on timeout or an invalid (zero) result */      \
    if (timeout == 0 || AD##n##CH##ch##DATA == 0) {                           \
        g_adcCalibrationFailedMask |= (1UL << (core));                     \
        g_adcxGainCoeff[core] = ADC_UNITY_GAIN;                               \
        return;                                                               \
    }                                                                         \
    /* Gain coefficient in fixed-point format (18-bit fractional) */           \
    g_adcxGainCoeff[core] =                                                   \
        (uint32_t)(ADC_REF_VOLTAGE * (1UL << 18) / AD##n##CH##ch##DATA);      \
}

ADC_GAIN_CAL_IMPL(1, 7,  ADC_SHARED_CORE_1)
ADC_GAIN_CAL_IMPL(2, 7,  ADC_SHARED_CORE_2)
ADC_GAIN_CAL_IMPL(3, 7,  ADC_SHARED_CORE_3)
ADC_GAIN_CAL_IMPL(4, 7,  ADC_SHARED_CORE_4)
ADC_GAIN_CAL_IMPL(5, 15, ADC_SHARED_CORE_5)

#undef ADC_GAIN_CAL_IMPL

/*******************************************************************************
* Private Function : AdcInitExt
* Description     : Run shared-core gain error calibration for all active ADCs
* Purpose         : Compute per-core gain coefficients so ISR-time conversions
*                   can be corrected for ADC gain error
* Input           : None
* Return          : None
* Calls           : Adc1SharedCoreGainErrorCal, Adc2SharedCoreGainErrorCal,
*                   Adc3SharedCoreGainErrorCal, Adc4SharedCoreGainErrorCal,
*                   Adc5SharedCoreGainErrorCal
* Called by       : MCC_EXT_SystemInitialize
* Note            : Must be called after ADCx modules are initialised by MCC
*                   and before any ISR reads the gain coefficients
*******************************************************************************/
static void AdcInitExt(void)
{
    g_adcCalibrationFailedMask = 0UL;
    Adc1SharedCoreGainErrorCal();
    Adc2SharedCoreGainErrorCal();
    Adc3SharedCoreGainErrorCal();
    Adc4SharedCoreGainErrorCal();
    Adc5SharedCoreGainErrorCal();
}

/*******************************************************************************
* Private Function : PinsInitExt
* Description     : Peripheral pin select (PPS) remapping and unused pins
* Purpose         : Route the OCP sources to the PWM PCIs; discrete outputs safe
* Input           : None
* Return          : None
* Calls           : PFC_IO_Init
* Called by       : MCC_EXT_SystemInitialize
* Note            : PPS unlock/lock sequence is required by hardware.
*                   PFC_IO_Init (pfc_io.c) forces the relay (RB2), the X-cap
*                   bleeder (RA4) and the unused pins low and to outputs, so
*                   they are safe right after the MCC pin setup
*******************************************************************************/
static void PinsInitExt(void)
{
    RPCONbits.IOLOCK = 0;

    /* Phase A OCP */
    _PCI8R = 132;       // CMP4 -> PCI22
    _PCI9R = 134;       // CMP6 -> PCI22
    _PCI10R = 53;      // RD4 (OCP A) -> PCI22
    /* Phase B OCP */
    _PCI11R = 133;      // CMP5 -> PCI22
    _PCI12R = 129;      // CMP1 -> PCI22
    _PCI13R = 56;      // RD7 (OCP B) -> PCI22

    RPCONbits.IOLOCK = 1;

    PFC_IO_Init();                  // Relay, X-cap bleeder and unused pins to safe state
}

/*******************************************************************************
* Private Function : PwmInitExt
* Description     : Configure PWM ADC trigger points and update behaviour
* Purpose         : Set the current sampling triggers on PG3/PG4, make output
*                   overrides take effect as soon as possible, and let a
*                   duty write request its own update, set up the CBC current
*                   limit, then start the PWM generators
* Input           : None
* Return          : None
* Calls           : PFC_PWM_AllOutputsOff, PWM_Enable
* Called by       : MCC_EXT_SystemInitialize
* Note            : CAHALF=1 with compare=0 selects the second-half boundary.
*                   Average-current validity needs waveform verification.
*                   PG3 = phase A (master), PG4 = phase B, PG5 = slow leg, PG6 = PWM6H only (RC3).
*                   OSYNC and UPDTRG together remove the need for software
*                   UPDREQ writes in the PFC PWM driver. Must run after the
*                   MCC PWM_Initialize (it sets OSYNC per UPDMOD and PG5 UPDTRG manual).
*                   PG3/PG4 cycle-by-cycle current limit: fault PCI 1, latched,
*                   auto-terminated at EOC, outputs low (FLT1DAT = 00), qualified
*                   by LEB_TIME_NS blanking started on the PWMxH rising edge.
*                   PG3 sources CMP4/CMP6/RD4 (PCI8..10), PG4 CMP5/CMP1/RD7
*                   (PCI11..13), routed in PinsInitExt.
*                   PWM_Enable is last: LEB/PCI must not change while ON = 1.
*                   PFC outputs stay low (MCC overrides, PGxIOCON2 = 0x300800)
*                   and the ADC triggers reach the ISRs only once main enables
*                   interrupts
*******************************************************************************/
static void PwmInitExt(void)
{
    /**************************************************************************/
    // PWM Event A = PG3 TRIGB compare (second half, duty point) -> retrigger PG1
    PWMEVTA = 0x0UL;
    /* PWM Event Source Selection, PG3 */
    PWMEVTAbits.EVTAPGS = 0b010;
    /* ADC Trigger 2 signal */
    PWMEVTAbits.EVTASEL = 0b01010;
    /* PWM Event Output Enable */
    PWMEVTAbits.EVTAOEN = 0;
    
    // PWM Event B = PG4 TRIGB compare (second half, duty point) -> retrigger PG2
    PWMEVTB = 0x0UL;
    /* PWM Event Source Selection, PG4 */
    PWMEVTBbits.EVTBPGS = 0b011;
    /* ADC Trigger 2 signal */
    PWMEVTBbits.EVTBSEL = 0b01010;
    /* PWM Event Output Enable */
    PWMEVTBbits.EVTBOEN = 0;
    
    /**************************************************************************/
    /* Combinational sync-window gate. Inputs are taken after dead time, SWAP and
     * override, and the result replaces the destination pin. PG1/PG2 are independent
     * edge, independent output, so H and L carry the same window. Each pin's window
     * side is forced to 1 (pass) by PG1/PG2 override when that pin carries the boost
     * switch, and to 0 (block) or follows the window on the synchronous-rectifier pin.
     */
    // LOGCONA: PWM1H & PWM3H -> PWM3H
    LOGCONAbits.PWMS1A  = 0b0000;  // Source 1: PWM1H
    LOGCONAbits.PWMS2A  = 0b0100;  // Source 2: PWM3H
    LOGCONAbits.PWMLFA  = 0b01;    // Function: AND
    LOGCONAbits.PWMLFAD = 0b010;   // Output:   PWM3H

    // LOGCONB: PWM1L & PWM3L -> PWM3L
    LOGCONBbits.PWMS1B  = 0b0001;  // Source 1: PWM1L
    LOGCONBbits.PWMS2B  = 0b0101;  // Source 2: PWM3L
    LOGCONBbits.PWMLFB  = 0b01;    // Function: AND
    LOGCONBbits.PWMLFBD = 0b010;   // Output:   PWM3L

    // LOGCONC: PWM2H & PWM4H -> PWM4H
    LOGCONCbits.PWMS1C  = 0b0010;  // Source 1: PWM2H
    LOGCONCbits.PWMS2C  = 0b0110;  // Source 2: PWM4H
    LOGCONCbits.PWMLFC  = 0b01;    // Function: AND
    LOGCONCbits.PWMLFCD = 0b011;   // Output:   PWM4H

    // LOGCOND: PWM2L & PWM4L -> PWM4L
    LOGCONDbits.PWMS1D  = 0b0011;  // Source 1: PWM2L
    LOGCONDbits.PWMS2D  = 0b0111;  // Source 2: PWM4L
    LOGCONDbits.PWMLFD  = 0b01;    // Function: AND
    LOGCONDbits.PWMLFDD = 0b011;   // Output:   PWM4L
    
    /**************************************************************************/
    /*
     * 0b00: PWM Generator x Trigger Mode Selection - PWM Generator operates in Single Trigger mode
     * 0b01: PWM Generator x Trigger Mode Selection - PWM Generator operates in Retriggerable mode
     */
    PG1CONbits.TRGMOD = 0b01;

    PG1SPCI1 = 0x0UL;
    /* No acceptance qualifier is used (qualifier forced to ?1?). */
    PG1SPCI1bits.AQSS = 0b000;
    /* Termination Event Selection bits - Auto-Terminate */
    PG1SPCI1bits.TERM = 0b001;
    /* Termination of latched PCI occurs immediately */
    PG1SPCI1bits.TSYNCDIS = 1;
    /* No termination qualifier is used (qualifier forced to ?1?). */
    PG1SPCI1bits.TQSS = 0b000;
    /* SR latch is Reset-dominant in Latched Acceptance modes */
    PG1SPCI1bits.LATMOD = 1;
    /* PCI Acceptance Criteria Selection bits - Latched */
    PG1SPCI1bits.ACP = 0b011;
    
    /* 
     * 0: Tied to '0'
     * 1<<23: PCI Source Selection (PSS) - PWM event A
     * 1<<24: PCI Source Selection (PSS) - PWM event B
     * 1<<25: PCI Source Selection (PSS) - PWM event C
     * 1<<26: PCI Source Selection (PSS) - APWM event A
     */
    PG1SPCI2 = 1UL<<23;

    /**************************************************************************/
    /*
     * 0b00: PWM Generator x Trigger Mode Selection - PWM Generator operates in Single Trigger mode
     * 0b01: PWM Generator x Trigger Mode Selection - PWM Generator operates in Retriggerable mode
     */
    PG2CONbits.TRGMOD = 0b01;

    PG2SPCI1 = 0x0UL;
    /* No acceptance qualifier is used (qualifier forced to ?1?). */
    PG2SPCI1bits.AQSS = 0b000;
    /* Termination Event Selection bits - Auto-Terminate */
    PG2SPCI1bits.TERM = 0b001;
    /* Termination of latched PCI occurs immediately */
    PG2SPCI1bits.TSYNCDIS = 1;
    /* No termination qualifier is used (qualifier forced to ?1?). */
    PG2SPCI1bits.TQSS = 0b000;
    /* SR latch is Reset-dominant in Latched Acceptance modes */
    PG2SPCI1bits.LATMOD = 1;
    /* PCI Acceptance Criteria Selection bits - Latched */
    PG2SPCI1bits.ACP = 0b011;
    
    /* 
     * 0: Tied to '0'
     * 1<<23: PCI Source Selection (PSS) - PWM event A
     * 1<<24: PCI Source Selection (PSS) - PWM event B
     * 1<<25: PCI Source Selection (PSS) - PWM event C
     * 1<<26: PCI Source Selection (PSS) - APWM event A
     */
    PG2SPCI2 = 1UL<<24;

    /* PG1/PG2 are retriggered by the PG3/PG4 event, so their period is the maximum
     * value. Window and sync-timing references use the PG3/PG4 period.
     * Outputs are internal only (no pin): PENH/PENL cleared so they never drive RD0..RD3.
     */
    PG1PER = 0xFFFFFUL;
    PG2PER = 0xFFFFFUL;
    PG1PHASE = 0UL;
    PG2PHASE = 0UL;
    PG1DC = 0UL;
    PG2DC = 0UL;
    PG1IOCON1bits.PENH = 0;
    PG1IOCON1bits.PENL = 0;
    PG2IOCON1bits.PENH = 0;
    PG2IOCON1bits.PENL = 0;
    /* Duty writes take effect at the next SOC (same as PG3/PG4) */
    PG1EVT1bits.UPDTRG = 0b01;
    PG2EVT1bits.UPDTRG = 0b01;
    /* Override applied as soon as possible; open window (both sides forced 1 = pass) */
    PG1IOCON2bits.OSYNC = 0b01;
    PG2IOCON2bits.OSYNC = 0b01;
    PG1IOCON2 = (PG1IOCON2 & ~_PG1IOCON2_OVRDAT_MASK) | _PG1IOCON2_OVRDAT_MASK
              | _PG1IOCON2_OVRENH_MASK | _PG1IOCON2_OVRENL_MASK;
    PG2IOCON2 = (PG2IOCON2 & ~_PG2IOCON2_OVRDAT_MASK) | _PG2IOCON2_OVRDAT_MASK
              | _PG2IOCON2_OVRENH_MASK | _PG2IOCON2_OVRENL_MASK;

    /**************************************************************************/
    // Apply both-edge dead time after MCC, before PWM_Enable.
    PG3DT = (PWM_DEADTIME_COUNT << 16) | PWM_DEADTIME_COUNT;
    PG4DT = (PWM_DEADTIME_COUNT << 16) | PWM_DEADTIME_COUNT;
    PG5DT = (PWM_DEADTIME_COUNT << 16) | PWM_DEADTIME_COUNT;

    // TRIGA/D compare at count 0 in the second center-aligned half-cycle.
    PG3TRIGA = 1UL<<31;   // PHASE A current sampling point
    PG3TRIGD = 1UL<<31;   // PG3 PGTRGSEL = Trigger D -> SOC of PG4/PG5, PHASE A&B 180 degree interleaved
    PG4TRIGA = 1UL<<31;   // PHASE B current sampling point
    /* TRIGB = PG3/PG4 duty point (bit31 = CAHALF) -> PWM event A/B -> retrigger PG1/PG2.
     * Runtime value written by PFC_PWM_DutyASet/BSet, always before DC.
     */
    PG3TRIGB = 1UL<<31;
    PG4TRIGB = 1UL<<31;

    /* DS70005591C 16.4.14: 01 applies override as soon as possible.
     * Duty buffering still follows UPDMOD; shutdown does not wait for SOC.
     * Polarity reversal is separated by the zero-crossing BLANK interval.
     */
    PG3IOCON2bits.OSYNC = 0b01;
    PG4IOCON2bits.OSYNC = 0b01;
    PG5IOCON2bits.OSYNC = 0b01;

    /* A DC write requests buffer update automatically. Double-update
     * center-aligned PG3/PG4 offer two update points per full PWM cycle.
     */
    PG3EVT1bits.UPDTRG = 0b01;
    PG4EVT1bits.UPDTRG = 0b01;
    PG5EVT1bits.UPDTRG = 0b01;
    
    /* Initialize PGxLEB Registers */
    PG3LEBbits.LEB = PERIOD_LEB;
    /*
     * 0b0: LEB ignores the rising edge of PWMx.
     * 0b1: Rising edge of PWMx will trigger the LEB duration counter.
     */
    PG3LEBbits.PHR = 1;

    //PCI logic configuration for current limit cycle by cycle mode
    PG3IOCON2bits.FLT1DAT = 0b00;
    PG3F1PCI1bits.TSYNCDIS = 0;     // Termination of latched PCI delays till PWM EOC (for Cycle by cycle mode)
    PG3F1PCI1bits.TERM = 0b001;     // Auto-Terminate: Terminate when PCI source transitions from active to inactive
    PG3F1PCI1bits.TERMPS = 0;       // PCI Termination Polarity Select bit : non inverted

    PG3F1PCI1bits.AQPS = 1;         // Acceptance Qualifier signal is inverted
    PG3F1PCI1bits.AQSS = 0b010;     // Acceptance Qualifier Source: LEB is active


    PG3F1PCI1bits.PPS = 0;          // PCI source signal is not inverted
    PG3F1PCI1bits.PSYNC = 0;        // PCI source is not synchronized to PWM EOC so that current limit resets PWM immediately
    PG3F1PCI1bits.SWTERM = 0;       // PCI Software Termination

    PG3F1PCI1bits.ACP = 0b011;      // PCI Acceptance Criteria Selection bits -> Latched
    PG3F1PCI1bits.BPEN = 0;         // PCI Bypass Enable Bit. PCI function is not bypassed
    PG3F1PCI1bits.BPSEL = 0;        // PCI Bypass Source Selection
    PG3F1PCI1bits.LATMOD = 0;       // PCI SR Latch Mode: SR latch is set-dominant in Latched Acceptance modes

    PG3F1PCI1bits.SWPCI = 0;        // Software PCI Control
    PG3F1PCI1bits.SWPCIM = 0;       // Software PCI Control Mode
    PG3F1PCI1bits.TQPS = 0;         // Termination Qualifier Polarity Selection : Non inverted
    PG3F1PCI1bits.TQSS = 0b0000;    // No termination qualifier used so terminator will work straight away without any qualifier
    /*
     * 0: Tied to '0'
     * 1<<2: Internally connected to Combo Trigger A
     * 1<<3: Internally connected to Combo Trigger B
     * 1<<8: PCI8R
     * ...
     * 1<<22: PCI22R
     * 1<<23: PCI Source Selection (PSS) - PWM event A
     * 1<<24: PCI Source Selection (PSS) - PWM event B
     * 1<<25: PCI Source Selection (PSS) - PWM event C
     * 1<<26: PCI Source Selection (PSS) - APWM event A
     * 1<<27: PCI Source Selection (PSS) - APWM event B
     * 1<<28: PCI Source Selection (PSS) - Comparator 1 output
     * 1<<29: PCI Source Selection (PSS) - Comparator 2 output
     * 1<<30: PCI Source Selection (PSS) - Comparator 3 output
     * 1<<31: PCI Source Selection (PSS) - CLC 1
     */
    PG3F1PCI2 = (1UL<<8) | (1UL<<9) | (1UL<<10);

    /* Initialize PGxLEB Registers */
    PG4LEBbits.LEB = PERIOD_LEB;
    /*
     * 0b0: LEB ignores the rising edge of PWMx.
     * 0b1: Rising edge of PWMx will trigger the LEB duration counter.
     */
    PG4LEBbits.PHR = 1;

    //PCI logic configuration for current limit cycle by cycle mode
    PG4IOCON2bits.FLT1DAT = 0b00;
    PG4F1PCI1bits.TSYNCDIS = 0;     // Termination of latched PCI delays till PWM EOC (for Cycle by cycle mode)
    PG4F1PCI1bits.TERM = 0b001;     // Auto-Terminate: Terminate when PCI source transitions from active to inactive
    PG4F1PCI1bits.TERMPS = 0;       // PCI Termination Polarity Select bit : non inverted

    PG4F1PCI1bits.AQPS = 1;         // Acceptance Qualifier signal is inverted
    PG4F1PCI1bits.AQSS = 0b010;     // Acceptance Qualifier Source: LEB is active


    PG4F1PCI1bits.PPS = 0;          // PCI source signal is not inverted
    PG4F1PCI1bits.PSYNC = 0;        // PCI source is not synchronized to PWM EOC so that current limit resets PWM immediately
    PG4F1PCI1bits.SWTERM = 0;       // PCI Software Termination

    PG4F1PCI1bits.ACP = 0b011;      // PCI Acceptance Criteria Selection bits -> Latched
    PG4F1PCI1bits.BPEN = 0;         // PCI Bypass Enable Bit. PCI function is not bypassed
    PG4F1PCI1bits.BPSEL = 0;        // PCI Bypass Source Selection
    PG4F1PCI1bits.LATMOD = 0;       // PCI SR Latch Mode: SR latch is set-dominant in Latched Acceptance modes

    PG4F1PCI1bits.SWPCI = 0;        // Software PCI Control
    PG4F1PCI1bits.SWPCIM = 0;       // Software PCI Control Mode
    PG4F1PCI1bits.TQPS = 0;         // Termination Qualifier Polarity Selection : Non inverted
    PG4F1PCI1bits.TQSS = 0b0000;    // No termination qualifier used so terminator will work straight away without any qualifier
    /*
     * 0: Tied to '0'
     * 1<<2: Internally connected to Combo Trigger A
     * 1<<3: Internally connected to Combo Trigger B
     * 1<<8: PCI8R
     * ...
     * 1<<22: PCI22R
     * 1<<23: PCI Source Selection (PSS) - PWM event A
     * 1<<24: PCI Source Selection (PSS) - PWM event B
     * 1<<25: PCI Source Selection (PSS) - PWM event C
     * 1<<26: PCI Source Selection (PSS) - APWM event A
     * 1<<27: PCI Source Selection (PSS) - APWM event B
     * 1<<28: PCI Source Selection (PSS) - Comparator 1 output
     * 1<<29: PCI Source Selection (PSS) - Comparator 2 output
     * 1<<30: PCI Source Selection (PSS) - Comparator 3 output
     * 1<<31: PCI Source Selection (PSS) - CLC 1
     */
    PG4F1PCI2 = (1UL<<11) | (1UL<<12) | (1UL<<13);

    // PG6 only drives PWM6H (RC3); PWM6L has no pin mapping, so disable it.
    // Must be done before PWM_Enable (IOCON1 must not change while ON = 1).
    PG6IOCON1bits.PENL = 0;

    // Do not rely on the MCC override reset value: force every PFC output low
    PFC_PWM_AllOutputsOff();
    PWM_Enable();
}

/*******************************************************************************
* Private Function : CmpInitExt
* Description     : Comparator / DAC extended configuration
* Purpose         : Set the two analog output voltages from app_config.h
* Input           : None
* Return          : None
* Calls           : CMP8_DACDataWrite, CMP7_DACDataWrite, CmpDacCount
* Called by       : MCC_EXT_SystemInitialize
* Note            : RA8 = DACOUT2 <- DAC8 (CMP8, DACOEN = 1): external VREF,
*                   APP_DAC_VREF_RA8_V.
*                   RA9 = UREF <- DAC7 (CMP7, UREFCON.INSEL = DAC7,
*                   OUTSEL = UREF): debug output, APP_DAC_DEBUG_RA9_V.
*                   Routing and enables stay as configured by MCC; MCC sets
*                   UPDTRG to update on every DACDAT write, so the new value
*                   takes effect at once.
*                   TODO: verify on hardware that TRIS = 0 with ANSEL = 1 on
*                   RA8/RA9 (MCC) does not load the DAC outputs
*******************************************************************************/
static void CmpInitExt(void)
{
    CMP8_DACDataWrite(CmpDacCount(APP_DAC_VREF_RA8_V));
    CMP7_DACDataWrite(CmpDacCount(APP_DAC_DEBUG_RA9_V));
}

/*******************************************************************************
* Private Function : CmpDacCount
* Description     : Convert a voltage to a DACDAT count
* Purpose         : Keep the app_config.h values inside the valid DAC range
* Input           : volts - requested output [V]
* Return          : DACDAT, clamped to DAC_DAT_MIN..DAC_DAT_MAX
* Calls           : None
* Called by       : CmpInitExt
* Note            : VDAC = DACDAT * 3.3 / 4095 (data sheet Eq. 19-1)
*******************************************************************************/
static uint16_t CmpDacCount(float volts)
{
    float count = volts * (DAC_FULL_SCALE / DAC_VREF_V) + 0.5f;

    if (count <= (float)DAC_DAT_MIN)
    {
        return DAC_DAT_MIN;
    }
    if (count >= (float)DAC_DAT_MAX)
    {
        return DAC_DAT_MAX;
    }
    return (uint16_t)count;
}

/*******************************************************************************
* Private Function : DmaInitExt
* Description     : DMA extended configuration
* Purpose         : Reserved for DMA channel setup; nothing is configured yet
* Input           : None
* Return          : None
* Calls           : None
* Called by       : MCC_EXT_SystemInitialize
* Note            : None
*******************************************************************************/
static void DmaInitExt(void)
{

}

/*******************************************************************************
* Private Function : InterruptInitExt
* Description     : Interrupt extended configuration
* Purpose         : Reserved for interrupt priority setup; nothing is
*                   configured yet
* Input           : None
* Return          : None
* Calls           : None
* Called by       : MCC_EXT_SystemInitialize
* Note            : None
*******************************************************************************/
static void InterruptInitExt(void)
{

}


/******************************************************************************/
#undef MCC_EXT_C
