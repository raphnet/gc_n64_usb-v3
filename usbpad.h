#ifndef USBPAD_H__
#define USBPAD_H__

void usbpad_init(void);
int usbpad_getReportSize(void);
void usbpad_buildReport(const gamepad_data *pad_data, unsigned char *dstbuf);

#endif // USBPAD_H__
