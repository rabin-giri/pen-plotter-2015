/*
 * header.h
 *
 * Original Creation Date: December 2015
 * Authors: Rabin Giri
 *
 * Description:
 * Hardware abstraction layer (HAL) and pin mapping definitions for the ATmega32
 * and ATmega8 microcontrollers. This file provides convenient macros for GPIO 
 * manipulation, SPI/UART configurations, and bitwise operations.
 */ 

#ifndef HEADER_H_
#define HEADER_H_

#include <avr/interrupt.h>

// ---------------------------------------------------------
// MCU-Specific Pin Definitions
// ---------------------------------------------------------

#if defined(atMega32)
    // SPI Interface
    #define DD_SS           B,4
    #define DD_MISO         B,6
    #define DD_MOSI         B,5
    #define DD_SCK          B,7

    // Slave Selects
    #define SLAVE1          B,0
    #define SLAVE2          B,2
    #define SLAVE3          B,3
    #define SLAVE4          B,1

    // UART Interface
    #define TX_PIN          D,1
    #define RX_PIN          D,0

    // Interrupts and I2C/PWM
    #define DD_INT0         D,2
    #define DD_INT1         D,3
    #define DD_SCL          C,0
    #define DD_SDA          C,1
    #define DD_OC1A         D,5
    #define DD_OC1B         D,4
#endif


#if defined(atMega8)
    // SPI Interface
    #define DD_SS           B,2
    #define DD_MISO         B,4
    #define DD_MOSI         B,3
    #define DD_SCK          B,5

    // Slave Selects
    #define SLAVE1          B,0
    #define SLAVE2          B,2
    #define SLAVE3          B,3
    #define SLAVE4          B,1

    // UART Interface
    #define TX_PIN          D,1
    #define RX_PIN          D,0

    // Interrupts and I2C/PWM
    #define DD_INT0         D,2
    #define DD_INT1         D,3
    #define DD_SCL          C,5
    #define DD_SDA          C,4
    #define DD_OC1A         B,1
    #define DD_OC1B         B,2
#endif

// ---------------------------------------------------------
// Low-Level GPIO Macros (Register Manipulation)
// ---------------------------------------------------------
#define INPUT2(port,pin)    DDR ## port &= ~_BV(pin)
#define OUTPUT2(port,pin)   DDR ## port |= _BV(pin)
#define CLEAR2(port,pin)    PORT ## port &= ~_BV(pin)
#define SET2(port,pin)      PORT ## port |= _BV(pin)
#define TOGGLE2(port,pin)   PORT ## port ^= _BV(pin)
#define READ2(port,pin)     ((PIN ## port & _BV(pin)) ? 1 : 0)

// ---------------------------------------------------------
// High-Level GPIO Abstractions
// ---------------------------------------------------------
#define INPUT(x)            INPUT2(x)
#define OUTPUT(x)           OUTPUT2(x)
#define CLEAR(x)            CLEAR2(x)
#define SET(x)              SET2(x)
#define TOGGLE(x)           TOGGLE2(x)
#define READ(x)             READ2(x)

#define PULLUP_ON(x)        INPUT2(x); SET2(x)
#define PULLUP_OFF(x)       INPUT2(x); CLEAR2(x)

#define Slave_Connect(x)    CLEAR2(x)
#define Slave_Disconnect(x) SET2(x)

// ---------------------------------------------------------
// Register Bit Manipulation Macros
// ---------------------------------------------------------
#define REGISTER_SET1(REGISTER, BIT1) REGISTER |= _BV(BIT1)
#define REGISTER_SET2(REGISTER, BIT1, BIT2) REGISTER |= _BV(BIT1) | _BV(BIT2)
#define REGISTER_SET3(REGISTER, BIT1, BIT2, BIT3) REGISTER |= _BV(BIT1) | _BV(BIT2) | _BV(BIT3)
#define REGISTER_SET4(REGISTER, BIT1, BIT2, BIT3, BIT4) REGISTER |= _BV(BIT1) | _BV(BIT2) | _BV(BIT3) | _BV(BIT4)
#define REGISTER_SET5(REGISTER, BIT1, BIT2, BIT3, BIT4, BIT5) REGISTER |= _BV(BIT1) | _BV(BIT2) | _BV(BIT3) | _BV(BIT4) | _BV(BIT5)
#define REGISTER_SET6(REGISTER, BIT1, BIT2, BIT3, BIT4, BIT5, BIT6) REGISTER |= _BV(BIT1) | _BV(BIT2) | _BV(BIT3) | _BV(BIT4) | _BV(BIT5) | _BV(BIT6)
#define REGISTER_RESET(REGISTER, BIT) REGISTER &= ~_BV(BIT)

// ---------------------------------------------------------
// Data Conversion Macros
// ---------------------------------------------------------
#define _16bitTo8bit(_16BitNum, _8BitHigh, _8BitLow) _8BitLow = _16BitNum; _8BitHigh = (_16BitNum >> 8);
#define _8bitTo16bit(_16BitNum, _8BitHigh, _8BitLow) _16BitNum = (int)_8BitLow; _16BitNum |= ((int)_8BitHigh << 8);

// ---------------------------------------------------------
// Global Interrupt Control
// ---------------------------------------------------------
#define General_interrupt_enable() sei()

#endif /* HEADER_H_ */
