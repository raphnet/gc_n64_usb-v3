# Gamecube/N64 to USB adapter

## Introduction

This is the source code for a Gamecube/N64 controller to USB adapter firmware
meant to run on [raphnet.net Multiuse PCB-X](http://www.raphnet.net/electronique/multiuse_pcbX/index_en.php).

The project homepage is located at: http://www.raphnet.net/electronique/multiuse_pcbX/index.php

## License

The project is released under the General Public License version 3.

## Directories

 * / : The firmware source code
 * tool/ : This directory contains utilities to configure and update an adapter, manipulate mempak
 image files, etc.
 * scripts/ : Scripts and/or useful files

## Compiling the firmware

You will need a working avr-gcc toolchain with avr-libc and standard utilities such as make. Just
type 'make' and it should build just fine. Under Linux at least.

## Programming the firmware

The makefile has a convenient 'flash' target which sends a command to the firmware to enter
the bootloader and then executes dfu-programmer (it must of course be installed) with the
correct arguments.

Note that the tool must be compiled first and your permissions must also be set so that your
user may access the device. See 'Using the tools' below for more information.

## Compiling the tools

In the tool/ directory, just type make.

There are a few dependencies:
 - libhidapi-dev
 - libhidapi-hidraw0
 - pkg-config
 - gtk3 (for the gui only)

Provided you have all the dependencies installed, under Linux at least, it should
compile without errors. For other environments such has MinGW, there are provisions
in the makefile to auto-detect and tweak the build accordingly, but it if fails, be
prepared to hack the makefile.

## Using the tools

Under Linux, you should configure udev to give the proper permissions to your user,
otherwise communicating with the device won't be possible. The same requirement
also applies to dfu-programmer.

An easy way to do this is to copy the two files below to /etc/udev/rules.d, restart
udev and reconnect the devices.

scripts/99-atmel-dfu.rules
scripts/99-raphnet.rules

For information on how to actually /use/ the tools, try --help. Ex:

$ ./gcn64ctl --help
