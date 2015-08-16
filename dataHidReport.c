#include <avr/pgmspace.h>
#include <ctype.h>

const uint8_t dataHidReport[] PROGMEM = {
	0x06, 0x00, 0xff,   // USAGE_PAGE (Generic Desktop)
	0x09, 0x01,         // USAGE (Vendor Usage 1)
	0xa1, 0x01,         // COLLECTION (Application)
	0x15, 0x00,         //   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,   //   LOGICAL_MAXIMUM (255)
	0x75, 0x08,         //   REPORT_SIZE (8)
	0x95, 0x05,         //   REPORT_COUNT (5)
	0x09, 0x01,         //   USAGE (Vendor defined)
	0xB1, 0x00,         //   FEATURE (Data,Ary,Abs)
	0xc0                // END_COLLECTION
};
