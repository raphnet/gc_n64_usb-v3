#include <avr/pgmspace.h>
#include "mappings.h"
#include "gamepads.h"
#include "usbpad.h"

/* Default N64 and Gamecube mappings meant to work together
 * i.e. Controllers should be mostly interchangeable
 *
 *  - Main buttons first
 *  - Common buttons at the same place
 *  - Similar layout for GC Y/X and N64 C-Left and C-Down
 */

const static struct mapping map_gc_default[] PROGMEM = {
	{ GC_BTN_A,		USB_BTN(0) },
	{ GC_BTN_B,		USB_BTN(1) },
	{ GC_BTN_Z,		USB_BTN(2) },
	{ GC_BTN_START,	USB_BTN(3) },

	{ GC_BTN_L,		USB_BTN(4) },
	{ GC_BTN_R,		USB_BTN(5) },

	{ GC_BTN_Y,		USB_BTN(8) }, // N64 C-Left
	{ GC_BTN_X,		USB_BTN(7) }, // N64 C-Down

	{ GC_BTN_DPAD_UP,		USB_BTN(10) },
	{ GC_BTN_DPAD_DOWN,		USB_BTN(11) },
	{ GC_BTN_DPAD_LEFT,		USB_BTN(12) },
	{ GC_BTN_DPAD_RIGHT,	USB_BTN(13) },

	{	} /* terminator */
};

const static struct mapping map_n64_default[] PROGMEM = {
	{ N64_BTN_A,			USB_BTN(0) },
	{ N64_BTN_B,			USB_BTN(1) },
	{ N64_BTN_Z,			USB_BTN(2) },
	{ N64_BTN_START,		USB_BTN(3) },

	{ N64_BTN_L,			USB_BTN(4) },
	{ N64_BTN_R,			USB_BTN(5) },
	{ N64_BTN_C_UP,			USB_BTN(6) },
	{ N64_BTN_C_DOWN,		USB_BTN(7) }, // GC X

	{ N64_BTN_C_LEFT,		USB_BTN(8) }, // GC_Y
	{ N64_BTN_C_RIGHT,		USB_BTN(9) },

	{ N64_BTN_DPAD_UP,		USB_BTN(10) },
	{ N64_BTN_DPAD_DOWN,	USB_BTN(11) },
	{ N64_BTN_DPAD_LEFT,	USB_BTN(12) },
	{ N64_BTN_DPAD_RIGHT,	USB_BTN(13) },

	{	} /* terminator */
};

static uint16_t domap(const struct mapping *map, uint16_t input)
{
	const struct mapping *cur = map;
	uint16_t out = 0;
	uint16_t ctl_btn, usb_btn;

	while (1) {
		ctl_btn = pgm_read_word(&cur->ctl_btn);
		usb_btn = pgm_read_word(&cur->usb_btn);

		if (!ctl_btn || !usb_btn)
			break;

		if (input & ctl_btn) {
			out |= usb_btn;
		}
		cur++;
	}

	return out;
}

uint16_t mappings_do(uint8_t mapping_id, uint16_t input)
{
	switch(mapping_id) {
		case MAPPING_GAMECUBE_DEFAULT:
			return domap(map_gc_default, input);
		case MAPPING_N64_DEFAULT:
			return domap(map_n64_default, input);
	}

	return 0;
}
