#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "usb.h"

#define STATE_POWERED		0
#define STATE_DEFAULT		1
#define STATE_ADDRESS		2
#define STATE_CONFIGURED	3

static volatile uint8_t g_usb_suspend;
static uint8_t g_ep0_buf[64];
static uint8_t g_device_state = STATE_DEFAULT;
static uint8_t g_current_config;
static void *interrupt_data;
static volatile int interrupt_data_len = -1;

#define CONTROL_WRITE_BUFSIZE	64
static struct usb_request control_write_rq;
static volatile uint16_t control_write_len;
static volatile uint8_t control_write_in_progress;
static uint8_t control_write_buf[CONTROL_WRITE_BUFSIZE];

static const struct usb_parameters *g_params;

static void initControlWrite(const struct usb_request *rq)
{
	memcpy(&control_write_rq, rq, sizeof(struct usb_request));
	control_write_len = 0;
	control_write_in_progress = 1;
	printf_P(PSTR("Init cw\r\n"));
}

static int wcslen(const wchar_t *str)
{
	int i=0;
	while (*str) {
		str++;
		i++;
	}
	return i;
}

static void setupEndpoints(void)
{
	/*** EP0 ***/

	// Order from figure 23-2
	UENUM = 0x00;  // select endpoint
//	UERST |= 0x01; // reset endpoint
	UECONX = 1<<EPEN; // activate endpoint
	UECFG0X = 0; // Control OUT
	UEIENX = (1<<RXSTPE) | (1<<RXOUTE) | (1<<NAKINE); /* | (1<<STALLEDE) | (1<<NAKOUTE) | (1<<TXINE) | (1<<RXOUTE) */;
	UECFG1X |= (1<<EPSIZE0)|(1<<EPSIZE1)|(1<<ALLOC); // 64 bytes, one bank, and allocate
	UEINTX = 0;

	if (!(UESTA0X & (1<<CFGOK))) {
	//	printf_P("CFG EP0 fail\r\n");
		return;
	}
//	printf_P("ok\r\n");

	/*** EP1 ***/
	UENUM = 0x01;  // select endpoint

	UECONX = 1<<EPEN; // activate endpoint
	UECFG0X = (3<<6) | (1<<EPDIR); // Interrupt IN
	UEIENX = (1<<TXINE);
	UECFG1X |= (1<<EPSIZE0)|(1<<EPSIZE1)|(1<<ALLOC); // 64 bytes, one bank, and allocate
	UEINTX = 0;

	if (!(UESTA0X & (1<<CFGOK))) {
		printf_P(PSTR("CFG EP1 fail\r\n"));
		return;
	}


}

// Requires UENUM already set
static uint16_t readEP2buf(uint8_t *dst)
{
	uint16_t len;
	int i;
#ifdef UEBCHX
	len = UEBCLX | (UEBCHX << 8);
#else
	len = UEBCLX;
#endif

	for (i=0; i<len; i++) {
		*dst = UEDATX;
		dst++;
	}

	return len;
}

static void buf2EP(uint8_t epnum, const void *src, uint16_t len, uint16_t max_len, uint8_t progmem)
{
	int i;


	UENUM = epnum;  // select endpoint

	if (len > max_len) {
		len = max_len;
	}

	if (progmem) {
		const unsigned char *s = src;
		for (i=0; i<len; i++) {
			UEDATX = pgm_read_byte(s);
			s++;
		}
	} else {
		const unsigned char *s = src;
		for (i=0; i<len; i++) {
			UEDATX = *s;
			s++;
		}
	}
}


