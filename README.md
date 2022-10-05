# garage-door

ESP8266-based garage door controller (status and control via MQTT)

This is a personal project to control my garage door opener remotely using my phone. It's designed specifically around my garage door opener, which is pretty old and I'm not even sure what make and model it is. It comprises a circuit containing a NodeMCU ESP8266 dev board, some components, and a small modification to the door opener to expose additional signals.

My opener has a series of screw terminals, notably:

- GND - common ground for all other terminals
- P/B (push button) - a 5V signal that when pulled low, causes the door to open or close. I believe it is equivalent to the button on the side of the unit
- 15V - a supply of power, which is poorly regulated so is often more like 18-20V

I had two goals. Firstly, to install a button inside the house with an LED that shows the status of the door.  For that I used the [PDL 680TMNPB-WH](https://www.scottelectrical.co.nz/shop/switch-gear-wiring-devices/modulesmechs/specialty/pdl-switch-elv-tactile-red-neon/) which combines a momentary switch (button) with an LED. But the LED has a built-in resistor such that it is designed to take a 24V input, which means I can't drive it easily from a microcontroller. So, I elected to use the 15V supply to power the LED. In fact, even with 5V the LED is visible, but dim. 15-20V is good and bright.

The LED is on when the door is open, flashing when it is partially open (e.g. opening or closing), and off when it is closed. The button is connected directly (electrically speaking) to the P/B terminal on the garage door so that it will work even if my device is unpowered.

The second goal is to control it from my phone. That means being able to see the status, and actuate the door. To achieve this, I made the microcontroller send to Home Assistant via MQTT status updates, and accept commands, which cause it to pull the P/B signal low to simulate a button press.

The list of terminals I gave above did not include any terminal which can be used to detect the state of the door. But the opener does have two microswitches that are actuated by cams to tell it when it has fully closed or fully opened. These are also 5V signals that are pulled low by the microswitches. I tapped into these and exposed them, along with the other three termials, on a convenient DB9 socket which I mounted on the opener. Using these two signals, my microcontroller can tell if the door is open, closed, or partially open. The fourth state, where both signals are low, should never occur.

This repository contains the firmware for the ESP8266, and the circuit diagram and veroboard layout I used. I designed in support for end-user WiFi setup, but unfortunately the MQTT settings are hard-coded. I don't know if I will ever bother to change that since I am not highly likely to have additional users to support, and if I do get some, they can just change the code.
