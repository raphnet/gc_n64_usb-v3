#ifndef _mappings_h__
#define _mappings_h__

#include <stdint.h>

struct mapping {
	uint16_t ctl_btn;
	uint16_t usb_btn;
};

#define MAPPING_GAMECUBE_DEFAULT	0x00
#define MAPPING_N64_DEFAULT			0x10

uint16_t mappings_do(uint8_t mapping_id, uint16_t input);

#endif // _mappings_h__
