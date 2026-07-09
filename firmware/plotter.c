/*
 * plotter.c (Main AVR Firmware)
 *
 * Original Creation Date: September 2015
 * Author: Rabin Giri
 *
 * Description:
 * Core firmware for the ATmega microcontroller. Handles UART Bluetooth 
 * communication, G-code parsing, and implements a Bresenham-style linear 
 * interpolation algorithm for synchronized multi-axis stepper motion.
 */ 

#define atmega32
#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <avr/interrupt.h>
#include "servo.h"
#include "header.h"


// ---------------------------------------------------------
// Hardware Configuration
// ---------------------------------------------------------

// UART Pins & Baud Rate Setup
#define TX_PIN          D,1
#define RX_PIN          D,0
#define USART_BAUDRATE  38400
#define BAUD_PRESCALE   (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

// X-Axis Stepper Pins
#define X_STEP_PIN      D,7   // OC2
#define X_DIR_PIN       D,3
#define X_ENABLE_PIN    C,3
#define X_MIN_PIN       C,4

// Y-Axis Stepper Pins
#define Y_STEP_PIN      B,3   // OC0
#define Y_DIR_PIN       D,6
#define Y_ENABLE_PIN    D,4
#define Y_MIN_PIN       C,5

// Machine Kinematics
#define X_STEPS_PER_INCH  48
#define X_STEPS_PER_MM    (5000.0 / 33.0)  // ~151.51 steps/mm
#define Y_STEPS_PER_INCH  48
#define Y_STEPS_PER_MM    (5000.0 / 33.0)

#define FAST_XY_FEEDRATE  3000

#define CURVE_SECTION_INCHES 0.019685
#define CURVE_SECTION_MM     0.5

// ---------------------------------------------------------
// Global State Variables
// ---------------------------------------------------------

unsigned char serial_count = 0;
#define COMMAND_SIZE 64
char commands[COMMAND_SIZE];

struct LongPoint {
    long int x;
    long int y;
};

struct FloatPoint {
    float x;
    float y;
};

unsigned char abs_mode = 1;   // 0 = incremental; 1 = absolute

// Units default to mm
float x_units = X_STEPS_PER_MM;
float y_units = Y_STEPS_PER_MM;
float curve_section = CURVE_SECTION_MM;

// Direction flags
unsigned char x_direction = 1;
unsigned char y_direction = 1;

// Feedrate variables
float feedrate = 0.0;
long feedrate_micros = 0;

FloatPoint current_units;
FloatPoint target_units;
FloatPoint delta_units;

LongPoint current_steps;
LongPoint target_steps;
LongPoint delta_steps;

// ---------------------------------------------------------
// Function Prototypes
// ---------------------------------------------------------
void init_uart(void);
void initialize_everything();
void steppers_initialize();
void motors_enable();
void motors_disable();
void write_output(unsigned char pin, unsigned char state);
unsigned char read_pin(unsigned char pin);
void do_step(unsigned char step_pin, unsigned char step_dirpin, unsigned char dir);
void goto_machine_zero();
void move_to_origin(unsigned char step, unsigned char dir_pin, unsigned char switch_pin);
void draw_line(long int micro_delay);
unsigned char can_step(unsigned char min_pin, long current, long target, unsigned char direction);
long to_steps(float steps_per_unit, float units);
void set_target(float x, float y);
void set_position(float x, float y);
void calculate_deltas();
long calculate_feedrate_delay(float feedrate);

void init_command_processing();
void process_command(char instruction[], int s);
float get_code(char key, char gcode[], int string_size);
unsigned char search_command(char key, char gcode[], int string_size);

// ---------------------------------------------------------
// Main Entry Point
// ---------------------------------------------------------
int main(void)
{
    char gcodebuffer[50];
    char data;
    int i = 0;
    
    init_uart();
    initialize_everything();
    
    // Main execution loop: Polling UART for G-code data
    while(1)
    {
        if (UCSRA & (1 << RXC))
        {
            data = UDR;
            
            // Handshake synchronization
            while (!(UCSRA & (1 << UDRE)));
            UDR = data + 1;
            
            if (data == '*')
            {
                while (!(UCSRA & (1 << UDRE)));
                UDR = 0xa0;
            }	
            else
            {
                strcpy(gcodebuffer, "");
                i = 0;
                
                // Read incoming stream until delimiter '#'
                while(1)
                {
                    if (data == '#')
                    {
                        gcodebuffer[i] = '\0';
                        break;
                    }
                    
                    gcodebuffer[i] = data;
                    i++;
                    
                    while (!(UCSRA & (1 << RXC)));
                    data = UDR;
                    
                    while (!(UCSRA & (1 << UDRE)));
                    UDR = data + 1;
                }
                
                // Process the buffered command string
                process_command(gcodebuffer, 20);
            }
        }			
    }
}

// ---------------------------------------------------------
// Motion Control & Interpolation
// ---------------------------------------------------------

/*
 * Implementation of linear interpolation (Bresenham-style approach).
 * Steps the dominant axis and accumulates error to step the minor axis,
 * ensuring straight physical lines. Uses a blocking microsecond delay 
 * to dictate hardware speed synchronously.
 */
