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
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include "gamepads.h"
#include "n64.h"
#include "gcn64_protocol.h"

#undef BUTTON_A_RUMBLE_TEST

/*********** prototypes *************/
static void n64Init(unsigned char chn);
static char n64Update(unsigned char chn);
static char n64Changed(unsigned char chn);
static void n64GetReport(unsigned char chn, gamepad_data *dst);
static void n64SetVibration(unsigned char chn, char enable);

static char must_rumble[GAMEPAD_MAX_CHANNELS] = { };
#ifdef BUTTON_A_RUMBLE_TEST
static char force_rumble[GAMEPAD_MAX_CHANNELS] = { };
#endif
static unsigned char n64_rumble_state[GAMEPAD_MAX_CHANNELS] = { };

unsigned char tmpdata[40]; // Shared between channels

#define RSTATE_UNAVAILABLE	0
#define RSTATE_OFF			1
#define RSTATE_TURNON		2
#define RSTATE_ON			3
#define RSTATE_TURNOFF		4
#define RSTATE_INIT			5

static void n64Init(unsigned char chn)
{
	n64Update(chn);
}

static char initRumble(unsigned char chn)
{
	int count;
	unsigned char data[4];

	tmpdata[0] = N64_EXPANSION_WRITE;
	tmpdata[1] = 0x80;
	tmpdata[2] = 0x01;
	memset(tmpdata+3, 0x80, 32);

	count = gcn64_transaction(chn, tmpdata, 35, data, sizeof(data));
	if (count == 1)
		return 0;

	return -1;
}

static char controlRumble(unsigned char chn, char enable)
{
	int count;
	unsigned char data[4];

	tmpdata[0] = N64_EXPANSION_WRITE;
	tmpdata[1] = 0xc0;
	tmpdata[2] = 0x1b;
	memset(tmpdata+3, enable ? 0x01 : 0x00, 32);
	count = gcn64_transaction(chn, tmpdata, 35, data, sizeof(data));
	if (count == 1)
		return 0;

	return -1;
}

