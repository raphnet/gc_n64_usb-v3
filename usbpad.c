#include <stdint.h>
#include "gamepads.h"
#include "usbpad.h"
#include "mappings.h"

#define REPORT_ID	1
#define REPORT_SIZE	15




void usbpad_init(void)
{
}

int usbpad_getReportSize(void)
{
	return REPORT_SIZE;
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

static void buildIdleReport(unsigned char dstbuf[REPORT_SIZE])
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

static void buildReportFromGC(const gc_pad_data *gc_data, unsigned char dstbuf[REPORT_SIZE])
{
	int16_t xval,yval;
	uint16_t buttons;

//	printf("GC x: %4d,  y: %4d\r\n", gc_data->x, gc_data->y);

	/* Force official range */
	xval = minmax(gc_data->x, -100, 100);
	yval = minmax(gc_data->y, -100, 100);

	/* Scale -100 ... + 1000 to -16000 ... +16000 */
	xval *= 160;
	yval *= 160;

	/* Unsign for HID report */
	xval += 16000;
	yval += 16000;

	dstbuf[1] = ((uint8_t*)&xval)[0];
	dstbuf[2] = ((uint8_t*)&xval)[1];
	dstbuf[3] = ((uint8_t*)&yval)[0];
	dstbuf[4] = ((uint8_t*)&yval)[1];

	buttons = mappings_do(MAPPING_GAMECUBE_DEFAULT, gc_data->buttons);
	btnsToReport(buttons, dstbuf+13);
}

static void buildReportFromN64(const n64_pad_data *n64_data, unsigned char dstbuf[REPORT_SIZE])
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

void usbpad_buildReport(const gamepad_data *pad_data, unsigned char dstbuf[REPORT_SIZE])
{
	/* Always start with an idle report. Specific report builders can just
	 * simply ignore unused parts */
	buildIdleReport(dstbuf);

	if (pad_data)
	{
		switch (pad_data->pad_type)
		{
			case PAD_TYPE_N64:
				buildReportFromN64(&pad_data->n64, dstbuf);
				break;

			case PAD_TYPE_GAMECUBE:
				buildReportFromGC(&pad_data->gc, dstbuf);
				break;

			default:
				break;
		}
	}
}

