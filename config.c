#include <string.h>
#include "eeprom.h"

#include "requests.h"

struct eeprom_data_struct g_eeprom_data;

/* Called by the eeprom driver if the content
 * was invalid and it needs to write defaults
 * values.  */
void eeprom_app_write_defaults(void)
{
	const char default_serial[SERIAL_NUM_LEN] = { '0','0','0','0','0','1' };

	memcpy(g_eeprom_data.cfg.serial, default_serial, SERIAL_NUM_LEN);
	g_eeprom_data.cfg.mode = CFG_MODE_STANDARD;
}

static void config_set_serial(char serial[SERIAL_NUM_LEN])
{
	memcpy(g_eeprom_data.cfg.serial, serial, SERIAL_NUM_LEN);
	eeprom_commit();
}

unsigned char config_getParam(unsigned char param, unsigned char *value, unsigned char max_len)
{
	switch (param)
	{
		case CFG_PARAM_MODE:
			*value = g_eeprom_data.cfg.mode;
			return 1;
		case CFG_PARAM_SERIAL:
			memcpy(value, g_eeprom_data.cfg.serial, SERIAL_NUM_LEN);
			return SERIAL_NUM_LEN;
	}

	return 0;
}

unsigned char config_setParam(unsigned char param, const unsigned char *value)
{
	if (!value)
		return 0;

	switch (param)
	{
		case CFG_PARAM_MODE:
			g_eeprom_data.cfg.mode = value[0];
			return 1;
		case CFG_PARAM_SERIAL:
			config_set_serial((char*)value);
			return 1;
	}

	return 0;
}