static char n64Update(unsigned char chn)
{
	unsigned char count;
	unsigned char x,y;
	unsigned char btns1, btns2;
	unsigned char caps[N64_CAPS_REPLY_LENGTH];
	unsigned char status[N64_GET_STATUS_REPLY_LENGTH];

	/* Pad answer to N64_GET_CAPABILITIES
	 *
	 * 0x050000 : 0000 0101 0000 0000 0000 0000 : No expansion pack
	 * 0x050001 : 0000 0101 0000 0000 0000 0001 : With expansion pack
	 * 0x050002 : 0000 0101 0000 0000 0000 0010 : Expansion pack removed
	 *
	 * Bit 0 tells us if there is something connected to the expansion port.
	 * Bit 1 tells is if there was something connected that has been removed.
	 */
	tmpdata[0] = N64_GET_CAPABILITIES;
	count = gcn64_transaction(chn, tmpdata, 1, caps, sizeof(caps));
	if (count != N64_CAPS_REPLY_LENGTH) {
		// a failed read could mean the pack or controller was gone. Init
		// will be necessary next time we detect a pack is present.
		n64_rumble_state[chn] = RSTATE_INIT;
		return -1;
	}

	/* Detect when a pack becomes present and schedule initialisation when it happens. */
	if ((caps[2] & 0x01) && (n64_rumble_state[chn] == RSTATE_UNAVAILABLE)) {
		n64_rumble_state[chn] = RSTATE_INIT;
	}

	/* Detect when a pack is removed. */
	if (!(caps[2] & 0x01) || (caps[2] & 0x02) ) {
		n64_rumble_state[chn] = RSTATE_UNAVAILABLE;
	}
#ifdef BUTTON_A_RUMBLE_TEST
	must_rumble[chn] = force_rumble[chn];
	//printf("Caps: %02x %02x %02x\r\n", caps[0], caps[1], caps[2]);
#endif


	switch (n64_rumble_state[chn])
	{
		case RSTATE_INIT:
			/* Retry until the controller answers with a full byte. */
			if (initRumble(chn) != 0) {
				if (initRumble(chn) != 0) {
					n64_rumble_state[chn] = RSTATE_UNAVAILABLE;
				}
				break;
			}

			if (must_rumble[chn]) {
				controlRumble(chn, 1);
				n64_rumble_state[chn] = RSTATE_ON;
			} else {
				controlRumble(chn, 0);
				n64_rumble_state[chn] = RSTATE_OFF;
			}
			break;

		case RSTATE_TURNON:
			if (0 == controlRumble(chn, 1)) {
				n64_rumble_state[chn] = RSTATE_ON;
			}
			break;

		case RSTATE_TURNOFF:
			if (0 == controlRumble(chn, 0)) {
				n64_rumble_state[chn] = RSTATE_OFF;
			}
			break;

		case RSTATE_ON:
			if (!must_rumble[chn]) {
				 controlRumble(chn, 0);
				 n64_rumble_state[chn] = RSTATE_OFF;
			}
			break;

		case RSTATE_OFF:
			if (must_rumble[chn]) {
				 controlRumble(chn, 1);
				n64_rumble_state[chn] = RSTATE_ON;
			}
			break;
	}

	tmpdata[0] = N64_GET_STATUS;
	count = gcn64_transaction(chn, tmpdata, 1, status, sizeof(status));
	if (count != N64_GET_STATUS_REPLY_LENGTH) {
		return -1;
	}

/*
	Bit	Function
	0	A
	1	B
	2	Z
	3	Start
	4	Directional Up
	5	Directional Down
	6	Directional Left
	7	Directional Right
	8	unknown (always 0)
	9	unknown (always 0)
	10	L
	11	R
	12	C Up
	13	C Down
	14	C Left
	15	C Right
	16-23: analog X axis
	24-31: analog Y axis
 */

	btns1 = status[0];
	btns2 = status[1];
	x = status[2];
	y = status[3];

#ifdef BUTTON_A_RUMBLE_TEST
	if (btns1 & 0x80) {
		force_rumble[chn] = 1;
	} else {
		force_rumble[chn] = 0;
	}
#endif

	last_built_report[chn].pad_type = PAD_TYPE_N64;
	last_built_report[chn].n64.buttons = (btns1 << 8) | btns2;
	last_built_report[chn].n64.x = x;
	last_built_report[chn].n64.y = y;

#ifdef PAD_DATA_HAS_RAW
	/* Copy all the data as-is for the raw field */
	last_built_report[chn].n64.raw_data[0] = btns1;
	last_built_report[chn].n64.raw_data[1] = btns2;
	last_built_report[chn].n64.raw_data[2] = x;
	last_built_report[chn].n64.raw_data[3] = y;
#endif

	/* Some cheap non-official controllers
	 * use the full 8 bit range instead of the
	 * normal +-80 observed on official controllers. In
	 * particular, some units (but not all!) produced
	 * by TTX. The symptom is usually "The joystick
	 * left direction does not work".
	 *
	 * So I limit values to the -127 to +127 range,
	 * otherwise it causes problem later
	 * when the sign is inverted. Using 16 bit
	 * signed numbers instead of 8 bit would solve
	 * this, but this is only for cheap, not
	 * even worth using controllers so I don't
	 * care.
	 *
	 * The joystick will now "work" as bad as it would
	 * on a N64, or maybe a little better. This should
	 * help people realise they got what the paid for
	 * instead of suspecting the adapter. */
    if (last_built_report[chn].n64.x == -128)
        last_built_report[chn].n64.x = -127;
    if (last_built_report[chn].n64.y == -128)
        last_built_report[chn].n64.y = -127;

	return 0;
}

static char n64Probe(unsigned char chn)
{
	int count;
	char i;
	unsigned char tmp;
	unsigned char data[4];

	/* Pad answer to N64_GET_CAPABILITIES
	 *
	 * 0x050000 : 0000 0101 0000 0000 0000 0000 : No expansion pack
	 * 0x050001 : 0000 0101 0000 0000 0000 0001 : With expansion pack
	 * 0x050002 : 0000 0101 0000 0000 0000 0010 : Expansion pack removed
	 *
	 * Bit 0 tells us if there is something connected to the expansion port.
	 * Bit 1 tells is if there was something connected that has been removed.
	 */

	n64_rumble_state[chn] = RSTATE_UNAVAILABLE;

	for (i=0; i<15; i++)
	{
		_delay_ms(30);

		tmp = N64_GET_CAPABILITIES;
		count = gcn64_transaction(chn, &tmp, 1, data, sizeof(data));

		if (count == N64_CAPS_REPLY_LENGTH) {
			return 1;
		}
	}
	return 0;
}

static char n64Changed(unsigned char chn)
{
	return memcmp(&last_built_report[chn], &last_sent_report[chn], sizeof(gamepad_data));
}

static void n64GetReport(unsigned char chn, gamepad_data *dst)
{
	if (dst)
		memcpy(dst, &last_built_report[chn], sizeof(gamepad_data));

	memcpy(&last_sent_report[chn], &last_built_report[chn], sizeof(gamepad_data));
}

static void n64SetVibration(unsigned char chn, char enable)
{
	must_rumble[chn] = enable;
}

static Gamepad N64Gamepad = {
	.init					= n64Init,
	.update					= n64Update,
	.changed				= n64Changed,
	.getReport				= n64GetReport,
	.probe					= n64Probe,
	.setVibration			= n64SetVibration,
};

Gamepad *n64GetGamepad(void)
{
	return &N64Gamepad;
}