static void handleSetupPacket(struct usb_request *rq)
{
	char unhandled = 0;

//	printf_P(PSTR("t: %02x, rq: 0x%02x, val: %04x, l: %d\r\n"), rq->bmRequestType, rq->bRequest, rq->wValue, rq->wLength);

	if (USB_RQT_IS_HOST_TO_DEVICE(rq->bmRequestType))
	{
		switch (rq->bmRequestType & USB_RQT_RECIPIENT_MASK)
		{
			case USB_RQT_RECIPIENT_DEVICE:
				switch (rq->bRequest)
				{
					case USB_RQ_SET_ADDRESS:
						UDADDR = rq->wValue;
						while (!(UEINTX & (1<<TXINI)));
						UEINTX &= ~(1<<TXINI);
						while (!(UEINTX & (1<<TXINI)));
						UDADDR |= (1<<ADDEN);
						printf_P(PSTR("Addr: %d\r\n"), rq->wValue);
						if (!rq->wValue) {
							g_device_state = STATE_DEFAULT;
						} else {
							g_device_state = STATE_ADDRESS;
						}
						break;

					case USB_RQ_SET_CONFIGURATION:
						g_current_config = rq->wValue;
						if (!g_current_config) {
							g_device_state = STATE_ADDRESS;
						} else {
							g_device_state = STATE_CONFIGURED;
						}
						while (!(UEINTX & (1<<TXINI)));
						UEINTX &= ~(1<<TXINI);
						printf_P(PSTR("Configured: %d\r\n"), g_current_config);
						break;

					default:
						unhandled = 1;
				}
				break; // USB_RQT_RECIPIENT_DEVICE

			case USB_RQT_RECIPIENT_INTERFACE:
				switch(rq->bmRequestType & (USB_RQT_TYPE_MASK))
				{
					case USB_RQT_CLASS:
						switch(rq->bRequest)
						{
							case HID_CLSRQ_SET_IDLE:
								while (!(UEINTX & (1<<TXINI)));
								UEINTX &= ~(1<<TXINI);
								break;
							case HID_CLSRQ_SET_REPORT:
								while (!(UEINTX & (1<<TXINI)));
								UEINTX &= ~(1<<TXINI);
								initControlWrite(rq);
								break;
							default:
								printf_P(PSTR("Unhandled class bRequest 0x%02x\n"), rq->bRequest);
								unhandled = 1;
						}
						break;
					default:
						unhandled = 1;
				}

				break;

			case USB_RQT_RECIPIENT_ENDPOINT:
			case USB_RQT_RECIPIENT_OTHER:
			default:
				break;
		}
	}

	// Request where we send data to the host. Handlers
	// simply load the endpoint buffer and transmission
	// is handled automatically.
	if (USB_RQT_IS_DEVICE_TO_HOST(rq->bmRequestType))
	{
		switch (rq->bmRequestType & USB_RQT_RECIPIENT_MASK)
		{
			case USB_RQT_RECIPIENT_DEVICE:
				switch (rq->bRequest)
				{
					case USB_RQ_GET_STATUS:
						{
							unsigned char status[2] = { 0x00, 0x00 };
							// status[0] & 0x01 : Self powered
							// status[1] & 0x02 : Remote wakeup
							buf2EP(0, status, 2, rq->wLength, 0);
						}
						break;

					case USB_RQ_GET_CONFIGURATION:
						{
							if (g_device_state != STATE_CONFIGURED) {
								unsigned char zero = 0;
								buf2EP(0, &zero, 1, rq->wLength, 0);
							} else {
								buf2EP(0, &g_current_config, 1, rq->wLength, 0);
							}
						}
						break;

					case USB_RQ_GET_DESCRIPTOR:
						switch (rq->wValue >> 8)
						{
							case DEVICE_DESCRIPTOR:
								buf2EP(0, (unsigned char*)g_params->devdesc,
										sizeof(struct usb_device_descriptor), rq->wLength,
										g_params->flags & USB_PARAM_FLAG_DEVDESC_PROGMEM);
								break;
							case CONFIGURATION_DESCRIPTOR:
								// Check index if more than 1 config
								buf2EP(0, (unsigned char*)g_params->configdesc, g_params->configdesc_ttllen,
											rq->wLength, g_params->flags & USB_PARAM_FLAG_CONFDESC_PROGMEM);
								break;
							case STRING_DESCRIPTOR:
								{
									int id, len, slen;
									struct usb_string_descriptor_header hdr;

									id = (rq->wValue & 0xff);
									if (id > 0 && id <= g_params->num_strings)
									{
										id -= 1; // Our string table is zero-based

										len = rq->wLength;
										slen = wcslen(g_params->strings[id]) << 1;

										hdr.bLength = sizeof(hdr) + slen;
										hdr.bDescriptorType = STRING_DESCRIPTOR;

										buf2EP(0, (unsigned char*)&hdr, 2, len, 0);
										len -= 2;
										buf2EP(0, (unsigned char*)g_params->strings[id], slen, len, 0);
									}
									else if (id == 0) // Table of supported languages (string id 0)
									{
										unsigned char languages[4] = {
											4, STRING_DESCRIPTOR, 0x09, 0x10 // English (Canadian)
										};
										buf2EP(0, languages, 4, rq->wLength, 0);
									}
									else
									{
										printf_P(PSTR("Unknown string id\r\n"));
									}
								}
								break;

							case DEVICE_QUALIFIER_DESCRIPTOR:
								// Full speed devices must respond with a request error.
								unhandled = 1;
								break;


							default:
//								printf("Unhandled descriptor 0x%02x\n", rq->wValue>>8);
								unhandled = 1;
						}
						break;

					default:
						unhandled = 1;
				}
				break;

			case USB_RQT_RECIPIENT_INTERFACE:
				switch(rq->bmRequestType & (USB_RQT_TYPE_MASK))
				{
					case USB_RQT_STANDARD:
						switch (rq->bRequest)
						{
							case USB_RQ_GET_STATUS:
								{ // 9.4.5 Get Status, Figure 9-5. Reserved (0)
									unsigned char status[2] = { 0x00, 0x00 };
									buf2EP(0, status, 2, rq->wLength, 0);
								}
								break;

							case USB_RQ_GET_DESCRIPTOR:
								switch (rq->wValue >> 8)
								{
									case REPORT_DESCRIPTOR:
										{
											uint16_t rqlen = rq->wLength;
											uint16_t todo = rqlen;
											uint16_t pos = 0;
											unsigned char *reportdesc;

											// HID 1.1 : 7.1.1 Get_Descriptor request. wIndex is the interface number.
											//
											if (rq->wIndex > g_params->n_hid_interfaces)
												break;

											reportdesc = (unsigned char*)g_params->hid_params[rq->wIndex].reportdesc;
											if (rqlen > g_params->hid_params[rq->wIndex].reportdesc_len) {
//												rqlen = g_params->hid_params[rq->wIndex].reportdesc_len;
											};

											// TODO : Without the delays those two printf add, it
											// does not work. A handshake is missing.
											printf_P(PSTR("t: %02x, rq: 0x%02x, val: %04x, l: %d\r\n"), rq->bmRequestType, rq->bRequest, rq->wValue, rq->wLength);

											while(1)
											{
												printf_P(PSTR("pos %d todo %d\r\n"), pos, todo);
												if (todo > 64) {
													buf2EP(0, reportdesc+pos, 64,
															64,
															g_params->flags & USB_PARAM_FLAG_REPORTDESC_PROGMEM);
													UEINTX &= ~(1<<TXINI);
													pos += 64;
													todo -= 64;
												} else {
													buf2EP(0, reportdesc+pos, todo,
															todo,
															g_params->flags & USB_PARAM_FLAG_REPORTDESC_PROGMEM);
													UEINTX &= ~(1<<TXINI);
													break;
												}
											}
											while (1)
											{
												if (UEINTX & (1<<RXOUTI)) {
													UEINTX &= ~(1<<RXOUTI); // ACK
													return;
												}
											}

										}
										break;

									default:
										unhandled = 1;
								}
								break;

							default:
								unhandled = 1;
						}
						break;

					case USB_RQT_CLASS:
						switch (rq->bRequest)
						{
							case HID_CLSRQ_GET_REPORT:
								{
									// HID 1.1 : 7.2.1 Get_Report request. wIndex is the interface number.
									if (rq->wIndex > g_params->n_hid_interfaces)
										break;

									if (g_params->hid_params[rq->wIndex].getReport) {
										const unsigned char *data;
										uint16_t len;
										len = g_params->hid_params[rq->wIndex].getReport(rq, &data);
										if (len) {
											buf2EP(0, data, len, rq->wLength, 0);
										}
									} else {
										// Treat as not-supported (i.e. STALL endpoint)
										unhandled = 1;
									}
								}
								break;
							default:
								unhandled = 1;
						}
						break;

					default:
						unhandled = 1;
				}
				break;

			case USB_RQT_RECIPIENT_ENDPOINT:
				switch (rq->bRequest)
				{
					case USB_RQ_GET_STATUS:
						{ // 9.4.5 Get Status, Figure 0-6
							unsigned char status[2] = { 0x00, 0x00 };
							// status[0] & 0x01 : Halt
							buf2EP(0, status, 2, rq->wLength, 0);
						}
						break;
					default:
						unhandled = 1;
				}
				break;

			case USB_RQT_RECIPIENT_OTHER:
			default:
				unhandled = 1;
		}

		if (!unhandled)
		{
			// Handle transmission now
			UEINTX &= ~(1<<TXINI);
			while (1)
			{
				if (UEINTX & (1<<TXINI)) {
					UEINTX &= ~(1<<TXINI);
				}

				if (UEINTX & (1<<RXOUTI)) {
					break;
				}
			}

			UEINTX &= ~(1<<RXOUTI); // ACK
		}
	} // IS DEVICE-TO-HOST

	if (unhandled) {
//		printf_P(PSTR("t: %02x, rq: 0x%02x, val: %04x\r\n"), rq->bmRequestType, rq->bRequest, rq->wValue);
		UECONX |= (1<<STALLRQ);
	}
}

