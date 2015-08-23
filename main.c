#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "util.h"
#include "usart1.h"
#include "usb.h"
#include "gamepads.h"
#include "bootloader.h"

#include "gcn64_protocol.h"
#include "n64.h"
#include "gamecube.h"
#include "usbpad.h"
#include "eeprom.h"
#include "hiddata.h"

uint16_t hid_get_report_main(struct usb_request *rq, const uint8_t **dat);
uint8_t hid_set_report_main(const struct usb_request *rq, const uint8_t *dat, uint16_t len);

uint16_t hid_get_report_data(struct usb_request *rq, const uint8_t **dat);
uint8_t hid_set_report_data(const struct usb_request *rq, const uint8_t *dat, uint16_t len);

#include "reportdesc.c"
#include "dataHidReport.c"

const wchar_t *const g_usb_strings[] = {
	[0] = L"raphnet technologies", 	// 1 : Vendor
	[1] = L"GC/N64 to USB v3.0",	// 2: Product
	[2] = L"123456",				// 3 : Serial
};

struct cfg0 {
	struct usb_configuration_descriptor configdesc;
	struct usb_interface_descriptor interface;
	struct usb_hid_descriptor hid;
	struct usb_endpoint_descriptor ep1_in;

	struct usb_interface_descriptor interface_admin;
	struct usb_hid_descriptor hid_data;
	struct usb_endpoint_descriptor ep2_in;
};

static const struct cfg0 cfg0 PROGMEM = {
	.configdesc = {
		.bLength = sizeof(struct usb_configuration_descriptor),
		.bDescriptorType = CONFIGURATION_DESCRIPTOR,
		.wTotalLength = sizeof(cfg0), // includes all descriptors returned together
		.bNumInterfaces = 2,
		.bConfigurationValue = 1,
		.bmAttributes = CFG_DESC_ATTR_RESERVED, // set Self-powred and remote-wakeup here if needed.
		.bMaxPower = 25, // for 50mA
	},

	// Main interface, HID
	.interface = {
		.bLength = sizeof(struct usb_interface_descriptor),
		.bDescriptorType = INTERFACE_DESCRIPTOR,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 1,
		.bInterfaceClass = USB_DEVICE_CLASS_HID,
		.bInterfaceSubClass = HID_SUBCLASS_NONE,
		.bInterfaceProtocol = HID_PROTOCOL_NONE,
	},
	.hid = {
		.bLength = sizeof(struct usb_hid_descriptor),
		.bDescriptorType = HID_DESCRIPTOR,
		.bcdHid = 0x0101,
		.bCountryCode = HID_COUNTRY_NOT_SUPPORTED,
		.bNumDescriptors = 1, // Only a report descriptor
		.bClassDescriptorType = REPORT_DESCRIPTOR,
		.wClassDescriptorLength = sizeof(gcn64_usbHidReportDescriptor),
	},
	.ep1_in = {
		.bLength = sizeof(struct usb_endpoint_descriptor),
		.bDescriptorType = ENDPOINT_DESCRIPTOR,
		.bEndpointAddress = USB_RQT_DEVICE_TO_HOST | 1, // 0x81
		.bmAttributes = TRANSFER_TYPE_INT,
		.wMaxPacketsize = 64,
		.bInterval = LS_FS_INTERVAL_MS(1),
	},

	// Second HID interface for config and update
	.interface_admin = {
		.bLength = sizeof(struct usb_interface_descriptor),
		.bDescriptorType = INTERFACE_DESCRIPTOR,
		.bInterfaceNumber = 1,
		.bAlternateSetting = 0,
		.bNumEndpoints = 1,
		.bInterfaceClass = USB_DEVICE_CLASS_HID,
		.bInterfaceSubClass = HID_SUBCLASS_NONE,
		.bInterfaceProtocol = HID_PROTOCOL_NONE,
	},
	.hid_data = {
		.bLength = sizeof(struct usb_hid_descriptor),
		.bDescriptorType = HID_DESCRIPTOR,
		.bcdHid = 0x0101,
		.bCountryCode = HID_COUNTRY_NOT_SUPPORTED,
		.bNumDescriptors = 1, // Only a report descriptor
		.bClassDescriptorType = REPORT_DESCRIPTOR,
		.wClassDescriptorLength = sizeof(dataHidReport),
	},
	.ep2_in = {
		.bLength = sizeof(struct usb_endpoint_descriptor),
		.bDescriptorType = ENDPOINT_DESCRIPTOR,
		.bEndpointAddress = USB_RQT_DEVICE_TO_HOST | 2, // 0x82
		.bmAttributes = TRANSFER_TYPE_INT,
		.wMaxPacketsize = 64,
		.bInterval = LS_FS_INTERVAL_MS(1),
	},

};

