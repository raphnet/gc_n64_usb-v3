#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include <util/atomic.h>
#include "stkchk.h"

extern uint16_t __stack;
extern uint16_t _end;

/** Write a canary at the end of the stack. */
void stkchk_init(void)
{
	*((&_end)-1) = 0xDEAD;
}

/** Check if the canary is still alive.
 *
 * Call this perdiocally to check if the
 * stack grew too large.
 **/
char stkchk_verify(void)
{
	if (*((&_end)-1) != 0xDEAD) {
		return -1;
	}

	return 0;
}

/* In order to get an approximate idea of how
 * much stack you are using, this can be called
 * at strategic places where you know the
 * call stack is deep or where large automatic
 * buffers are used.
 */
#ifdef STKCHK_WITH_STATUS_CHECK
void stkchk(const char *fname)
{
	static int max_usage = 0;
	uint16_t end = ((uint16_t)&_end);
	uint16_t s_top = ((uint16_t)&__stack);
	uint16_t s_cur = SPL | SPH<<8;
	uint16_t used, s_size;
	uint8_t grew = 0;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		used = s_top - s_cur;
		s_size = s_top - end;
		if (used > max_usage) {
			max_usage = used;
			grew = 1;
		}
	}

	if (grew) {
		printf("[stkchk] %s: %d/%d\r\n", fname, used, s_size);
	}
}
#endif
