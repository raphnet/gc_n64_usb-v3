/*	gc_n64_usb : Gamecube or N64 controller to USB adapter firmware
	Copyright (C) 2007-2015  Raphael Assenat <raph@raphnet.net>

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
#include <avr/eeprom.h>
#include <util/crc16.h>
#include <stdint.h>
#include <string.h>
#include "eeprom.h"

static uint16_t calc_geeprom_data_crc(void)
{
	uint16_t crc = 0x0000;
	int i;

	/* Update the CRC */
	for (i=0; i<EEPROM_USED_SIZE_NOCRC; i++) {
		crc = _crc_xmodem_update(crc, ((uint8_t*)&g_eeprom_data)[i]);
	}

	return crc;
}

void eeprom_commit(void)
{
	g_eeprom_data.crc16 = calc_geeprom_data_crc();

	/* Sync eeprom content */
	eeprom_update_block(&g_eeprom_data, EEPROM_BASE_PTR, EEPROM_USED_SIZE);
}

static char isCrcValid()
{
	return g_eeprom_data.crc16 == calc_geeprom_data_crc();
}

// return 1 if eeprom was blank
void eeprom_init(void)
{
	eeprom_read_block(&g_eeprom_data, EEPROM_BASE_PTR, EEPROM_USED_SIZE);

	/* Detect new or corrupted content. Program default values if required. */
	if ((g_eeprom_data.magic != EEPROM_MAGIC) || !isCrcValid())
	{
		memset(&g_eeprom_data, 0, EEPROM_USED_SIZE);
		g_eeprom_data.magic = EEPROM_MAGIC;

		// Call application code to set application defaults
		eeprom_app_write_defaults();

		// Write the now valid content to the EEPROM at once.
		eeprom_commit();
	}

	eeprom_app_ready();
}
