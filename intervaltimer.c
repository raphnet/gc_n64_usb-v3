#include <avr/io.h>
#include "intervaltimer.h"


void intervaltimer_init(void)
{
	TCCR1A = 0;
	TCCR1B = (1<<WGM12) | (1<<CS12) | (1<<CS00); // CTC, /1024 prescaler
}

void intervaltimer_set(int interval_ms)
{
	static int cur_interval = 0;

	// We are setting TCNT back to zero, and updating
	// the compare match register.
	//
	// To allows for simple repeated calling of this
	// function from the main loop, only touch
	// the counter when the interval changes.
	if (cur_interval != interval_ms) {
		cur_interval = interval_ms;

		TCNT1 = 0;
		OCR1A = interval_ms * (F_CPU/1024) / 1000;
	}
}

char intervaltimer_get(void)
{
	char a;

	if (TIFR1 & (1<<OCF1A)) {
		TIFR1 = 1<<OCF1A;
		return 1;
	}

	return 0;
}
