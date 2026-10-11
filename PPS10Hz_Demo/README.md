# LiDAR PPS 10 Hz test

This demo outputs a 10 Hz PWM signal on PB5 using TIM3_CH2. The high level is 10 ms.

To use it with the existing Keil project, add this `main.c` to the target and exclude `User/main.c` from that target so there is only one `main()` function. The project must retain the STM32F10x Standard Peripheral Library source files and the existing 72 MHz system clock setup.

Connect PB5 to the LiDAR PPS input and connect the MCU ground to the LiDAR ground. Measure PB5 with an oscilloscope or logic analyzer to confirm a 100 ms period and 10 ms high pulse.

This is an output-only test. For MCU-side input capture or loopback measurement, connect PB5 to a separate timer input pin and add that pin's capture configuration.
