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

#undef FORCE_KEYBOARD

#define GCN64_BUF_SIZE	600
static volatile unsigned char gcn64_workbuf[GCN64_BUF_SIZE];

/******** IO port definitions and options **************/
#ifndef STK525
	#define GCN64_DATA_PORT	PORTD
	#define GCN64_DATA_DDR	DDRD
	#define GCN64_DATA_PIN	PIND
	#define GCN64_DATA_BIT	(1<<0)
	#define GCN64_BIT_NUM_S	"0" // for asm
	#define FREQ_IS_16MHZ
	#define DISABLE_INTS_DURING_COMM
#else
	#define GCN64_DATA_PORT	PORTA
	#define GCN64_DATA_DDR	DDRA
	#define GCN64_DATA_PIN	PINA
	#define GCN64_DATA_BIT	(1<<0)
	#define GCN64_BIT_NUM_S	"0" // for asm
	#define FREQ_IS_16MHZ
	#define DISABLE_INTS_DURING_COMM
#endif

/*
 * \brief Explode bytes to bits
 * \param bytes 	The input byte array
 * \param num_bytes	The number of input bytes
 * \param workbuf_bit_offset	The offset to start writing at
 * \return number of bits (i.e. written output bytes)
 *
 * 1 input byte = 8 output bytes, where each output byte is zero or non-zero depending
 * on the input byte bits, starting with the most significant one.
 */
static int bitsToWorkbufBytes(unsigned char *bytes, int num_bytes, int workbuf_bit_offset)
{
	int i, bit;
	unsigned char p;

	if (num_bytes * 8 > GCN64_BUF_SIZE)
		return 0;

	for (i=0,bit=0; i<num_bytes; i++) {
		for (p=0x80; p; p>>=1) {
			gcn64_workbuf[bit+workbuf_bit_offset] = bytes[i] & p;
			bit++;
		}
	}

	return bit;
}

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

// The bit timeout is a counter to 127. This is the 
// start value. Counting from 0 takes hundreads of 
// microseconds. Because of this, the reception function
// "hangs in there" much longer than necessary..
#ifdef FREQ_IS_16MHZ
#define TIMING_OFFSET	75
#else
#define TIMING_OFFSET	100 // gives about 12uS. Twice the expected maximum bit period.
#endif

static unsigned int gcn64_receive()
{
	register unsigned int count=0;

#define SET_DBG	"	nop\n"
#define CLR_DBG	"	nop\n"
//#define SET_DBG	"	sbi %3, 4		\n"
//#define CLR_DBG	"	cbi %3, 4		\n"

	// The data line has been released. 
	// The receive part below expects it to be still high
	// and will wait for it to become low before beginning
	// the counting.
	asm volatile(
		"	push r30				\n"	// save Z
		"	push r31				\n"	// save Z
		"	clr r27					\n" // clear X (%0)
		"	clr r26					\n"

		"	clr r16					\n"
"initial_wait_low:\n"
		"	inc r16					\n"
		"	breq timeout			\n" // overflow to 0
		"	sbic %2, "GCN64_BIT_NUM_S"		\n"
		"	rjmp initial_wait_low	\n"

		// the next transition is to a high bit	
		"	rjmp waithigh			\n"

"waitlow:\n"
		"	ldi r16, %4				\n"
"waitlow_lp:\n"
		"	inc r16					\n"
		"	brmi timeout			\n" // > 127 (approx 50uS timeout)
		"	sbic %2, "GCN64_BIT_NUM_S"			\n"
		"	rjmp waitlow_lp			\n"

		"	adiw %0, 1					\n" // count this timed low level
		"	breq overflow			\n" // > 255
		"	st z+,r16				\n"

"waithigh:\n"
		"	ldi r16, %4				\n"
"waithigh_lp:\n"
		"	inc r16					\n"
		"	brmi timeout			\n" // > 127
		"	sbis %2, "GCN64_BIT_NUM_S"		\n"
		"	rjmp waithigh_lp		\n"
		"	adiw %0, 1				\n" // count this timed high level

		"	breq overflow			\n" // > 255
		"	st z+,r16				\n"

		"	rjmp waitlow			\n"

"overflow:  \n"
"timeout:	\n"
"			pop r31				\n" // restore z
"			pop r30				\n" // restore z

		: 	"=&x" (count)						// %0
		: 	"z" ((unsigned char volatile *)gcn64_workbuf),		// %1
			"I" (_SFR_IO_ADDR(GCN64_DATA_PIN)),	// %2
			"I" (_SFR_IO_ADDR(PORTB)),			// %3
			"M" (TIMING_OFFSET)					// %4
		: 	"r16","memory"
	);

	return count;
}

