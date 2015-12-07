/*	gc_n64_usb : Gamecube or N64 controller to USB firmware
	Copyright (C) 2007-2015  Raphael Assenat <raph@raphnet.net>

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
#include "intervaltimer.h"

void intervaltimer_init(void)
{
	TCCR1A = 0;
	TCCR1B = (1<<WGM12) | (1<<CS12) | (1<<CS00); // CTC, /1024 prescaler
}

void intervaltimer_set(int interval_ms)
{
	static int cur_interval = 0;

	// We are setting TCNT back to zero, and updating
	// the compare match register.
	//
	// To allows for simple repeated calling of this
	// function from the main loop, only touch
	// the counter when the interval changes.
	if (cur_interval != interval_ms) {
		cur_interval = interval_ms;

		TCNT1 = 0;
		OCR1A = interval_ms * (F_CPU/1024) / 1000;
	}
}

char intervaltimer_get(void)
{
	if (TIFR1 & (1<<OCF1A)) {
		TIFR1 = 1<<OCF1A;
		return 1;
	}

	return 0;
}