void draw_line(long int micro_delay)
{
    int max_delta = 0;
    
    motors_enable();
    
    // Find the dominant axis for Bresenham calculation
    if (delta_steps.y > delta_steps.x)
        max_delta = delta_steps.y;
    else
        max_delta = delta_steps.x;

    // Init counter
    long int x_counter = -max_delta / 2;
    long int y_counter = -max_delta / 2;
    
    unsigned char x_can_step = 0;
    unsigned char y_can_step = 0;
    
    do
    {
        x_can_step = can_step('c', current_steps.x, target_steps.x, x_direction);
        y_can_step = can_step('d', current_steps.y, target_steps.y, y_direction);
        
        if (x_can_step)
        {
            x_counter += delta_steps.x;
            if (x_counter > 0)
            {
                do_step('x', 'a', x_direction);
                x_counter -= max_delta;
                
                if (x_direction) current_steps.x--;
                else current_steps.x++;
            }
        }

        if (y_can_step)
        {
            y_counter += delta_steps.y;
            if (y_counter > 0)
            {
                do_step('y', 'b', y_direction);
                y_counter -= max_delta;

                if (y_direction) current_steps.y++;
                else current_steps.y--;
            }
        }
        
        // Blocking delay to match physical hardware limitations
        _delay_us(350);
        
    } while (x_can_step || y_can_step);

    // Synchronize software position with physical position
    current_units.x = target_units.x;
    current_units.y = target_units.y;
    calculate_deltas();
}

unsigned char can_step(unsigned char min_pin, long current, long target, unsigned char direction)
{
    if (target == current) return 0;
    return 1;
}

void do_step(unsigned char step_pin, unsigned char step_dirpin, unsigned char dir) 
{
    switch(dir << 2 | read_pin(step_pin) << 1 | read_pin(step_dirpin))
    {
        case 0:
        case 5: write_output(step_pin, 1); break;
        case 2:
        case 7: write_output(step_pin, 0); break;
        case 4: write_output(step_dirpin, 1); write_output(step_pin, 1); break;
        case 1: write_output(step_pin, 1); write_output(step_dirpin, 0); break;
        case 3: write_output(step_pin, 0); write_output(step_dirpin, 0); break;
        case 6: write_output(step_pin, 0); write_output(step_dirpin, 1); break;
    }
}

// ---------------------------------------------------------
// G-Code Parsing & Processing
// ---------------------------------------------------------

void process_command(char instruction[], int s)
{
    int size = strlen(instruction);
    int code = 0;
    
    if (instruction[0] == '/') return;
    
    FloatPoint fp;
    fp.x = 0.0;
    fp.y = 0.0;
    
    // Handle Coordinate G-Codes (G0, G1, etc.)
    if (search_command('G', instruction, size) || search_command('X', instruction, size) || 
        search_command('Y', instruction, size) || search_command('Z', instruction, size))
    {
        code = (int)get_code('G', instruction, size);
        
        switch (code)
        {
            case 0:
            case 1:
            case 2:
            case 3:
                if (abs_mode)
                {
                    if (search_command('X', instruction, size)) fp.x = get_code('X', instruction, size);
                    else fp.x = current_units.x;
                    
                    if (search_command('Y', instruction, size)) fp.y = get_code('Y', instruction, size);
                    else fp.y = current_units.y;
                }
                else
                {
                    fp.x = get_code('X', instruction, size) + current_units.x;
                    fp.y = get_code('Y', instruction, size) + current_units.y;
                }
                break;
        }
        
        switch (code)
        {
            case 0: // Rapid Positioning
            case 1: // Linear Interpolation
                set_target(fp.x, fp.y);
                draw_line(500);
                break;
                
            case 2: // Clockwise arc (Stub)
            case 3: // Counterclockwise arc (Stub)
                break;
                
            case 20: // Inches units
                x_units = X_STEPS_PER_INCH;
                y_units = Y_STEPS_PER_INCH;
                curve_section = CURVE_SECTION_INCHES;
                calculate_deltas();
                break;

            case 21: // MM units
                x_units = X_STEPS_PER_MM;
                y_units = Y_STEPS_PER_MM;
                curve_section = CURVE_SECTION_MM;
                calculate_deltas();
                break;

            case 28: // Go home
                set_target(0.0, 0.0);
                goto_machine_zero();
                break;

            case 90: // Absolute Positioning
                abs_mode = 1;
                break;

            case 91: // Incremental Positioning
                abs_mode = 0;
                break;

            case 92: // Set as home
                set_position(0.0, 0.0);
                break;
        }
    }
    
    // Handle Machine M-Codes (Pen Actuation)
    if (search_command('M', instruction, size))
    {
        code = get_code('S', instruction, size);
        switch(code)
        {
            case 30: 
                activate_servo(1); // Pen down to draw
                break;
            case 50: 
                activate_servo(0); // Pen up
                break;
        }
        _delay_ms(200);
    }
}

