#ifndef USBPAD_H__
#define USBPAD_H__

void usbpad_init(void);
int usbpad_getReportSize(void);
void usbpad_buildReport(const gamepad_data *pad_data, unsigned char *dstbuf);

// For mappings. ID starts at 0.
#define USB_BTN(id)	(0x0001 << (id))

#endif // USBPAD_H__
