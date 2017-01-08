#ifndef _usbstrings_h__
#define _usbstrings_h__

#include <avr/pgmspace.h>

extern const wchar_t *g_usb_strings[];

/* Sample: "Dual Gamecube to USB v3.4" (25) */
#define PRODUCT_STRING_MAXCHARS	32

#define NUM_USB_STRINGS	3

/* Array indexes (i.e. zero-based0 */
#define USB_STRING_SERIAL_IDX	2

/**
 * \param str Must be in PROGMEM
 */
void usbstrings_changeProductString_P(const char *str);

#endif
