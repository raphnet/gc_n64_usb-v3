# Gamecube/N64 to USB adapter

## Introduction

This is the source code for a Gamecube/N64 controller to USB adapter firmware
meant to run on [raphnet.net Multiuse PCB-X](http://www.raphnet.net/electronique/multiuse_pcbX/index_en.php).

The project homepage is located at: http://www.raphnet.net/electronique/multiuse_pcbX/index.php

## License

The project is released under the General Public License version 3.

## Compiling the firmware

You will need a working avr-gcc toolchain with avr-libc and standard utilities such as make. Just
type 'make' and it should build just fine. Under Linux at least.

## Programming the firmware

The makefile has a convenient 'flash' target which sends a command to the firmware to enter
the bootloader and then executes dfu-programmer (it must of course be installed) with the
correct arguments.

