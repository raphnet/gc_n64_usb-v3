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
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "gcn64_protocol.h"
#include "gcn64txrx.h"

#undef FORCE_KEYBOARD

#define GCN64_BUF_SIZE	600
static unsigned char gcn64_workbuf[GCN64_BUF_SIZE];

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


/* Read a byte from the buffer (where 1 byte is 1 bit).
 * MSb first.
 */
unsigned char gcn64_protocol_getByte(int offset)
{
	unsigned char val, b;
	unsigned char volatile *addr = gcn64_workbuf + offset;

	for (b=0x80, val=0; b; b>>=1)
	{
		if (*addr)
			val |= b;
		addr++;
	}
	return val;
}

void gcn64_protocol_getBytes(int offset, int n_bytes, unsigned char *dstbuf)
{
	int i;

	for (i=0; i<n_bytes; i++) {
		*dstbuf = gcn64_protocol_getByte(offset + (i*8));
		dstbuf++;
	}
}

/* \brief Decode the received length of low/high states to byte-per-bit format
 *
 * The result is in workbuf.
 *
 **/
static void gcn64_decodeWorkbuf(unsigned int count)
{
	unsigned int i;
	volatile unsigned char *output = gcn64_workbuf;
	volatile unsigned char *input = gcn64_workbuf;
	unsigned char t;

    //  
    //          ________
    // ________/
    //  
    //   [i*2]    [i*2+1]
    //  
    //          ________________
    // 0 : ____/
    //                      ____
    // 1 : ________________/
    //  
    // The timings on a real N64 are
    //  
    // 0 : 1 us low, 3 us high
    // 1 : 3 us low, 1 us high
    //  
    // However, HORI pads use something similar to
    //  
    // 0 : 1.5 us low, 4.5 us high
    // 1 : 4.5 us low, 1.5 us high
    //  
    //  
    // No64 us = microseconds

	// This operation takes approximately 100uS on 64bit gamecube messages
	for (i=0; i<count; i++) {
		t = *input; 
		input++;

		*output = t < *input;

		input++;
		output++;
	}
}

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
 * \return The number of bits received, 0 on timeout/error.
 *
 * The result is in gcn64_workbuf, where each byte represents
 * a bit.
 */
int gcn64_transaction(unsigned char *data_out, int data_out_len)
{
	int count;
	unsigned char sreg = SREG;

#ifdef DISABLE_INTS_DURING_COMM
	cli();
#endif
	gcn64_sendBytes(data_out, data_out_len);
	//count = gcn64_receive();
	count = gcn64_receiveBits(gcn64_workbuf, 0);
	SREG = sreg;

	if (!count)
		return 0;

	if (!(count & 0x01)) {
		// If we don't get an odd number of level lengths from gcn64_receive
		// something is wrong. 
		//
		// The stop bit is a short (~1us) low state followed by an "infinite"
		// high state, which timeouts and lets the function return. This
		// is why we should receive and odd number of lengths.
		return 0;
	}

	gcn64_decodeWorkbuf(count);
	
	/* this delay is required on N64 controllers. Otherwise, after sending
	 * a rumble-on or rumble-off command (probably init too), the following
	 * get status fails. This starts to work at 2us. 5 should be safe. */
	_delay_us(5);
	
	/* return the number of full bits received. */
	return (count-1) / 2;
}


#if (GC_GETID != 	N64_GET_CAPABILITIES)
#error N64 vs GC detection commnad broken
#endif
int gcn64_detectController(void)
{
	unsigned char tmp = GC_GETID;
	int count;
	unsigned short id;

	count = gcn64_transaction(&tmp, 1);
	if (count == 0) {
		return CONTROLLER_IS_ABSENT;
	}
	if (count != 24) {
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
	 * This last entry worries me. I never observed it, but who knows
	 * what the user will connect? Better be safe and consider 0xb as
	 * a gamecube controller too.
	 *
	 * */

	id = gcn64_protocol_getByte(0)<<8;
	id |= gcn64_protocol_getByte(8);

#ifdef FORCE_KEYBOARD
	return CONTROLLER_IS_GC_KEYBOARD;
#endif

	switch (id >> 8) {
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
