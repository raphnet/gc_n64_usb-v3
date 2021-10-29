# Gamecube/N64 to USB adapter firmware (3rd generation)

## Introduction

This is the source code for a Gamecube/N64 controller to USB adapter firmware
meant to run on [raphnet.net Multiuse PCB-X](http://www.raphnet.net/electronique/multiuse_pcbX/index_en.php).

## Homepage

* English: [Gamecube/N64 controller to USB adapter (Third generation)](http://www.raphnet.net/electronique/gcn64_usb_adapter_gen3/index_en.php)
* French: [Adaptateur manette Gamecube/N64 à USB (Troisième génération)](http://www.raphnet.net/electronique/gcn64_usb_adapter_gen3/index.php)

## License

The project is released under the General Public License version 3.

## Compiling the firmware

You will need a working avr-gcc toolchain with avr-libc and standard utilities such as make. Just
type 'make' and it should build just fine. Under Linux at least.
If you are compiling for a custom board or Arduino running on an ATmega32u4, then run 'make -f Makefile.32u4' instead.

## Programming the firmware

The makefile has a convenient 'flash' target which sends a command to the firmware to enter
the bootloader and then executes dfu-programmer (it must of course be installed) with the
correct arguments.