static void handleDataPacket(const struct usb_request *rq, uint8_t *dat, uint16_t len)
{
	uint16_t i;

	if ((rq->bmRequestType & (USB_RQT_TYPE_MASK)) == USB_RQT_CLASS) {

		// TODO : Cechk for HID_CLSRQ_SET_REPORT in rq->bRequest

		// HID 1.1 : 7.2.2 Set_Report request. wIndex is the interface number.

		if (rq->wIndex > g_params->n_hid_interfaces)
			return;

		if (g_params->hid_params[rq->wIndex].setReport) {
			if (g_params->hid_params[rq->wIndex].setReport(rq, dat, len)) {
				UECONX |= (1<<STALLRQ);
			} else {
				// xmit status
				UEINTX &= ~(1<<TXINI);
			}
			return;
		}
	}

	printf_P(PSTR("Unhandled control write [%d] : "), len);
	for (i=0; i<len; i++) {
		printf("%02X ", dat[i]);
	}
	printf("\r\n");
}

// Device interrupt
ISR(USB_GEN_vect)
{
	uint8_t i;
	i = UDINT;

	if (i & (1<<SUSPI)) {
		UDINT &= ~(1<<SUSPI);
		g_usb_suspend = 1;
		UDIEN |= (1<<WAKEUPE);
//		printf_P(PSTR("SUSPI\r\n"));
		// CPU could now be put in low power mode. Later,
		// WAKEUPI would wake it up.
	}

	// this interrupt is to wakeup the cpu from sleep mode.
	if (i & (1<<WAKEUPI)) {
		UDINT &= ~(1<<WAKEUPE);
		if (g_usb_suspend) {
			g_usb_suspend = 0;
//			printf_P(PSTR("WAKEUPI\r\n"));
			UDIEN &= ~(1<<WAKEUPE); // woke up. Not needed anymore.
		}
	}

	if (i & (1<<EORSTI)) {
//		printf_P(PSTR("EORSTI\r\n"));
		g_usb_suspend = 0;
		setupEndpoints();
		UDINT &= ~(1<<EORSTI);
	}

	if (i & (1<<SOFI)) {
		UDINT &= ~(1<<SOFI);
//		printf_P(PSTR("SOFI\r\n"));
	}

	if (i & (1<<EORSMI)) {
		UDINT &= ~(1<<EORSMI);
//		printf_P(PSTR("EORSMI\r\n"));
	}

	if (i & (1<<UPRSMI)) {
		UDINT &= ~(1<<UPRSMI);
		printf_P(PSTR("UPRSMI\r\n"));
	}
}



