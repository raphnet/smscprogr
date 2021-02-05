# SMSCPROGR : SMS/Mark-III cartridge reader/programmer

## Overview

This repository contains the firmware source code for a master system game
cartridge reader and programmer.

For more information about this, and use examples and instructions, please
visit the project page:

* [Project page (english)](https://www.raphnet.net/electronique/sms_cartridge_programmer/index_en.php)
* [Project page (french)](https://www.raphnet.net/electronique/sms_cartridge_programmer/index.php)


## Compilation

On a Linux or Unit system, with make, gcc-avr and avr-libc installed and in your path, it should be
a simple matter of typing "make".

You will also need dfu-programmer to program the micro-controller. If you have the correct permissions
set in udev, you should be able to type "make flash" to program the micro-controller for the first time.

If you are reprogramming or upgrading, you must enter bootloader mode first by using the "boot" command
in the virtual com port.


##  Authors

* **Raphael Assenat**


## Licence

GPL. See LICENSE for more information.
