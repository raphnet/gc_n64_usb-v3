/*	gc_n64_usb : Gamecube or N64 controller to USB firmware
	Copyright (C) 2007-2016  Raphael Assenat <raph@raphnet.net>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <string.h>
#include "requests.h"
#include "config.h"
#include "hiddata.h"
#include "usbpad.h"
#include "bootloader.h"
#include "gcn64_protocol.h"
#include "version.h"
#include "main.h"

// dataHidReport is 63 bytes. Endpoint is 64 bytes.
#define CMDBUF_SIZE 64

#define STATE_IDLE			0
#define STATE_NEW_COMMAND	1	// New command in buffer
#define STATE_COMMAND_DONE	2	// Result in buffer

//#define DEBUG

extern char g_polling_suspended;

static volatile uint8_t state = STATE_IDLE;
static unsigned char cmdbuf[CMDBUF_SIZE];
static volatile unsigned char cmdbuf_len = 0;

/*** Get/Set report called from interrupt context! */
uint16_t hiddata_get_report(void *ctx, struct usb_request *rq, const uint8_t **dat)
{
//	printf("Get data\n");
	if (state == STATE_COMMAND_DONE) {
		*dat = cmdbuf;
		state = STATE_IDLE;
#ifdef DEBUG
		printf_P(PSTR("hiddata idle, sent %d bytes\r\n"), cmdbuf_len);
#endif
		return cmdbuf_len;
	}
	return 0;
}

/*** Get/Set report called from interrupt context! */
uint8_t hiddata_set_report(void *ctx, const struct usb_request *rq, const uint8_t *dat, uint16_t len)
{
#ifdef DEBUG
	int i;
	printf_P(PSTR("Set data %d\n"), len);
	for (i=0; i<len; i++) {
		printf_P(PSTR("%02x "), dat[i]);
	}
	printf_P(PSTR("\r\n"));
#endif

	state = STATE_NEW_COMMAND;
	memcpy(cmdbuf, dat, len);
	cmdbuf_len = len;

	return 0;
}

static uint8_t processBlockIO(void)
{
	uint8_t requestCopy[CMDBUF_SIZE];
	int i, rx_offset = 0, rx;
	uint8_t chn, n_tx, n_rx;

	memcpy(requestCopy, cmdbuf, CMDBUF_SIZE);
	memset(cmdbuf + 1, 0xff, CMDBUF_SIZE-1);

	for (rx_offset = 1, i=1; i<CMDBUF_SIZE; ) {
		if (i + 3 >= CMDBUF_SIZE)
			break;

		chn = requestCopy[i];
		if (chn == 0xff)
			break;
		i++;
		n_tx = requestCopy[i];
		i++;
		n_rx = requestCopy[i];
		i++;
		if (n_tx == 0)
			continue;

		if (rx_offset + 1 + n_rx >= CMDBUF_SIZE) {
			break;
		}

		rx = gcn64_transaction(chn, requestCopy + i, n_tx, cmdbuf + rx_offset + 1, n_rx);
		cmdbuf[rx_offset] = n_rx;
		if (rx <= 0) {
			// timeout
			cmdbuf[rx_offset] |= 0x80;
		} else if (rx < n_rx) {
			// less than expected
			cmdbuf[rx_offset] |= 0x40;
		}
		rx_offset += n_rx + 1;

		i += n_tx;
	}

	return 63;
}

