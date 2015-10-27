#include <avr/pgmspace.h>
#include "version.h"


const char *g_version = VERSIONSTR; // From Makefile
#ifdef STK525
const char g_signature[] PROGMEM = "e106420a-7c54-11e5-ae9a-001bfca3c593";
#else
const char g_signature[] PROGMEM = "9c3ea8b8-753f-11e5-a0dc-001bfca3c593";
#endif
