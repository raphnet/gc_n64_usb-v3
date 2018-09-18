#ifndef USBPAD_H__
#define USBPAD_H__

#include "gamepads.h"
#include "usb.h"

#define USBPAD_REPORT_SIZE	15

struct usbpad {
	volatile unsigned char gamepad_vibrate; // output
	unsigned char vibration_on, force_vibrate;
	unsigned char constant_force;
	unsigned char periodic_magnitude;

	unsigned short _FFB_effect_duration; // in milliseconds
	unsigned char _FFB_effect_index;
#define LOOP_MAX    0xFFFF
	unsigned int _loop_count;

	unsigned char gamepad_report0[USBPAD_REPORT_SIZE];
	unsigned char hid_report_data[8]; // Used for force feedback
};

void usbpad_init(struct usbpad *pad);
int usbpad_getReportSize(void);
unsigned char *usbpad_getReportBuffer(struct usbpad *pad);

void usbpad_update(struct usbpad *pad, const gamepad_data *pad_data);
void usbpad_vibrationTask(struct usbpad *pad);
char usbpad_mustVibrate(struct usbpad *pad);
void usbpad_forceVibrate(struct usbpad *pad, char force);

uint8_t usbpad_hid_set_report(struct usbpad *pad, const struct usb_request *rq, const uint8_t *data, uint16_t len);
uint16_t usbpad_hid_get_report(struct usbpad *pad, struct usb_request *rq, const uint8_t **dat);

// For mappings. ID starts at 0.
#define USB_BTN(id)	(0x0001 << (id))

#endif // USBPAD_H__
