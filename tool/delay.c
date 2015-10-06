#ifdef LIBDRAGON
#include <libdragon.h>

void _delay_us(unsigned long us)
{
	wait_ms(us/1000);
}

void _delay_s(unsigned long s)
{
	wait_ms(s*1000);
}

#else
#include <unistd.h>

void _delay_us(unsigned long us)
{
	usleep(us);
}

void _delay_s(unsigned long s)
{
	sleep(s);
}

#endif
