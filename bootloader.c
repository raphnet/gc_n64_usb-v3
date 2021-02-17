/*	gc_n64_usb : Gamecube or N64 controller to USB adapter firmware
	Copyright (C) 2007-2021  Raphael Assenat <raph@raphnet.net>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "usb.h"

void enterBootLoader(void)
{
	cli();
	usb_shutdown();
	_delay_ms(10);

#if defined(__AVR_ATmega32U2__)
	// ATmega32u2   : 0x3800
	asm volatile(
		"cli			\n"
		"ldi r30, 0x00	\n" // ZL
		"ldi r31, 0x38	\n" // ZH
		"ijmp");
#elif defined(__AVR_AT90USB1286__) || defined(__AVR_AT90USB1287__)
	// AT90USB1287/1286 : 0xF000 (word address)
	asm volatile(
		"cli			\n"
		"ldi r30, 0x00	\n" // ZL
		"ldi r31, 0xF0	\n" // ZH
		"ijmp");
#else
	#error Bootloader address unknown for this CPU
#endif
}

void resetFirmware(void)
{
	usb_shutdown();

	// jump to the application reset vector
	asm volatile(
		"cli			\n"
		"ldi r30, 0x00	\n" // ZL
		"ldi r31, 0x00	\n" // ZH
		"ijmp");
}
