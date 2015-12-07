/*	gc_n64_usb : Gamecube or N64 controller to USB firmware
	Copyright (C) 2007-2013  Raphael Assenat <raph@raphnet.net>

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
#include "usart1.h"

#ifdef UART1_STDOUT
static int uart1_putchar(char c, FILE *stream)
{
	usart1_send(&c, 1);
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart1_putchar, NULL,
                                           _FDEV_SETUP_WRITE);
#endif

void usart1_send(void *data, int len)
{
	while (!(UCSR1A & (1<<UDRE1)));
	UDR1 = *((unsigned char*)data);
	data++;
}

void usart1_init(void)
{
	UCSR1B = (1<<TXEN1);
	UCSR1C = (1<<UCSZ11) | (1<<UCSZ10);
#ifdef UCSR1D
	UCSR1D = 0;
#endif

	UBRR1H = 0;
#if F_CPU==8000000L
	UBRR1L = 8; // 57600 at 3.5% error at 8MHz
#elif F_CPU==16000000L
	UBRR1L = 16; // 57600 at 2.1% error at 16MHz
#else
	#error Unsupported clock
#endif

#ifdef UART1_STDOUT
	stdout = &mystdout;
#endif
}
