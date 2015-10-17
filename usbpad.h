#ifndef USBPAD_H__
#define USBPAD_H__

#include "usb.h"

void usbpad_init(void);
int usbpad_getReportSize(void);
unsigned char *usbpad_getReportBuffer(void);

void usbpad_update(const gamepad_data *pad_data);
char usbpad_mustVibrate(void);

uint8_t usbpad_hid_set_report(const struct usb_request *rq, const uint8_t *data, uint16_t len);
uint16_t usbpad_hid_get_report(struct usb_request *rq, const uint8_t **dat);

// For mappings. ID starts at 0.
#define USB_BTN(id)	(0x0001 << (id))

#endif // USBPAD_H__