static void gcn64_sendBytes(unsigned char *data, unsigned char n_bytes)
{
	unsigned int bits;

	if (n_bytes == 0)
		return;

	// Explode the data to one byte per bit for very easy transmission in assembly.
	// This trades memory for ease of implementation.
	bits = bitsToWorkbufBytes(data, n_bytes, 0);

	// the value of the gpio is pre-configured to low. We simulate
	// an open drain output by toggling the direction.
#define PULL_DATA		"	sbi %0, "GCN64_BIT_NUM_S"\n"
#define RELEASE_DATA	"	cbi %0, "GCN64_BIT_NUM_S"\n"

#ifdef FREQ_IS_16MHZ
	// busy looping delays based on busy loop and nop tuning.
	// valid for 16Mhz clock. (Tuned to 1us/3us using a scope)
#define DLY_SHORT_1ST	"ldi r17, 2\n nop\nrcall sb_dly%=\n "
#define DLY_LARGE_1ST	"ldi r17, 13\n rcall sb_dly%=\n"
#define DLY_SHORT_2ND	"nop\nnop\nnop\nnop\n" 
#define DLY_LARGE_2ND	"ldi r17, 9\n rcall sb_dly%=\nnop\nnop\n"

#else
	// busy looping delays based on busy loop and nop tuning.
	// valid for 12Mhz clock.
#define DLY_SHORT_1ST	"ldi r17, 1\n rcall sb_dly%=\n "
#define DLY_LARGE_1ST	"ldi r17, 9\n rcall sb_dly%=\n"
#define DLY_SHORT_2ND	"\n" 
#define DLY_LARGE_2ND	"ldi r17, 5\n rcall sb_dly%=\n nop\nnop\n"
#endif

	asm volatile(
	// Save the modified input operands
	"	push r28			\n" // y
	"	push r29			\n"
	"	push r30			\n" // z
	"	push r31			\n"

	"sb_loop%=:				\n"
	"	ld r16, z+			\n"
	"	tst r16				\n"
	"	breq sb_send0%=		\n"
	"	brne sb_send1%=		\n"

	"	rjmp sb_end%=		\n" // not reached


	"sb_send0%=:			\n"
	"	nop					\n"
	PULL_DATA
	DLY_LARGE_1ST
	RELEASE_DATA
	DLY_SHORT_2ND
	"	sbiw	%1, 1		\n"
	"	brne sb_loop%=		\n"
	"	rjmp sb_end%=		\n"

	"sb_send1%=:			\n"
	PULL_DATA
	DLY_SHORT_1ST
	RELEASE_DATA
	DLY_LARGE_2ND
	"	sbiw	%1, 1		\n"
	"	brne sb_loop%=		\n"
	"	rjmp sb_end%=		\n"

// delay sub (arg r17)
	"sb_dly%=:				\n"
	"	dec r17				\n"
	"	brne sb_dly%=		\n"
	"	ret					\n"
	

	"sb_end%=:\n"
	// going here is fast so we need to extend the last
	// delay by 500nS
	"	nop\n "
#ifdef FREQ_IS_16MHZ
	"	nop\n"
#endif
	"	pop r31				\n"
	"	pop r30				\n"
	PULL_DATA
	"	pop r29				\n"
	"	pop r28				\n"
	//DLY_SHORT_1ST
	"nop\nnop\nnop\nnop\n"
	RELEASE_DATA

	// Now, we need to loop until the wire is high to 
	// prevent the reception code from thinking this is
	// the beginning of the first reply bit.

	"	ldi r16, 0xff		\n" // setup a timeout
	"sb_waitHigh%=:			\n"
	"	dec r16				\n" // decrement timeout
	"	breq sb_wait_high_done%=		\n" // handle timeout condition
	"	sbis %3, "GCN64_BIT_NUM_S"			\n" // Read the port
	"	rjmp sb_waitHigh%=	\n"
"sb_wait_high_done%=:\n"
	:
	: "I" (_SFR_IO_ADDR(GCN64_DATA_DDR)), // %0
	  "w" (bits),						// %1
	  "z" ((unsigned char volatile *)gcn64_workbuf),					// %2
	  "I" (_SFR_IO_ADDR(GCN64_DATA_PIN))	// %3
	: "r16", "r17");
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
	count = gcn64_receive();
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


