#include <avr/io.h>
#include "usb.h"

void enterBootLoader(void)
{
	usb_shutdown();

	// AT90USB1287/1286 : 0xF000 (word address)
	// ATmega32u2   : ???
	asm volatile(
		"cli			\n"
		"ldi r30, 0x00	\n" // ZL
		"ldi r31, 0xF0	\n" // ZH
		"ijmp");

}

