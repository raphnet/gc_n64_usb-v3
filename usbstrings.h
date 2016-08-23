#ifndef _usbstrings_h__
#define _usbstrings_h__

extern const wchar_t *g_usb_strings[];

#define NUM_USB_STRINGS	3

/* Array indexes (i.e. zero-based0 */
#define USB_STRING_SERIAL_IDX	2

void usbstrings_changeProductString(const wchar_t *str);

#endif