// Endpoint interrupt
ISR(USB_COM_vect)
{
	uint8_t ueint;
	uint8_t i;

	ueint = UEINT;

	if (ueint & (1<<EPINT0)) {
		UENUM = 0;
		i = UEINTX;

		if (i & (1<<RXSTPI)) {
		//	printf_P(PSTR("RXSTPI\r\n"));
			readEP2buf(g_ep0_buf);
			UEINTX &= ~(1<<RXSTPI);
			handleSetupPacket((struct usb_request *)g_ep0_buf);
		}

		if (i & (1<<RXOUTI)) {
			uint16_t len;
			len = readEP2buf(g_ep0_buf);
			UEINTX &= ~(1<<RXOUTI);
			if (control_write_in_progress) {
			//	printf("chunk: %d\r\n", len);
				if (control_write_len + len < CONTROL_WRITE_BUFSIZE) {
					memcpy(control_write_buf + control_write_len, g_ep0_buf, len);
					control_write_len += len;
				}
			}
		}

		if (i & (1<<NAKINI)) {
			UEINTX &= ~(1<<NAKINI);
			if (control_write_in_progress) {
		//		printf("end. total: %d\n", control_write_len);
				handleDataPacket(&control_write_rq, control_write_buf, control_write_len);
				control_write_in_progress = 0;
			}
		}
	}

	if (ueint & (1<<EPINT1)) {
		UENUM = 1;
		i = UEINTX;

		if (i & (1<<TXINI)) {
			if (interrupt_data_len < 0) {
				// If there's not already data waiting to be
				// sent, disable the interrupt.
				UEIENX &= ~(1<<TXINE);
			} else {
				UEINTX &= ~(1<<TXINI);
				buf2EP(1, interrupt_data, interrupt_data_len, interrupt_data_len, 0);
				interrupt_data = NULL;
				interrupt_data_len = -1;
				UEINTX &= ~(1<<FIFOCON);
			}
		}
	}

#if 0
	if (i & (1<<RXOUTI)) {
		UEINTX &= ~(1<<RXOUTI);
		printf_P(PSTR("RXOUTI\r\n"));
	}
#endif
}