const struct usb_device_descriptor device_descriptor PROGMEM = {
	.bLength = sizeof(struct usb_device_descriptor),
	.bDescriptorType = DEVICE_DESCRIPTOR,
	.bcdUSB = 0x0101,
	.bDeviceClass = 0, // set at interface
	.bDeviceSubClass = 0, // set at interface
	.bDeviceProtocol = 0,
	.bMaxPacketSize = 64,
	.idVendor = 0x289B,
	.idProduct = 0x0017,
	.bcdDevice = 0x0300, // 1.0.0
	.bNumConfigurations = 1,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
};

/** **/

static struct usb_parameters usb_params = {
	.flags = USB_PARAM_FLAG_CONFDESC_PROGMEM |
				USB_PARAM_FLAG_DEVDESC_PROGMEM |
					USB_PARAM_FLAG_REPORTDESC_PROGMEM,
	.devdesc = (PGM_VOID_P)&device_descriptor,
	.configdesc = (PGM_VOID_P)&cfg0,
	.configdesc_ttllen = sizeof(cfg0),
	.num_strings = ARRAY_SIZE(g_usb_strings),
	.strings = g_usb_strings,

	.n_hid_interfaces = 2,
	.hid_params = {
		[0] = {
			.reportdesc = gcn64_usbHidReportDescriptor,
			.reportdesc_len = sizeof(gcn64_usbHidReportDescriptor),
			.getReport = hid_get_report_main,
			.setReport = hid_set_report_main,
		},
		[1] = {
			.reportdesc = dataHidReport,
			.reportdesc_len = sizeof(dataHidReport),
			.getReport = hiddata_get_report,
			.setReport = hiddata_set_report,
		},
	},
};

void hwinit(void)
{
	/* PORTB
	 *
	 * 7: NC    Output low
	 * 6: NC	Output low
	 * 5: NC	Output low
	 * 4: NC	Output low
	 * 3: MISO	Output low
	 * 2: MOSI	Output low
	 * 1: SCK	Output low
	 * 0: NC	Output low
	 */
	PORTB = 0x00;
	DDRB = 0xFF;

	/* PORTC
	 *
	 * 7: NC	Output low
	 * 6: NC	Output low
	 * 5: NC	Output low
	 * 4: NC	Output low
	 * 3: (no such pin)
	 * 2: NC	Output low
	 * 1: RESET	(N/A: Reset input per fuses)
	 * 0: XTAL2	(N/A: Crystal oscillator)
	 */
	PORTB = 0x00;
	DDRB = 0xff;

	/* PORTD
	 *
	 * 7: HWB		Input (external pull-up)
	 * 6: NC		Output low
	 * 5: NC		Output low
	 * 4: NC		Output low
	 * 3: IO3_MCU	Input
	 * 2: IO2_MCU	Input
	 * 1: IO1_MCU	Input
	 * 0: IO0_MCU	Input
	 */
	PORTD = 0x00;
	DDRD = 0x70;

	// System clock. External crystal is 16 Mhz and we want
	// to run at max. speed.
	CLKPR = 0x80;
	CLKPR = 0x0; // Division factor of 1
	PRR0 = 0;
	PRR1 = 0;
}

static unsigned char hid_report_data[32];
static unsigned char gamepad_report0[32];

// Output Report IDs for various functions
#define REPORT_SET_EFFECT			0x01
#define REPORT_SET_STATUS			0x02
#define	REPORT_SET_PERIODIC			0x04
#define REPORT_SET_CONSTANT_FORCE	0x05
#define REPORT_EFFECT_OPERATION		0x0A
#define REPORT_EFFECT_BLOCK_IDX		0x0B
#define REPORT_DISABLE_ACTUATORS	0x0C
#define REPORT_PID_POOL				0x0D

// Feature reports
#define REPORT_CREATE_EFFECT		0x09

// For the 'Usage Effect Operation' report
#define EFFECT_OP_START			1
#define EFFECT_OP_START_SOLO	2
#define EFFECT_OP_STOP			3

// Feature report
#define PID_SIMULTANEOUS_MAX	3
#define PID_BLOCK_LOAD_REPORT	2

