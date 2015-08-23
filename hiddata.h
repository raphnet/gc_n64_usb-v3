#ifndef _hiddata_h__
#define _hiddata_h__

#include <stdint.h>
#include "usb.h"

uint16_t hiddata_get_report(struct usb_request *rq, const uint8_t **dat);
uint8_t hiddata_set_report(const struct usb_request *rq, const uint8_t *dat, uint16_t len);

void hiddata_doTask(void);

#endif
