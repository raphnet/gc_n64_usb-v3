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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "usb.h"
#include "gamepads.h"
#include "usbpad.h"
#include "mappings.h"
#include "eeprom.h"
#include "config.h"

#define REPORT_ID	1

// Output Report IDs for various functions
#define REPORT_SET_EFFECT			0x01
#define REPORT_SET_STATUS			0x02
#define	REPORT_SET_PERIODIC			0x04
#define REPORT_SET_CONSTANT_FORCE	0x05
#define REPORT_EFFECT_OPERATION		0x0A
#define REPORT_EFFECT_BLOCK_IDX		0x0B
#define REPORT_DISABLE_ACTUATORS	0x0C
#define REPORT_PID_POOL				0x0D

// Feature reports
#define REPORT_CREATE_EFFECT		0x09

// For the 'Usage Effect Operation' report
#define EFFECT_OP_START			1
#define EFFECT_OP_START_SOLO	2
#define EFFECT_OP_STOP			3

// Feature report
#define PID_SIMULTANEOUS_MAX	3
#define PID_BLOCK_LOAD_REPORT	2

static void buildIdleReport(unsigned char dstbuf[USBPAD_REPORT_SIZE]);

void usbpad_init(struct usbpad *pad)
{
	memset(pad, 0, sizeof(struct usbpad));
	buildIdleReport(pad->gamepad_report0);
}

int usbpad_getReportSize(void)
{
	return USBPAD_REPORT_SIZE;
}

static int16_t minmax(int16_t input, int16_t min, int16_t max)
{
	if (input > max)
		return max;
	if (input < min)
		return min;

	return input;
}

static void btnsToReport(unsigned short buttons, unsigned char dstbuf[2])
{
	dstbuf[0] = buttons & 0xff;
	dstbuf[1] = buttons >> 8;
}

static void buildIdleReport(unsigned char dstbuf[USBPAD_REPORT_SIZE])
{
	int i;

	dstbuf[0] = REPORT_ID;

	/* Inactive and centered axis */
	for (i=0; i<6; i++) {
		dstbuf[1+i*2] = 0x80;
		dstbuf[2+i*2] = 0x3e;
	}

	/* Inactive buttons */
	dstbuf[13] = 0;
	dstbuf[14] = 0;
}

static void buildReportFromGC(const gc_pad_data *gc_data, unsigned char dstbuf[USBPAD_REPORT_SIZE])
{
	int16_t xval,yval,cxval,cyval,ltrig,rtrig;
	uint16_t buttons;
	uint16_t gcbuttons = gc_data->buttons;

	/* Force official range */
	xval = minmax(gc_data->x, -100, 100);
	yval = minmax(gc_data->y, -100, 100);
	cxval = minmax(gc_data->cx, -100, 100);
	cyval = minmax(gc_data->cy, -100, 100);
	ltrig = gc_data->lt;
	rtrig = gc_data->rt;

	/* Scale -100 ... + 1000 to -16000 ... +16000 */
	xval *= 160;
	yval *= -160;
	// TODO : Is C-stick different?
	cxval *= 160;
	cyval *= -160;

	if (g_eeprom_data.cfg.flags & FLAG_GC_SLIDERS_AS_BUTTONS) {
		/* In this mode, the sliders control buttons */
		if (ltrig > 32)
			gcbuttons |= GC_BTN_L;
		if (rtrig > 32)
			gcbuttons |= GC_BTN_R;

		/* And the sliders analog values are fixed. */
		ltrig = rtrig = 0;
	}
	else {
		if (g_eeprom_data.cfg.flags & FLAG_GC_FULL_SLIDERS) {
			int16_t lts = (int16_t)ltrig - 127;
			int16_t rts = (int16_t)rtrig - 127;
			lts *= 126;
			ltrig = lts;
			rts *= 126;
			rtrig = rts;

		} else {
			/* Scale 0...255 to 0...16000 */
			ltrig *= 63;
			if (ltrig > 16000) ltrig=16000;
			rtrig *= 63;
			if (rtrig > 16000) rtrig=16000;
		}

		if (g_eeprom_data.cfg.flags & FLAG_GC_INVERT_TRIGS) {
			ltrig = -ltrig;
			rtrig = -rtrig;
		}
	}

	/* Unsign for HID report */
	xval += 16000;
	yval += 16000;
	cxval += 16000;
	cyval += 16000;
	ltrig += 16000;
	rtrig += 16000;

	dstbuf[1] = ((uint8_t*)&xval)[0];
	dstbuf[2] = ((uint8_t*)&xval)[1];
	dstbuf[3] = ((uint8_t*)&yval)[0];
	dstbuf[4] = ((uint8_t*)&yval)[1];

	dstbuf[5] = ((uint8_t*)&cxval)[0];
	dstbuf[6] = ((uint8_t*)&cxval)[1];
	dstbuf[7] = ((uint8_t*)&cyval)[0];
	dstbuf[8] = ((uint8_t*)&cyval)[1];

	dstbuf[9] = ((uint8_t*)&ltrig)[0];
	dstbuf[10] = ((uint8_t*)&ltrig)[1];

	dstbuf[11] = ((uint8_t*)&rtrig)[0];
	dstbuf[12] = ((uint8_t*)&rtrig)[1];

	buttons = mappings_do(MAPPING_GAMECUBE_DEFAULT, gcbuttons);
	btnsToReport(buttons, dstbuf+13);
}

