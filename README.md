# SMSCPROGR : SMS/Mark-III cartridge reader/programmer

## Overview

This repository contains the firmware source code for a master system game
cartridge reader and programmer.

For more information about this, and use examples and instructions, please
visit the project page:

* [Project page (english)](https://www.raphnet.net/electronique/sms_cartridge_programmer/index_en.php)
* [Project page (french)](https://www.raphnet.net/electronique/sms_cartridge_programmer/index.php)


## Compilation


### AVR firmware

On a Linux or Unit system, with make, gcc-avr and avr-libc installed and in your path, it should be
a simple matter of typing "make".

You will also need dfu-programmer to program the micro-controller. If you have the correct permissions
set in udev, you should be able to type "make flash" to program the micro-controller for the first time.

If you are reprogramming or upgrading, you must enter bootloader mode first by using the "boot" command
in the virtual com port.


### CPLD

You will need WinCUPL, which is at this date (2021-02) available for free from
microchip here:

[WinCUPL](https://www.microchip.com/en-us/products/fpgas-and-plds/spld-cplds/pld-design-resources)

I could not get the GUI to work reliably on my system (it kept crashing,
especially in simulation) so instead I use batch files. But it's probably
better like this anyway.

Run setup.bat once (optionally edit it first if the paths are different on your system), then run compile.bat to compile and generate a new .JED file and use the ATMISP tool to program the CPLD.


## Use

The cartridge reader/programmer can be controlled using your favorite serial terminal software. For instance,
under Linux the programmer should appear as /dev/ttyACMx and you can simply fire minicom -D /dev/ttyACMx 
and type ? followed by enter to get a list of available commands.

For examples, please visit the project homepage.


## Python client

The client/ directory contains a python tool which opens the device (ttyACMx) and types commands for you. If
you are producing a large batch of cartridges, this is recommended as it is much faster (less typing!).

The following python packages are required:

 * python3-serial
 * xmodem

For examples, please visit the project homepage.


##  Authors

* **Raphael Assenat**


## Licence

GPL. See LICENSE for more information.
