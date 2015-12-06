/*	gc_n64_usb : Gamecube or N64 controller to USB firmware
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
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

#include "gcn64_protocol.h"
#include "gcn64txrx.h"

#undef FORCE_KEYBOARD
#undef TRACE_GCN64

/******** IO port definitions and options **************/
#ifndef STK525
	#define GCN64_DATA_PORT	PORTD
	#define GCN64_DATA_DDR	DDRD
	#define GCN64_DATA_PIN	PIND
	#define GCN64_DATA_BIT	(1<<0)
	#define GCN64_BIT_NUM_S	"0" // for asm
#else
	#define GCN64_DATA_PORT	PORTA
	#define GCN64_DATA_DDR	DDRA
	#define GCN64_DATA_PIN	PINA
	#define GCN64_DATA_BIT	(1<<0)
	#define GCN64_BIT_NUM_S	"0" // for asm
#endif

#define DISABLE_INTS_DURING_COMM

void gcn64protocol_hwinit(void)
{
	// data as input
	GCN64_DATA_DDR &= ~(GCN64_DATA_BIT);

	// keep data low. By toggling the direction, we make the
	// pin act as an open-drain output.
	GCN64_DATA_PORT &= ~GCN64_DATA_BIT;

	/* debug bit PORTB4 (MISO) */
	DDRB |= 0x10;
	PORTB &= ~0x10;
}

/**
 * \brief Send n data bytes + stop bit, wait for answer.
 * \return The number of bytes received, 0 on timeout/error.
 *
 * The result is in gcn64_workbuf.
 */
unsigned char gcn64_transaction(const unsigned char *tx, int tx_len, unsigned char *rx, unsigned char rx_max)
{
	int count;
	unsigned char sreg = SREG;

#ifdef TRACE_GCN64
	int i;

	printf("TX[%d] = { ", tx_len);
	for (i=0; i<tx_len; i++) {
		printf("%02x ", tx[i]);
	}
	printf("}\r\n");
#endif

#ifdef DISABLE_INTS_DURING_COMM
	cli();
#endif
	gcn64_sendBytes(tx, tx_len);
	count = gcn64_receiveBytes(rx, rx_max);
	SREG = sreg;

	if (count == 0xff) {
		printf("rx error\n");
		return 0;
	}

#ifdef TRACE_GCN64
	printf("RX[%d] = { ", count);
	for (i=0; i<count; i++) {
		printf("%02x ", rx[i]);
	}
	printf("}\r\n");
#endif

	/* this delay is required on N64 controllers. Otherwise, after sending
	 * a rumble-on or rumble-off command (probably init too), the following
	 * get status fails. This starts to work at 30us. 60us should be safe. */
	_delay_us(80); // Note that this results in a ~100us delay between packets.

	return count;
}


#if (GC_GETID != 	N64_GET_CAPABILITIES)
#error N64 vs GC detection commnad broken
#endif
int gcn64_detectController(void)
{
	unsigned char tmp = GC_GETID;
	unsigned char count;
	unsigned short id;
	unsigned char data[4];

	count = gcn64_transaction(&tmp, 1, data, sizeof(data));
	if (count == 0) {
		return CONTROLLER_IS_ABSENT;
	}
	if (count != GC_GETID_REPLY_LENGTH) {
		return CONTROLLER_IS_UNKNOWN;
	}

	/*
	 * -- Standard gamecube controller answer:
	 * 0000 1001 0000 0000 0010 0011  : 0x090023  or
	 * 0000 1001 0000 0000 0010 0000  : 0x090020
	 *
	 * 0000 1001 0000 0000 0010 0000
	 *
	 * -- Wavebird gamecube controller
	 * 1010 1000 0000 0000 0000 0000 : 0xA80000
	 * (receiver first power up, controller off)
	 *
	 * 1110 1001 1010 0000 0001 0111 : 0xE9A017
	 * (controller on)
	 *
	 * 1010 1000 0000
	 *
	 * -- Intec wireless gamecube controller
	 * 0000 1001 0000 0000 0010 0000 : 0x090020
	 *
	 *
	 * -- Standard N64 controller
	 * 0000 0101 0000 0000 0000 0000 : 0x050000 (no pack)
	 * 0000 0101 0000 0000 0000 0001 : 0x050001 With expansion pack
	 * 0000 0101 0000 0000 0000 0010 : 0x050002 Expansion pack removed
	 *
	 * -- Ascii keyboard (keyboard connector)
	 * 0000 1000 0010 0000 0000 0000 : 0x082000
	 *
	 * Ok, so based on the above, when the second nibble is a 9 or 8, a
	 * gamecube compatible controller is present. If on the other hand
	 * we have a 5, then we are communicating with a N64 controller.
	 *
	 * This conclusion appears to be corroborated by my old printout of
	 * the document named "Yet another gamecube documentation (but one
	 * that's worth printing). The document explains that and ID can
	 * be read by sending what they call the 'SI command 0x00 to
	 * which the controller replies with 3 bytes. (Clearly, that's
	 * what we are doing here). The first 16 bits are the id, and they
	 * list, among other type of devices, the following:
	 *
	 * 0x0500 N64 controller
	 * 0x0900 GC standard controller
	 * 0x0900 Dkongas
	 * 0xe960 Wavebird
	 * 0xe9a0 Wavebird
	 * 0xa800 Wavebird
	 * 0xebb0 Wavebird
	 *
	 **/

	id = (data[0]<<8) | data[1];

	printf("Id: %04x   (%d: %02x %02x %02x)\r\n", id, count, data[0], data[1], data[2]);

#ifdef FORCE_KEYBOARD
	return CONTROLLER_IS_GC_KEYBOARD;
#endif

	switch ((id >> 8) & 0x0F) {
		case 0x05:
			return CONTROLLER_IS_N64;

		case 0x09: // normal controllers
		case 0x0b: // Never saw this one, but it is mentionned above.
			return CONTROLLER_IS_GC;

		case 0x08:
			if (id == 0x0820) {
				// Ascii keyboard
				return CONTROLLER_IS_GC_KEYBOARD;
			}
			// wavebird, controller off.
			return CONTROLLER_IS_GC;

		default:
			return CONTROLLER_IS_UNKNOWN;
	}

	return 0;
}
