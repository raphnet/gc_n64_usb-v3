/*	gc_n64_usb : Gamecube or N64 controller to USB firmware
	Copyright (C) 2007-2016  Raphael Assenat <raph@raphnet.net>

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
#include "intervaltimer2.h"

void intervaltimer2_init(void)
{
	TCCR0A = (1<<WGM01);
	TCCR0B = (1<<CS02) | (1<<CS00); // CTC, /1024 prescaler
}

void intervaltimer2_set16ms(void)
{
	int interval_ms = 16;

	// Maximum 16ms. 17ms overflows the 8-bit OCR0A register (value of 265)
	// 16ms: 250

	TCNT0 = 0;
	OCR0A = interval_ms * (F_CPU/1024) / 1000;
}

char intervaltimer2_get(void)
{
	if (TIFR0 & (1<<OCF0A)) {
		TIFR0 = 1<<OCF0A;
		return 1;
	}

	return 0;
}
