#ifndef _eeprom_h__
#define _eeprom_h__

#include <stdint.h>

#define EEPROM_MAGIC	0xfeed
#define EEPROM_BASE_PTR	((void*)0x0000)
#define EEPROM_USED_SIZE	(sizeof(struct eeprom_data_struct))
#define EEPROM_USED_SIZE_NOCRC	(EEPROM_USED_SIZE-2)

#include "config.h" // config.h to struct eeprom_cfg

struct eeprom_data_struct {
	uint16_t magic;
	struct eeprom_cfg cfg;
	uint16_t crc16;
};

extern struct eeprom_data_struct g_eeprom_data;

/* Application specific function to implement. When
 * called, write application defaults to g_eeprom_data.
 *
 * Only called when eeprom is new or corrupted. */
extern void eeprom_app_write_defaults(void);

/* Application specific function to implement. Called
 * after the eeprom content has been loaded or initialized.
 * A good place to copy values elsewhere or otherwise
 * act on configuration content. */
extern void eeprom_app_ready(void);

/* Load, Validate and init eeprom if needed. */
void eeprom_init(void);

/* Commit changes made to g_eeprom_data */
void eeprom_commit(void);

#endif // _eeprom_h__