static void hiddata_processCommandBuffer(struct hiddata_ops *ops)
{
	unsigned char channel;
#ifdef DEBUG
	int i;
#endif

	if (cmdbuf_len < 1) {
		state = STATE_IDLE;
		return;
	}

//	printf("Process cmd 0x%02x\r\n", cmdbuf[0]);
	switch(cmdbuf[0])
	{
		case RQ_GCN64_ECHO:
			// Cmd : RQ, data[]
			// Answer: RQ, data[]
			break;
		case RQ_GCN64_JUMP_TO_BOOTLOADER:
			enterBootLoader();
			break;
		case RQ_RNT_RESET_FIRMWARE:
			resetFirmware();
			break;
		case RQ_GCN64_RAW_SI_COMMAND:
			// TODO : Range checking
			// cmdbuf[] : RQ, CHN, LEN, data[]
			channel = cmdbuf[1];
			if (channel >= NUM_CHANNELS)
				break;
			cmdbuf_len = gcn64_transaction(channel, cmdbuf+3, cmdbuf[2], cmdbuf + 3, CMDBUF_SIZE-3);
			cmdbuf[2] = cmdbuf_len;
			cmdbuf_len += 3; // Answer: RQ, CHN, LEN, data[]
			break;
		case RQ_GCN64_GET_CONFIG_PARAM:
			// Cmd : RQ, PARAM
			// Answer: RQ, PARAM, data[]
			cmdbuf_len = config_getParam(cmdbuf[1], cmdbuf + 2, CMDBUF_SIZE-2);
			cmdbuf_len += 2; // Datalen + RQ + PARAM
			break;
		case RQ_GCN64_SET_CONFIG_PARAM:
			// Cmd: RQ, PARAM, data[]
			config_setParam(cmdbuf[1], cmdbuf+2);
			// Answer: RQ, PARAM
			cmdbuf_len = 2;
			break;
		case RQ_GCN64_SUSPEND_POLLING:
			// CMD: RQ, PARAM
			if (ops && ops->suspendPolling) {
				ops->suspendPolling(cmdbuf[1]);
			}
			break;
		case RQ_GCN64_GET_VERSION:
			// CMD: RQ
			// Answer: RQ, version string (zero-terminated)
			strcpy((char*)(cmdbuf + 1), g_version);
			cmdbuf_len = 1 + strlen(g_version) + 1;
			break;
		case RQ_GCN64_GET_SIGNATURE:
			strcpy_P((char*)(cmdbuf + 1), g_signature);
			cmdbuf_len = 1 + strlen_P(g_signature) + 1;
			break;
		case RQ_GCN64_GET_CONTROLLER_TYPE:
			// CMD : RQ, CHN
			// Answer: RQ, CHN, TYPE
			channel = cmdbuf[1];
			if (channel >= NUM_CHANNELS)
				break;
			cmdbuf[2] = current_pad_type[channel];
			cmdbuf_len = 3;
			break;
		case RQ_GCN64_SET_VIBRATION:
			// CMD : RQ, CHN, Vibrate
			// Answer: RQ, CHN, Vibrate
			if (ops && ops->forceVibration) {
				ops->forceVibration(cmdbuf[1], cmdbuf[2]);
			}
			cmdbuf_len = 3;
			break;
		case RQ_GCN64_BLOCK_IO:
			cmdbuf_len = processBlockIO();
			break;
		case RQ_RNT_GET_SUPPORTED_REQUESTS:
			cmdbuf[1] = RQ_GCN64_JUMP_TO_BOOTLOADER;
			cmdbuf[2] = RQ_GCN64_RAW_SI_COMMAND;
			cmdbuf[3] = RQ_GCN64_GET_CONFIG_PARAM;
			cmdbuf[4] = RQ_GCN64_SET_CONFIG_PARAM;
			cmdbuf[5] = RQ_GCN64_SUSPEND_POLLING;
			cmdbuf[6] = RQ_GCN64_GET_VERSION;
			cmdbuf[7] = RQ_GCN64_GET_SIGNATURE;
			cmdbuf[8] = RQ_GCN64_GET_CONTROLLER_TYPE;
			cmdbuf[9] = RQ_GCN64_SET_VIBRATION;
			cmdbuf[10] = RQ_GCN64_BLOCK_IO;
			cmdbuf[11] = RQ_RNT_GET_SUPPORTED_CFG_PARAMS;
			cmdbuf[12] = RQ_RNT_GET_SUPPORTED_MODES;
			cmdbuf[13] = RQ_RNT_GET_SUPPORTED_REQUESTS;
			cmdbuf_len = 14;
			break;
		case RQ_RNT_GET_SUPPORTED_CFG_PARAMS:
			cmdbuf_len = 1 + config_getSupportedParams(cmdbuf + 1);
			break;
		case RQ_RNT_GET_SUPPORTED_MODES:
			cmdbuf_len = 1;
			if (ops && ops->getSupportedModes) {
				cmdbuf_len += ops->getSupportedModes(cmdbuf + 1);
			}
			break;

	}

#ifdef DEBUG
	printf("Pending data %d\n", cmdbuf_len);
	for (i=0; i<cmdbuf_len; i++) {
		printf("%02x ", cmdbuf[i]);
	}
	printf("\r\n");
#endif

	state = STATE_COMMAND_DONE;
}

void hiddata_doTask(struct hiddata_ops *ops)
{
	switch (state)
	{
		default:
			state = STATE_IDLE;
		case STATE_IDLE:
			break;

		case STATE_NEW_COMMAND:
			hiddata_processCommandBuffer(ops);
			break;

		case STATE_COMMAND_DONE:
			break;
	}
}
