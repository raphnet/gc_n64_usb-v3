#include "gamepads.h"

/* Shared between N64 and GC (only one is used at a time). Saves memory. */

gamepad_data last_sent_report;
gamepad_data last_built_report;
