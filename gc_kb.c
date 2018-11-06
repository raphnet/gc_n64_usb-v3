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
#include "gc_kb.h"
#include "hid_keycodes.h"
#include "gcn64_protocol.h"


// http://www2d.biglobe.ne.jp/~msyk/keyboard/layout/usbkeycode.html

static const unsigned char gc_to_hid_table[] PROGMEM = {
	GC_KEY_RESERVED,		HID_KB_NOEVENT,
	GC_KEY_HOME,			HID_KB_HOME,
	GC_KEY_END,				HID_KB_END,
	GC_KEY_PGUP,			HID_KB_PGUP,
	GC_KEY_PGDN,			HID_KB_PGDN,
	GC_KEY_SCROLL_LOCK,		HID_KB_SCROLL_LOCK,
	GC_KEY_DASH_UNDERSCORE, HID_KB_DASH_UNDERSCORE,
	GC_KEY_PLUS_EQUAL,		HID_KB_EQUAL_PLUS,
	GC_KEY_YEN,				HID_KB_INTERNATIONAL3,
	GC_KEY_OPEN_BRKT_BRACE,	HID_KB_OPEN_BRKT_BRACE,
	GC_KEY_SEMI_COLON_COLON,HID_KB_SEMI_COLON_COLON,
	GC_KEY_QUOTES,			HID_KB_QUOTES,
	GC_KEY_CLOSE_BRKT_BRACE,HID_KB_CLOSE_BRKT_BRACE,
	GC_KEY_BRACKET_MU,		HID_KB_NONUS_HASH_TILDE,
	GC_KEY_COMMA_ST,		HID_KB_COMMA_SMALLER_THAN,
	GC_KEY_PERIOD_GT,		HID_KB_PERIOD_GREATER_THAN,
	GC_KEY_SLASH_QUESTION,	HID_KB_SLASH_QUESTION,
	GC_KEY_INTERNATIONAL1,	HID_KB_INTERNATIONAL1,
	GC_KEY_ESC,				HID_KB_ESCAPE,
	GC_KEY_INSERT,			HID_KB_INSERT,
	GC_KEY_DELETE,			HID_KB_DELETE_FORWARD,
	GC_KEY_HANKAKU,			HID_KB_GRAVE_ACCENT_AND_TILDE,
	GC_KEY_BACKSPACE,		HID_KB_BACKSPACE,
	GC_KEY_TAB,				HID_KB_TAB,
	GC_KEY_CAPS_LOCK,		HID_KB_CAPS_LOCK,
	GC_KEY_MUHENKAN,		HID_KB_INTERNATIONAL5,
	GC_KEY_SPACE,			HID_KB_SPACE,
	GC_KEY_HENKAN,			HID_KB_INTERNATIONAL4,
	GC_KEY_KANA,			HID_KB_INTERNATIONAL2,
	GC_KEY_LEFT,			HID_KB_LEFT_ARROW,
	GC_KEY_DOWN,			HID_KB_DOWN_ARROW,
	GC_KEY_UP,				HID_KB_UP_ARROW,
	GC_KEY_RIGHT,			HID_KB_RIGHT_ARROW,
	GC_KEY_ENTER,			HID_KB_ENTER,

	/* "shift" keys */
	GC_KEY_LEFT_SHIFT,		HID_KB_LEFT_SHIFT,
	GC_KEY_RIGHT_SHIFT,		HID_KB_RIGHT_SHIFT,
	GC_KEY_LEFT_CTRL,		HID_KB_LEFT_CONTROL,

	/* This keyboard only has a left alt key. But as right alt is required to access some
	 * functions on japanese keyboards, I map the key to right alt.
	 *
	 * eg: RO-MAJI on the hiragana/katakana key */
	GC_KEY_LEFT_ALT,		HID_KB_RIGHT_ALT,
};

unsigned char gcKeycodeToHID(unsigned char gc_code)
{
	int i;

	if (gc_code >= GC_KEY_A && gc_code <= GC_KEY_0) {
		// Note: This works since A-Z, 1-9, 0 have consecutive keycode values.
		return (gc_code - GC_KEY_A) + HID_KB_A;
	}
	if (gc_code >= GC_KEY_F1 && gc_code <= GC_KEY_F12) {
		return (gc_code - GC_KEY_F1) + HID_KB_F1;
	}

	for (i=0; i<sizeof(gc_to_hid_table); i++) {
		if (pgm_read_byte(gc_to_hid_table + i*2) == gc_code) {
			return pgm_read_byte(gc_to_hid_table + i*2 + 1);
		}
	}

	return 0x38; // HID /? key for unknown keys
}

