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

#define FLAG_GC_FULL_SLIDERS	1
#define FLAG_GC_INVERT_TRIGS	2

void eeprom_app_write_defaults(void);
void eeprom_app_ready(void);

unsigned char config_setParam(unsigned char param, const unsigned char *value);
unsigned char config_getParam(unsigned char param, unsigned char *value, unsigned char max_len);

#endif
