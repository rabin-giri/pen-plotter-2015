/*
 * Servo.h
 *
 * Original Creation Date: August 2015
 * Author: Rabin Giri
 *
 * Description:
 * Software-based PWM driver for hobby servo actuation (Pen Up/Down).
 * This uses synchronous blocking delays to physically hold the CPU until
 * the servo has had enough time to reach its target angle, maintaining 
 * the open-loop synchronization of the system.
 */ 

#ifndef SERVO_H_
#define SERVO_H_

#include <avr/io.h>
#include "header.h"

// Servo Control Pin
#define SERVO_PIN D,5

// Millisecond pulse widths for specific servo angles
#define DEGREE_0    0.45
#define DEGREE_45   0.8225
#define DEGREE_90   1.5
#define DEGREE_135  1.6075
#define DEGREE_180  2.1

// Function Prototypes
void servo_initialize();
void activate_servo(unsigned char state);

// ---------------------------------------------------------
// Implementations
// ---------------------------------------------------------

/*
 * Initializes the servo pin as an output and sets the pen to the default UP state.
 */
inline void servo_initialize() 
{
    OUTPUT(SERVO_PIN);
    activate_servo(0); // Default to pen up
}
			
/*
 * Actuates the servo to either the 0-degree or 180-degree position.
 * Generates 40 software PWM pulses (~800ms total) to allow the physical 
 * motor time to reach the target position.
 */
inline void activate_servo(unsigned char state)
{
    if (state) 
    {
        // State 1: Pen Down (Drawing)
        for (int j = 0; j < 40; j++) 
        {
            SET(SERVO_PIN);
            _delay_ms(DEGREE_0);
            CLEAR(SERVO_PIN);
            _delay_ms(20.0 - DEGREE_0);
        }
    } 
    else 
    {
        // State 0: Pen Up (Hovering)
        for (int j = 0; j < 40; j++) 
        {
            SET(SERVO_PIN);
            _delay_ms(DEGREE_180);
            CLEAR(SERVO_PIN);
            _delay_ms(20.0 - DEGREE_180);
        }
    }
}
			
#endif /* SERVO_H_ */
