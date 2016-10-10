#ifndef _hiddata_h__
#define _hiddata_h__

#include <stdint.h>
#include "usb.h"

struct hiddata_ops {
	void (*suspendPolling)(uint8_t suspend);
	void (*forceVibration)(uint8_t channel, uint8_t force);
};

uint16_t hiddata_get_report(void *ctx, struct usb_request *rq, const uint8_t **dat);
uint8_t hiddata_set_report(void *ctx, const struct usb_request *rq, const uint8_t *dat, uint16_t len);

void hiddata_doTask(struct hiddata_ops *ops);

#endif