float get_code(char key, char gcode[], int string_size)
{
    char temp[10];
    unsigned char i;
    for (i = 0; i < string_size; i++)
    {
        if (gcode[i] == key)
        {
            i++;
            unsigned char k = 0;
            while (i < string_size && k < 10)
            {
                if (gcode[i] == 0 || gcode[i] == ' ') break;
                temp[k] = gcode[i];
                i++;
                k++;
            }
            return strtod(temp, NULL);
        }
    }
    return 0;
}

unsigned char search_command(char key, char gcode[], int string_size)
{
    unsigned char i;
    for (i = 0; i < string_size; i++)
    {
        if (gcode[i] == key) return 1;
    }
    return 0;
}

// ---------------------------------------------------------
// Hardware Setup & Utility Functions
// ---------------------------------------------------------

void initialize_everything()
{
    steppers_initialize();
    motors_disable();
    servo_initialize();
    init_command_processing();
    current_units.x = 0.0;
    current_units.y = 0.0;
    target_units.x = 0.0;
    target_units.y = 0.0;
    calculate_deltas();
}

void steppers_initialize()
{
    OUTPUT(X_STEP_PIN);
    OUTPUT(X_DIR_PIN);
    OUTPUT(X_ENABLE_PIN);
    PULLUP_ON(X_MIN_PIN);

    OUTPUT(Y_STEP_PIN);
    OUTPUT(Y_DIR_PIN);
    OUTPUT(Y_ENABLE_PIN);
    PULLUP_ON(Y_MIN_PIN);
}

void init_uart(void)
{
    INPUT(RX_PIN);
    OUTPUT(TX_PIN);
    REGISTER_SET2(UCSRB, RXEN, TXEN);
    REGISTER_SET3(UCSRC, UCSZ1, UCSZ0, URSEL);
    UBRRH = (BAUD_PRESCALE >> 8);
    UBRRL = BAUD_PRESCALE;
}

void write_output(unsigned char pin, unsigned char state)
{
    switch(pin)
    {
        case 'x': if (state == 1) SET(X_STEP_PIN); else CLEAR(X_STEP_PIN); break;
        case 'y': if (state == 1) SET(Y_STEP_PIN); else CLEAR(Y_STEP_PIN); break;
        case 'a': if (state == 1) SET(X_DIR_PIN); else CLEAR(X_DIR_PIN); break;
        case 'b': if (state == 1) SET(Y_DIR_PIN); else CLEAR(Y_DIR_PIN); break;
    }
}

unsigned char read_pin(unsigned char pin)
{
    switch(pin)
    {
        case 'x': return (bit_is_set(PIND, 7)) ? 1 : 0;
        case 'y': return (bit_is_set(PINB, 3)) ? 1 : 0;
        case 'a': return (bit_is_set(PIND, 3)) ? 1 : 0;
        case 'b': return (bit_is_set(PIND, 6)) ? 1 : 0;
        case 'c': return (bit_is_set(PINC, 4)) ? 1 : 0;
        case 'd': return (bit_is_set(PINC, 5)) ? 1 : 0;
    }
    return 0;
}

void motors_disable()
{
    CLEAR(X_ENABLE_PIN);
    CLEAR(Y_ENABLE_PIN);
}

void motors_enable()
{
    SET(X_ENABLE_PIN);
    SET(Y_ENABLE_PIN);
}

void goto_machine_zero()
{
    move_to_origin('x', 'a', 'c');
    move_to_origin('y', 'b', 'd');
}

void move_to_origin(unsigned char step, unsigned char dir_pin, unsigned char switch_pin)
{
    while (!read_pin(switch_pin)) {
        do_step(step, dir_pin, 0);
        _delay_us(100);
    }
}

long to_steps(float steps_per_unit, float units)
{
    return (long)(steps_per_unit * units);
}

void set_target(float x, float y)
{
    target_units.x = x;
    target_units.y = y;
    calculate_deltas();
}

void set_position(float x, float y)
{
    current_units.x = x;
    current_units.y = y;
    calculate_deltas();
}

void calculate_deltas()
{
    delta_units.x = abs(target_units.x - current_units.x);
    delta_units.y = abs(target_units.y - current_units.y);
    
    current_steps.x = to_steps(x_units, current_units.x);
    current_steps.y = to_steps(y_units, current_units.y);
    target_steps.x = to_steps(x_units, target_units.x);
    target_steps.y = to_steps(y_units, target_units.y);

    delta_steps.x = abs(target_steps.x - current_steps.x);
    delta_steps.y = abs(target_steps.y - current_steps.y);
    
    x_direction = !(target_units.x >= current_units.x);
    y_direction = (target_units.y >= current_units.y);
}

long calculate_feedrate_delay(float feedrate)
{
    float distance = sqrt(delta_units.x * delta_units.x + delta_units.y * delta_units.y);
    long master_steps = (delta_steps.x > delta_steps.y) ? delta_steps.x : delta_steps.y;
    return ((distance * 60000000.0) / feedrate) / master_steps;
}

void init_command_processing()
{
    for (unsigned char i = 0; i < COMMAND_SIZE; i++)
        commands[i] = 0;
    serial_count = 0;
}
