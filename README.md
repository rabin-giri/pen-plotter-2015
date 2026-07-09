# Open-Loop CNC Pen Plotter (2015 College Project)

An end to end electromechanical CNC Pen Plotter system developed as a college project in 2015. The project bridges a custom Windows desktop application with bare metal AVR microcontroller firmware to parse G-code and control physical hardware via Bluetooth.

### 🛠️ Architecture Overview
* **Windows Desktop Application (C++ / Win32 API):** A G-code parsing and transmission utility that maps COM ports, establishes a wireless Bluetooth data link, reads `.gcode` files sequentially, and handles hardware synchronization.
* **Firmware (Bare-Metal C / ATmega32 or ATmega8):** Embedded control system utilizing a custom Bresenham style linear interpolation algorithm for precise, synchronized multi-axis stepper motor coordination alongside software-timed PWM hobby servo pen actuation.

### 📐 Engineering Philosophy: Synchronous Open Loop Design
Because standard stepper motors and hobby servos operate without optical encoders or positional feedback loops, the physical hardware cannot report back when it has reached its destination. 

To solve this, the firmware uses a deliberate, highly predictable **synchronous timing model**. The microcontroller calculates precise steps and utilizes blocking timing delays (`_delay_us`) during axis movement to guarantee that the mechanical parts have physically caught up before the system accepts or processes the next G-code instruction from the Bluetooth stream.
