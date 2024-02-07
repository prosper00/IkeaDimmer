# HK32F030MF4P6 - Simple LED Dimmer
This project configures a 15c HK32F030MF4P6 microcontroller as a dimmer module for LED lighting

Rotating the encoder will increase or decrease the brightness of the LED.

PWM output is inverted, for use with a low-side switch on the ground leg of the LED

Pushing the encoder button will save the current brightness of the LED, and turn it off. Pushing the button again will turn the LED back on to the saved level.

User inputs are squared, to provide for a mostly-linear brigtness level response.

PWM switching is running at 2kHz at a resolution of 14 bits for enhanced dynamic range (i.e. smoother low-end brightness increments)

A 'timeout' is implemented to gradually dim the lamp from the current brightness setting down to zero over about an hour. Rotating the encoder or pushing the button at any point will adjust the brightness, reset the timeout counter.

### Connection map
 - Encoder output 'A' - PD1
 - Encoder output 'B' - PD2
 - Encoder pushbutton - PD3

 - PWM output - PC6

 ### Build instructions
 This project is built on the template environment by @IOsetting at https://github.com/IOsetting/hk32f030m-template

 Assuming you have the arm-none-eabi- GCC toolchain installed, build is done with a `make` command. To program the mcu, connect a pyocd-compatible probe (such as a DAPLink), and run `make flash`