static volatile unsigned char gamepad_vibrate = 0; // output

static unsigned char vibration_on = 0;
static unsigned char constant_force = 0;
static unsigned char magnitude = 0;

static unsigned char _FFB_effect_index;
#define LOOP_MAX    0xFFFF
static unsigned int _loop_count;

static void decideVibration(void)
{
	if (!vibration_on) {
		gamepad_vibrate = 0;
	} else {
		if (constant_force > 0x7f) {
			gamepad_vibrate = 1;
		}
		if (magnitude > 0x7f) {
			gamepad_vibrate = 1;
		}
	}
}

uint16_t hid_get_report_main(struct usb_request *rq, const uint8_t **dat)
{
	uint8_t report_id = (rq->wValue & 0xff);

	// USB HID 1.11 section 7.2.1 Get_Report
	// wValue high byte : report type
	// wValue low byte : report id
	// wIndex Interface
	switch (rq->wValue >> 8)
	{
		case HID_REPORT_TYPE_INPUT:
			{
				if (report_id == 1) { // Joystick
					// report_id = rq->wValue & 0xff
					// interface = rq->wIndex
					*dat = gamepad_report0;
					printf_P(PSTR("Get joy report\r\n"));
					return 9;
				} else if (report_id == 2) { // 2 : ES playing
					hid_report_data[0] = report_id;
					hid_report_data[1] = 0;
					hid_report_data[2] = _FFB_effect_index;
					printf_P(PSTR("ES playing\r\n"));
					*dat = hid_report_data;
					return 3;
				} else {
					printf_P(PSTR("Get input report %d ??\r\n"), rq->wValue & 0xff);
				}
			}
			break;

		case HID_REPORT_TYPE_FEATURE:
			if (report_id == PID_BLOCK_LOAD_REPORT) {
				hid_report_data[0] = report_id;
				hid_report_data[1] = 0x1; // Effect block index
				hid_report_data[2] = 0x1; // (1: success, 2: oom, 3: load error)
				hid_report_data[3] = 10;
				hid_report_data[4] = 10;
				printf_P(PSTR("block load\r\n"));
				*dat = hid_report_data;
				return 5;
			}
			else if (report_id == PID_SIMULTANEOUS_MAX) {
				hid_report_data[0] = report_id;
				// ROM Effect Block count
				hid_report_data[1] = 0x1;
				hid_report_data[2] = 0x1;
				// PID pool move report?
				hid_report_data[3] = 0xff;
				hid_report_data[4] = 1;
				printf_P(PSTR("simultaneous max\r\n"));
				*dat = hid_report_data;
				return 5;
			}
			else if (report_id == REPORT_CREATE_EFFECT) {
				hid_report_data[0] = report_id;
				hid_report_data[1] = 1;
				printf_P(PSTR("create effect\r\n"));
				*dat = hid_report_data;
				return 2;
			} else {
				printf_P(PSTR("Unknown feature %d\r\n"), rq->wValue & 0xff);
			}
			break;
	}

	printf_P(PSTR("Unhandled hid get report type=0x%02x, rq=0x%02x, wVal=0x%04x, wLen=0x%04x\r\n"), rq->bmRequestType, rq->bRequest, rq->wValue, rq->wLength);
	return 0;
}

