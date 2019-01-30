ADS1262 measurement kit
=======================

A bachelor theses project by Finn Stutzenstein.

Description
-----------

This project uses a STM32F746NG evaluation board to control an ADS1262 (`datasheet <http://www.ti.com/lit/ds/symlink/ads1262.pdf>`_)
There is some control software in python to control the measurementdevice.
Fetures:

- Multiple parallel measurements
- Data transmission over ethernet
- Spectral signal processing using FFT
- Controlsoftware in Python
- Full control over the ADC
- Integrated webserver for a webpage
- webpage to control the ADC (WIP)
- WebSocket support.

Directions
----------

For the different software components, see the subfolders:

- `mikrocontroller <https://github.com/FinnStutzenstein/ADS1262/tree/master/mikcrocontroller>`_: An TrueSTUDIO project with the code for the microcontroller.
- `control <https://github.com/FinnStutzenstein/ADS1262/tree/master/control>`_: A collection of Python scripts and the management interface
- `webpage <https://github.com/FinnStutzenstein/ADS1262/tree/master/webpage>`_: An angular project to control the ADC.
