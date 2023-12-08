# Aquarium-PWM-LED

This project came up on my mind when I realized the exorbitant price of an aquarium LED light controller. The starting price for a professional equipment like this one can be as high as 1000€.

So, if you can build your own controller with the exact same functions (or even a better one) why spend a fortune in an equipment that, in my opinion, doesn’t worth what it costs.

For this project I used four LED strings. Each string comprises three LEDs. The LEDs have a forward voltage around ~4.0V and forward current around ~750mA meaning that a [IRF3205](resources/IRF3205.pdf) MOSFET is enough to switch each string. These LEDs are known as 3W High Power LED.

My estimative for a project with twelve 3 Watt LEDs is around 50 EUR (this value includes all electronics LEDs and driver).

To build this circuit you need the following items:

- 1x Arduino Nano 3.0
- 1x DS1307 real-time clock module
- 1x Hitachi HD44780 20×4 LCD
- 1x 74HC595 shift register
- 1x LM35 temperature sensor
- 4x IRF3205 N-Channel Power MOSFETs
- 3x 10kOhm resistors
- 1x 10kOhm trimmer
- 2x Push-buttons
- 12x 3 Watt LEDs
- 1x 12V 40 Watt LED driver
- Breadboard and wires

The schematic follows below<br/>
<br/>
![alt text](resources/Aquarium-PWM-LED_bb.png?raw=true)

Some important remarks that should not be forgotten:

1) Before compiling the code you need to download, uncompress and install the following libraries: [RTClib](https://github.com/adafruit/RTClib) and [ShiftPWM](https://github.com/elcojacobs/ShiftPWM). To install simply copy them into the “libraries” folder. Alternatively you can read the official tutorial: [Installing Additional Arduino Libraries](http://arduino.cc/en/Guide/Libraries).

2) The pinout for the IRF3205 N-Channel Power MOSFETs is the following:<br/>
![alt text](resources/to220.png?raw=true)

If you’re using other transistors to switch the LEDs and you are not sure about its pinout, you should read the datasheet.

3) The ground of the LED driver (Driver V-) should be connected to the power-supply’s ground. The positive of the LED driver (Driver V+) should NOT be connected to the positive of the power-supply.

4) The circuit can be powered via Arduino’s USB port or applying a 12V voltage at Arduino’s Vin pin.

5) The only trimmer (variable resistor) in the circuit has a value of 10kOhm and it’s used to regulate contrast of the LCD.

6) In order to view all data available on the LCD I recommend you to use a 20×4 char LCD instead of a 16×2 char LCD.

7) Some LCD boards have the backlight LED pins inverted. So, before connecting it to the circuit, make sure to check the datasheet to avoid damage on your LCD.

8) This project is no longer supported, so, if you have strange characters on the LCD please use an older version of the Arduino IDE (1.0.5 was reported to work).

This is the final result of the illumination system in my South American aquarium biotope:<br/>
<br/>
![alt text](resources/DSC06334.JPG?raw=true)