uint8_t hid_set_report_main(const struct usb_request *rq, const uint8_t *data, uint16_t len)
{
	if (len < 1) {
		printf_P(PSTR("shrt\n"));
		return -1;
	}

	if ((rq->wValue >> 8) == HID_REPORT_TYPE_OUTPUT) {

		switch(data[0])
		{
			case REPORT_SET_STATUS:
				printf_P(PSTR("eff. set stat 0x%02x 0x%02x\r\n"),data[1],data[2]);
				break;
			case REPORT_EFFECT_BLOCK_IDX:
				printf_P(PSTR("eff. blk. idx %d\r\n"), data[1]);
				break;
			case REPORT_DISABLE_ACTUATORS:
				printf_P(PSTR("disable actuators\r\n"));
				break;
			case REPORT_PID_POOL:
				printf_P(PSTR("pid pool\r\n"));
				break;
			case REPORT_SET_EFFECT:
				_FFB_effect_index = data[1];
				printf_P(PSTR("set effect %d\n"), data[1]);
				break;
			case REPORT_SET_PERIODIC:
				magnitude = data[2];
			//	decideVibration();
				printf_P(PSTR("periodic mag: %d"), data[2]);
				break;
			case REPORT_SET_CONSTANT_FORCE:
				if (data[1] == 1) {
					constant_force = data[2];
					decideVibration();
					printf_P(PSTR("Constant force"));
				}
				break;
			case REPORT_EFFECT_OPERATION:
				if (len != 4)
					return -1;
				/* Byte 0 : report ID
				 * Byte 1 : bit 7=rom flag, bits 6-0=effect block index
				 * Byte 2 : Effect operation
				 * Byte 3 : Loop count */
				_loop_count = data[3]<<3;

				printf_P(PSTR("EFFECT OP: rom=%s, idx=0x%02x"), data[1] & 0x80 ? "Yes":"No", data[1] & 0x7F);

				switch(data[1] & 0x7F) // Effect block index
				{
					case 1: // constant force
					case 3: // square
					case 4: // sine
						switch (data[2]) // effect operation
						{
							case EFFECT_OP_START:
								printf_P(PSTR("Start\r\n"));
								vibration_on = 1;
								decideVibration();
								break;

							case EFFECT_OP_START_SOLO:
								printf_P(PSTR("Start solo\r\n"));
								vibration_on = 1;
								decideVibration();
								break;

							case EFFECT_OP_STOP:
								printf_P(PSTR("Stop\r\n"));
								vibration_on = 0;
								decideVibration();
								break;
						}
						break;

					// TODO : should probably drop these from the descriptor since they are

					case 2: // ramp
					case 5: // triangle
					case 6: // sawtooth up
					case 7: // sawtooth down
					case 8: // spring
					case 9: // damper
					case 10: // inertia
					case 11: // friction
					case 12: // custom force data
						printf_P(PSTR("Ununsed effect %d\n"), data[1] & 0x7F);
						break;
				}
				break;
			default:
				printf_P(PSTR("Set output report 0x%02x\r\n"), data[0]);
		}
	}
	else if ((rq->wValue >> 8) == HID_REPORT_TYPE_FEATURE) {
		switch(data[0])
		{
			case REPORT_CREATE_EFFECT:
				_FFB_effect_index = data[1];
				printf_P(PSTR("create effect %d\n"), data[1]);
				break;

			default:
				printf_P(PSTR("What?\n"));
		}
	}
	else {
		printf_P(PSTR("impossible\n"));
	}
	return 0;
}



#define NUM_PAD_TYPES	2

Gamepad *detectPad(void)
{
	char type;

	type = gcn64_detectController();

	switch (type)
	{
		case CONTROLLER_IS_ABSENT:
		case CONTROLLER_IS_UNKNOWN:
			return NULL;

		case CONTROLLER_IS_N64:
			return n64GetGamepad();
			break;

		case CONTROLLER_IS_GC:
			return gamecubeGetGamepad();
	}

	return NULL;
}

void eeprom_app_ready(void)
{
	// TODO : Set serial number from configured value
}

char g_polling_suspended = 0;

int main(void)
{
	Gamepad *pad = NULL;
	gamepad_data pad_data;

	hwinit();
	usart1_init();
	eeprom_init();

	/* Init the buffer with idle data */
	usbpad_buildReport(NULL, gamepad_report0);

	sei();
	usb_init(&usb_params);

	while (1)
	{
		static char last_v = 0;

		/* Try to auto-detect controller if none*/
		if (!pad) {
			pad = detectPad();
		}

		usb_doTasks();

		// Todo : The _delay_ms used to poll the controller at
		// a fixed interval is slowing down the frequency of
		// hiddata_doTask calls. Rework this code to use a timer
		// for polling the controller...
		hiddata_doTask();

		_delay_ms(5);
		decideVibration();

		if (last_v != gamepad_vibrate) {
			if (pad && pad->setVibration) {
				pad->setVibration(gamepad_vibrate);
			}
			last_v = gamepad_vibrate;
		}

		if (pad && !g_polling_suspended) {
			pad->update();

			if (pad->changed()) {
				int report_size;

				pad->getReport(&pad_data);
				usbpad_buildReport(&pad_data, gamepad_report0);
				report_size = usbpad_getReportSize();
				usb_interruptSend(gamepad_report0, report_size);
			}
		} else {
			/* Just make sure gamepad_report0 holds valid and
			 * inactive data for the HID GET_REPORT request */
			usbpad_buildReport(NULL, gamepad_report0);
		}
	}

	return 0;
}