static void buildReportFromN64(const n64_pad_data *n64_data, unsigned char dstbuf[USBPAD_REPORT_SIZE])
{
	int16_t xval, yval;
	uint16_t buttons;

	/* Force official range */
	xval = minmax(n64_data->x, -80, 80);
	yval = minmax(n64_data->y, -80, 80);

	/* Scale -80 ... +80 to -16000 ... +16000 */
	xval *= 200;
	yval *= 200;
	yval = -yval;

	/* Unsign for HID report */
	xval += 16000;
	yval += 16000;

	dstbuf[1] = ((uint8_t*)&xval)[0];
	dstbuf[2] = ((uint8_t*)&xval)[1];
	dstbuf[3] = ((uint8_t*)&yval)[0];
	dstbuf[4] = ((uint8_t*)&yval)[1];

	buttons = mappings_do(MAPPING_N64_DEFAULT, n64_data->buttons);
	btnsToReport(buttons, dstbuf+13);
}

void usbpad_update(struct usbpad *pad, const gamepad_data *pad_data)
{
	/* Always start with an idle report. Specific report builders can just
	 * simply ignore unused parts */
	buildIdleReport(pad->gamepad_report0);

	if (pad_data)
	{
		switch (pad_data->pad_type)
		{
			case PAD_TYPE_N64:
				buildReportFromN64(&pad_data->n64, pad->gamepad_report0);
				break;

			case PAD_TYPE_GAMECUBE:
				buildReportFromGC(&pad_data->gc, pad->gamepad_report0);
				break;

			default:
				break;
		}
	}
}

void usbpad_forceVibrate(struct usbpad *pad, char force)
{
	pad->force_vibrate = force;
}

char usbpad_mustVibrate(struct usbpad *pad)
{
	if (pad->force_vibrate) {
		return 1;
	}

	if (!pad->vibration_on) {
		pad->gamepad_vibrate = 0;
	} else {
		if (pad->constant_force > 0x7f) {
			pad->gamepad_vibrate = 1;
		} else if (pad->periodic_magnitude > 0x7f) {
			pad->gamepad_vibrate = 1;
		} else {
			pad->gamepad_vibrate = 0;
		}
	}

	return pad->gamepad_vibrate;
}

unsigned char *usbpad_getReportBuffer(struct usbpad *pad)
{
	return pad->gamepad_report0;
}

uint16_t usbpad_hid_get_report(struct usbpad *pad, struct usb_request *rq, const uint8_t **dat)
{
	uint8_t report_id = (rq->wValue & 0xff);

	// USB HID 1.11 section 7.2.1 Get_Report
	// wValue high byte : report type
	// wValue low byte : report id
	// wIndex Interface
	switch (rq->wValue >> 8)
	{
		case HID_REPORT_TYPE_INPUT:
			{
				if (report_id == 1) { // Joystick
					// report_id = rq->wValue & 0xff
					// interface = rq->wIndex
					*dat = pad->gamepad_report0;
					printf_P(PSTR("Get joy report\r\n"));
					return USBPAD_REPORT_SIZE
				;
				} else if (report_id == 2) { // 2 : ES playing
					pad->hid_report_data[0] = report_id;
					pad->hid_report_data[1] = 0;
					pad->hid_report_data[2] = pad->_FFB_effect_index;
					printf_P(PSTR("ES playing\r\n"));
					*dat = pad->hid_report_data;
					return 3;
				} else {
					printf_P(PSTR("Get input report %d ??\r\n"), rq->wValue & 0xff);
				}
			}
			break;

		case HID_REPORT_TYPE_FEATURE:
			if (report_id == PID_BLOCK_LOAD_REPORT) {
				pad->hid_report_data[0] = report_id;
				pad->hid_report_data[1] = 0x1; // Effect block index
				pad->hid_report_data[2] = 0x1; // (1: success, 2: oom, 3: load error)
				pad->hid_report_data[3] = 10;
				pad->hid_report_data[4] = 10;
				printf_P(PSTR("block load\r\n"));
				*dat = pad->hid_report_data;
				return 5;
			}
			else if (report_id == PID_SIMULTANEOUS_MAX) {
				pad->hid_report_data[0] = report_id;
				// ROM Effect Block count
				pad->hid_report_data[1] = 0x1;
				pad->hid_report_data[2] = 0x1;
				// PID pool move report?
				pad->hid_report_data[3] = 0xff;
				pad->hid_report_data[4] = 1;
				printf_P(PSTR("simultaneous max\r\n"));
				*dat = pad->hid_report_data;
				return 5;
			}
			else if (report_id == REPORT_CREATE_EFFECT) {
				pad->hid_report_data[0] = report_id;
				pad->hid_report_data[1] = 1;
				printf_P(PSTR("create effect\r\n"));
				*dat = pad->hid_report_data;
				return 2;
			} else {
				printf_P(PSTR("Unknown feature %d\r\n"), rq->wValue & 0xff);
			}
			break;
	}

	printf_P(PSTR("Unhandled hid get report type=0x%02x, rq=0x%02x, wVal=0x%04x, wLen=0x%04x\r\n"), rq->bmRequestType, rq->bRequest, rq->wValue, rq->wLength);
	return 0;
}

