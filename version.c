#include <avr/pgmspace.h>
#include "version.h"

const char *g_version = VERSIONSTR; // From Makefile
const char signature[] PROGMEM = "9c3ea8b8-753f-11e5-a0dc-001bfca3c593";
