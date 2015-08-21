#include <stdint.h>
#include "gamepads.h"
#include "usbpad.h"

#define REPORT_ID	1
#define REPORT_SIZE	15

void usbpad_init(void)
{
}

int usbpad_getReportSize(void)
{
	return REPORT_SIZE;
}

static void buildReportFromGC(const gc_pad_data *gc_data, unsigned char dstbuf[REPORT_SIZE])
{
}

static void buildReportFromN64(const n64_pad_data *n64_data, unsigned char dstbuf[REPORT_SIZE])
{
	int16_t xval, yval;

	xval = n64_data->x;
	yval = n64_data->y;

	/* Force official range */
	if (xval>80) xval = 80; else if (xval<-80) xval = -80;
	if (yval>80) yval = 80; else if (yval<-80) yval = -80;

	/* Scale -80 ... +80 to -16000 ... +16000 */
	xval *= 200;
	yval *= 200;
	yval = -yval;

	/* Unsign for HID report */
	xval += 16000;
	yval += 16000;

	dstbuf[0] = REPORT_ID;
	dstbuf[1] = ((uint8_t*)&xval)[0];
	dstbuf[2] = ((uint8_t*)&xval)[1];
	dstbuf[3] = ((uint8_t*)&yval)[0];
	dstbuf[4] = ((uint8_t*)&yval)[1];

	/* Inactive and centered axis */
	dstbuf[5] = 0x80;
	dstbuf[6] = 0x3e;
	dstbuf[7] = 0x80;
	dstbuf[8] = 0x3e;
	dstbuf[9] = 0x80;
	dstbuf[10] = 0x3e;
	dstbuf[11] = 0x80;
	dstbuf[12] = 0x3e;

	/* TODO : Button mapping */

	dstbuf[13] = n64_data->raw_data[0];
	dstbuf[14] = n64_data->raw_data[1];
}

void usbpad_buildReport(const gamepad_data *pad_data, unsigned char dstbuf[REPORT_SIZE])
{
	switch (pad_data->pad_type)
	{
		case PAD_TYPE_N64:
			buildReportFromN64(&pad_data->n64, dstbuf);
			break;

		case PAD_TYPE_GAMECUBE:
			buildReportFromGC(&pad_data->gc, dstbuf);
			break;
	}
}

