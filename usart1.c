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
