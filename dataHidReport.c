/*	gc_n64_usb : Gamecube or N64 controller to USB adapter firmware
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
#include <avr/pgmspace.h>
#include <ctype.h>

const uint8_t dataHidReport[] PROGMEM = {
	0x06, 0x00, 0xff,   // USAGE_PAGE (Generic Desktop)
	0x09, 0x01,         // USAGE (Vendor Usage 1)
	0xa1, 0x01,         // COLLECTION (Application)
	0x15, 0x00,         //   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,   //   LOGICAL_MAXIMUM (255)
	0x75, 0x08,         //   REPORT_SIZE (8)
	0x95, 40,         //   REPORT_COUNT (40)
	0x09, 0x01,         //   USAGE (Vendor defined)
	0xB1, 0x00,         //   FEATURE (Data,Ary,Abs)
	0xc0                // END_COLLECTION
};
