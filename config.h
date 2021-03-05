#ifndef _config_h__
#define _config_h__

#define NUM_CHANNELS	4
#define SERIAL_NUM_LEN	6
struct eeprom_cfg {
	uint8_t serial[SERIAL_NUM_LEN];
	uint8_t mode;
	uint8_t poll_interval[NUM_CHANNELS];
	uint32_t flags;
};

#define FLAG_GC_FULL_SLIDERS			0x01
#define FLAG_GC_INVERT_TRIGS			0x02
#define FLAG_GC_SLIDERS_AS_BUTTONS		0x04
#define FLAG_DISABLE_ANALOG_TRIGGERS	0x08
#define FLAG_SWAP_STICK_AND_DPAD		0x10

void eeprom_app_write_defaults(void);
void eeprom_app_ready(void);

unsigned char config_setParam(unsigned char param, const unsigned char *value);
unsigned char config_getParam(unsigned char param, unsigned char *value, unsigned char max_len);

uint8_t config_getSupportedParams(uint8_t *dst);

#endif
