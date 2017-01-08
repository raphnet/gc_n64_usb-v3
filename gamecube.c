/*	gc_n64_usb : Gamecube or N64 controller to USB adapter firmware
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
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include "gamepads.h"
#include "gamecube.h"
#include "gcn64_protocol.h"

/*********** prototypes *************/
static void gamecubeInit(unsigned char chn);
static char gamecubeUpdate(unsigned char chn);
static char gamecubeChanged(unsigned char chn);

static char gc_rumbling[GAMEPAD_MAX_CHANNELS] = { };
static char origins_set[GAMEPAD_MAX_CHANNELS] = { };
static unsigned char orig_x[GAMEPAD_MAX_CHANNELS];
static unsigned char orig_y[GAMEPAD_MAX_CHANNELS];
static unsigned char orig_cx[GAMEPAD_MAX_CHANNELS];
static unsigned char orig_cy[GAMEPAD_MAX_CHANNELS];

static void gamecubeInit(unsigned char chn)
{
	gamecubeUpdate(chn);
}

void gc_decodeAnswer(unsigned char chn, unsigned char data[8])
{
	unsigned char x,y,cx,cy;

	// Note: Checking seems a good idea, adds protection
	// against corruption (if the "constant" bits are invalid,
	// maybe others are : Drop the packet).
	//
	// However, I have seen bit 2 in a high state. To be as compatible
	// as possible, I decided NOT to look at these bits since instead
	// of being "constant" bits they might have a meaning I don't know.

	// Check the always 0 and always 1 bits
#if 0
	if (gcn64_workbuf[0] || gcn64_workbuf[1] || gcn64_workbuf[2])
		return 1;

	if (!gcn64_workbuf[8])
		return 1;
#endif

/*
	(Source: Nintendo Gamecube Controller Protocol
		updated 8th March 2004, by James.)

	Bit		Function
	0-2		Always 0    << RAPH: Not true, I see 001!
	3		Start
	4		Y
	5		X
	6		B
	7		A
	8		Always 1
	9		L
	10		R
	11		Z
	12-15	Up,Down,Right,Left
	16-23	Joy X
	24-31	Joy Y
	32-39	C Joystick X
	40-47	C Joystick Y
	48-55	Left Btn Val
	56-63	Right Btn Val
 */

	last_built_report[chn].pad_type = PAD_TYPE_GAMECUBE;
	last_built_report[chn].gc.buttons = data[0] | data[1] << 8;
	x = data[2];
	y = data[3];
	cx = data[4];
	cy = data[5];
	last_built_report[chn].gc.lt = data[6];
	last_built_report[chn].gc.rt = data[7];

#ifdef PAD_DATA_HAS_RAW
	memcpy(last_built_report[chn].gc.raw_data, data, 8);
#endif

	if (origins_set[chn]) {
		last_built_report[chn].gc.x = ((int)x-(int)orig_x[chn]);
		last_built_report[chn].gc.y = ((int)y-(int)orig_y[chn]);
		last_built_report[chn].gc.cx = ((int)cx-(int)orig_cx[chn]);
		last_built_report[chn].gc.cy = ((int)cy-(int)orig_cy[chn]);
	} else {
		orig_x[chn] = x;
		orig_y[chn] = y;
		orig_cx[chn] = cx;
		orig_cy[chn] = cy;
		last_built_report[chn].gc.x = 0;
		last_built_report[chn].gc.y = 0;
		last_built_report[chn].gc.cx = 0;
		last_built_report[chn].gc.cy = 0;
		origins_set[chn] = 1;
	}
}

static char gamecubeUpdate(unsigned char chn)
{
	unsigned char tmpdata[GC_GETSTATUS_REPLY_LENGTH];
	unsigned char count;

	tmpdata[0] = GC_GETSTATUS1;
	tmpdata[1] = GC_GETSTATUS2;
	tmpdata[2] = GC_GETSTATUS3(gc_rumbling[chn]);

	count = gcn64_transaction(chn, tmpdata, 3, tmpdata, GC_GETSTATUS_REPLY_LENGTH);
	if (count != GC_GETSTATUS_REPLY_LENGTH) {
		return 1;
	}

	gc_decodeAnswer(chn, tmpdata);

	return 0;
}

static void gamecubeHotplug(unsigned char chn)
{
	// Make sure next read becomes the refence center values
	origins_set[chn] = 0;
}

static char gamecubeProbe(unsigned char chn)
{
	origins_set[chn] = 0;

	if (gamecubeUpdate(chn)) {
		return 0;
	}

	return 1;
}

static char gamecubeChanged(unsigned char chn)
{
	return memcmp(&last_built_report[chn], &last_sent_report[chn], sizeof(gamepad_data));
}

static void gamecubeGetReport(unsigned char chn, gamepad_data *dst)
{
	if (dst)
		memcpy(dst, &last_built_report[chn], sizeof(gamepad_data));
}

static void gamecubeVibration(unsigned char chn, char enable)
{
	gc_rumbling[chn] = enable;
}

Gamepad GamecubeGamepad = {
	.init					= gamecubeInit,
	.update					= gamecubeUpdate,
	.changed				= gamecubeChanged,
	.getReport				= gamecubeGetReport,
	.probe					= gamecubeProbe,
	.setVibration			= gamecubeVibration,
	.hotplug				= gamecubeHotplug,
};

Gamepad *gamecubeGetGamepad(void)
{
	return &GamecubeGamepad;
}