void usb_interruptSend(void *data, int len)
{
	uint8_t sreg = SREG;

	while (interrupt_data_len != -1) { }

	cli();

	interrupt_data = data;
	interrupt_data_len = len;

	UENUM = 1;
	UEIENX |= (1<<TXINE);

	SREG = sreg;
}

void usb_shutdown(void)
{
	UDCON |= (1<<DETACH);

	// Disable interrupts
	UDIEN = 0;


	USBCON &= ~(1<<USBE);
	USBCON |= (1<<FRZCLK); // initial value
#ifdef UHWCON
	UHWCON &= ~(1<<UVREGE); // Disable USB pad regulator
#endif
}

#define STATE_WAIT_VBUS	0
#define STATE_ATTACHED	1
static unsigned char usb_state;

void usb_doTasks(void)
{
	switch (usb_state)
	{
		default:
			usb_state = STATE_WAIT_VBUS;
		case STATE_WAIT_VBUS:
#ifdef USBSTA
			if (USBSTA & (1<<VBUS)) {
#endif
				printf_P(PSTR("ATTACH\r\n"));
				UDCON &= ~(1<<DETACH); // clear DETACH bit
				usb_state = STATE_ATTACHED;
#ifdef USBSTA
			}
#endif
			break;
		case STATE_ATTACHED:
			break;
	}
}

#if defined(__AVR_ATmega32U2__)

/* Atmega32u2 datasheet 8.11.6, PLLCSR.
 * But register summary says PLLP0... */
#ifndef PINDIV
#define PINDIV	2
#endif
static void pll_init(void)
{
#if F_CPU==8000000L
	PLLCSR = 0;
#elif F_CPU==16000000L
	PLLCSR = (1<<PINDIV);
#else
	#error Unsupported clock frequency
#endif
	PLLCSR |= (1<<PLLE);
	while (!(PLLCSR&(1<<PLOCK))) {
		// wait for PLL lock
	}
}
#else
static void pll_init(void)
{
#if F_CPU==8000000L
	// The PLL generates a clock that is 24x a nominal 2MHz input.
	// Hence, we need to divide by 4 the external 8MHz crystal
	// frequency.
	PLLCSR = (1<<PLLP1)|(1<<PLLP0);
#elif F_CPU==16000000L
	// The PLL generates a clock that is 24x a nominal 2MHz input.
	// Hence, we need to divide by 8 the external 16MHz crystal
	// frequency.
	PLLCSR = (1<<PLLP2)|(1<<PLLP0);
#else
	#error Unsupported clock frequency
#endif

	PLLCSR |= (1<<PLLE);
	while (!(PLLCSR&(1<<PLOCK))) {
		// wait for PLL lock
	}
}
#endif

void usb_init(const struct usb_parameters *params)
{
	// Initialize the registers to the default values
	// from the datasheet. The bootloader that sometimes
	// runs before we get here (when doing updates) leaves
	// different values...
#ifdef UHWCON
	UHWCON = 0x80;
#endif
	USBCON = 0x20;
	UDCON = 0x01;
	UDIEN = 0x00;
	UDADDR = 0x00;

	g_params = params;

	// Set some initial values
	USBCON &= ~(1<<USBE);
	USBCON |= (1<<FRZCLK); // initial value
#ifdef UHWCON
	UHWCON |= (1<<UVREGE); // Enable USB pad regulator
	UHWCON &= ~(1<<UIDE);
	UHWCON |= (1<UIMOD);
#endif

#ifdef UPOE
	UPOE = 0; // Disable direct drive of USB pins
#endif
#ifdef REGCR
	REGCR = 0; // Enable the regulator
#endif

	pll_init();

	USBCON |= (1<<USBE);
	USBCON &= ~(1<<FRZCLK); // Unfreeze clock
#ifdef OTGPADE
	USBCON |= (1<<OTGPADE);
#endif

#ifdef LSM
	// Select full speed mode
	UDCON &= (1<<LSM);
#endif

	setupEndpoints();

	UDINT &= ~(1<<SUSPI);
	UDIEN = (1<<SUSPE) | (1<<EORSTE) |/* (1<<SOFE) |*/ (1<<WAKEUPE) | (1<<EORSME) | (1<<UPRSME);
}


