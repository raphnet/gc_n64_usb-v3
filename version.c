/*	gc_n64_usb : Gamecube or N64 controller to USB firmware
	Copyright (C) 2007-2013  Raphael Assenat <raph@raphnet.net>

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
#include <avr/pgmspace.h>
#include "version.h"

const char *g_version = VERSIONSTR; // From Makefile
#ifdef STK525
const char g_signature[] PROGMEM = "e106420a-7c54-11e5-ae9a-001bfca3c593";
#else
const char g_signature[] PROGMEM = "9c3ea8b8-753f-11e5-a0dc-001bfca3c593";
#endif