uint8_t usbpad_hid_set_report(struct usbpad *pad, const struct usb_request *rq, const uint8_t *data, uint16_t len)
{
	if (len < 1) {
		printf_P(PSTR("shrt\n"));
		return -1;
	}

	if ((rq->wValue >> 8) == HID_REPORT_TYPE_OUTPUT) {

		switch(data[0])
		{
			case REPORT_SET_STATUS:
				printf_P(PSTR("eff. set stat 0x%02x 0x%02x\r\n"),data[1],data[2]);
				break;
			case REPORT_EFFECT_BLOCK_IDX:
				printf_P(PSTR("eff. blk. idx %d\r\n"), data[1]);
				break;
			case REPORT_DISABLE_ACTUATORS:
				printf_P(PSTR("disable actuators\r\n"));
				pad->periodic_magnitude = 0;
				pad->constant_force = 0;
				pad->vibration_on = 0;
				break;
			case REPORT_PID_POOL:
				printf_P(PSTR("pid pool\r\n"));
				break;
			case REPORT_SET_EFFECT:
				pad->_FFB_effect_index = data[1];
				printf_P(PSTR("set effect %d\r\n"), data[1]);
				break;
			case REPORT_SET_PERIODIC:
				pad->periodic_magnitude = data[2];
				printf_P(PSTR("periodic mag: %d"), data[2]);
				break;
			case REPORT_SET_CONSTANT_FORCE:
				if (data[1] == 1) {
					pad->constant_force = data[2];
					printf_P(PSTR("Constant force %d\r\n"), data[2]);
				}
				break;
			case REPORT_EFFECT_OPERATION:
				if (len != 4)
					return -1;
				/* Byte 0 : report ID
				 * Byte 1 : bit 7=rom flag, bits 6-0=effect block index
				 * Byte 2 : Effect operation
				 * Byte 3 : Loop count */
				pad->_loop_count = data[3]<<3;

				printf_P(PSTR("EFFECT OP: rom=%s, idx=0x%02x"), data[1] & 0x80 ? "Yes":"No", data[1] & 0x7F);

				switch(data[1] & 0x7F) // Effect block index
				{
					case 1: // constant force
					case 3: // square
					case 4: // sine
						switch (data[2]) // effect operation
						{
							case EFFECT_OP_START:
								printf_P(PSTR("Start\r\n"));
								pad->vibration_on = 1;
								break;

							case EFFECT_OP_START_SOLO:
								printf_P(PSTR("Start solo\r\n"));
								pad->vibration_on = 1;
								break;

							case EFFECT_OP_STOP:
								printf_P(PSTR("Stop\r\n"));
								pad->vibration_on = 0;
								break;
						}
						break;

					// TODO : should probably drop these from the descriptor since they are

					case 2: // ramp
					case 5: // triangle
					case 6: // sawtooth up
					case 7: // sawtooth down
					case 8: // spring
					case 9: // damper
					case 10: // inertia
					case 11: // friction
					case 12: // custom force data
						printf_P(PSTR("Ununsed effect %d\n"), data[1] & 0x7F);
						break;
				}
				break;
			default:
				printf_P(PSTR("Set output report 0x%02x\r\n"), data[0]);
		}
	}
	else if ((rq->wValue >> 8) == HID_REPORT_TYPE_FEATURE) {
		switch(data[0])
		{
			case REPORT_CREATE_EFFECT:
				pad->_FFB_effect_index = data[1];
				printf_P(PSTR("create effect %d\n"), data[1]);
				break;

			default:
				printf_P(PSTR("What?\n"));
		}
	}
	else {
		printf_P(PSTR("impossible\n"));
	}
	return 0;
}
