/**
 * PINS Generated Driver Source File 
 * 
 * @file      pins.c
 *            
 * @ingroup   pinsdriver
 *            
 * @brief     This is the generated driver source file for PINS driver.
 *
 * @skipline @version   PLIB Version 1.0.5
 *
 * @skipline  Device : dsPIC33AK256MPS206
*/

/*
? [2026] Microchip Technology Inc. and its subsidiaries.

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

// Section: Includes
#include <xc.h>
#include <stddef.h>
#include "../pins.h"

// Section: File specific functions

// Section: Driver Interface Function Definitions
void PINS_Initialize(void)
{
    /****************************************************************************
     * Setting the Output Latch SFR(s)
     ***************************************************************************/
    LATA = 0x0000UL;
    LATB = 0x0000UL;
    LATC = 0x0280UL;
    LATD = 0x0000UL;

    /****************************************************************************
     * Setting the GPIO Direction SFR(s)
     ***************************************************************************/
    TRISA = 0x0C66UL;
    TRISB = 0x0338UL;
    TRISC = 0x0167UL;
    TRISD = 0x01F0UL;


    /****************************************************************************
     * Setting the Weak Pull Up and Weak Pull Down SFR(s)
     ***************************************************************************/
    CNPUA = 0x0000UL;
    CNPUB = 0x0000UL;
    CNPUC = 0x0000UL;
    CNPUD = 0x0000UL;
    CNPDA = 0x0000UL;
    CNPDB = 0x0000UL;
    CNPDC = 0x0000UL;
    CNPDD = 0x0000UL;


    /****************************************************************************
     * Setting the Open Drain SFR(s)
     ***************************************************************************/
    ODCA = 0x0000UL;
    ODCB = 0x0000UL;
    ODCC = 0x0000UL;
    ODCD = 0x0000UL;


    /****************************************************************************
     * Setting the Analog/Digital Configuration SFR(s)
     ***************************************************************************/
    ANSELA = 0x0F66UL;
    ANSELB = 0x0338UL;

    /****************************************************************************
     * Set the PPS
     ***************************************************************************/
      PINS_PPSUnlock(); // unlock PPS

        RPINR14bits.U3RXR = 0x0027UL; //RC6->UART3:U3RX;
        RPINR13bits.U1RXR = 0x0029UL; //RC8->UART1:U1RX;
        RPINR2bits.ICM2R = 0x0037UL; //RD6->SCCP2:ICM2;
        RPINR2bits.ICM1R = 0x0037UL; //RD6->SCCP1:ICM1;
        RPOR9bits.RP40R = 0x0017UL;  //RC7->UART3:U3TX;
        RPOR10bits.RP42R = 0x0013UL;  //RC9->UART1:U1TX;
        RPOR9bits.RP37R = 0x0029UL;  //RC4->SCCP3:OCM3;
        RPOR12bits.RP51R = 0x0005UL;  //RD2->PWM:PWM3H;
        RPOR12bits.RP52R = 0x0006UL;  //RD3->PWM:PWM3L;
        RPOR12bits.RP49R = 0x0007UL;  //RD0->PWM:PWM4H;
        RPOR12bits.RP50R = 0x0008UL;  //RD1->PWM:PWM4L;
        RPOR10bits.RP43R = 0x0009UL;  //RC10->PWM:PWM5H;
        RPOR10bits.RP44R = 0x000AUL;  //RC11->PWM:PWM5L;
        RPOR8bits.RP36R = 0x000BUL;  //RC3->PWM:PWM6H;

      PINS_PPSLock(); // lock PPS


}

