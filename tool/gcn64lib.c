#include <string.h>
#include <stdio.h>
#include "gcn64lib.h"
#include "../requests.h"
#include "../gcn64_protocol.h"
#include "hexdump.h"

int gcn64lib_getConfig(gcn64_hdl_t hdl, unsigned char param, unsigned char *rx, unsigned char rx_max)
{
	unsigned char cmd[2];
	int n;

	cmd[0] = RQ_GCN64_GET_CONFIG_PARAM;
	cmd[1] = param;

	n = gcn64_exchange(hdl, cmd, 2, rx, rx_max);
	if (n<2)
		return n;

	n -= 2;

	// Drop the leading CMD and PARAM
	if (n) {
		memmove(rx, rx+2, n);
	}

	return n;
}
int gcn64lib_setConfig(gcn64_hdl_t hdl, unsigned char param, unsigned char *data, unsigned char len)
{
	unsigned char cmd[2 + len];
	int n;

	cmd[0] = RQ_GCN64_SET_CONFIG_PARAM;
	cmd[1] = param;
	memcpy(cmd + 2, data, len);

	n = gcn64_exchange(hdl, cmd, 2 + len, cmd, sizeof(cmd));
	if (n<0)
		return n;

	return 0;
}

int gcn64lib_suspendPolling(gcn64_hdl_t hdl, unsigned char suspend)
{
	unsigned char cmd[2];
	int n;

	cmd[0] = RQ_GCN64_SUSPEND_POLLING;
	cmd[1] = suspend;

	n = gcn64_exchange(hdl, cmd, 2, cmd, sizeof(cmd));
	if (n<0)
		return n;

	return 0;
}

int gcn64lib_getVersion(gcn64_hdl_t hdl, char *dst, int dstmax)
{
	unsigned char cmd[32];
	int n;

	if (dstmax <= 0)
		return -1;

	cmd[0] = RQ_GCN64_GET_VERSION;

	n = gcn64_exchange(hdl, cmd, 1, cmd, sizeof(cmd));
	if (n<0)
		return n;

	dst[0] = 0;
	if (n > 1) {
		strncpy(dst, (char*)cmd+1, n);
	}
	dst[dstmax-1] = 0;

	return 0;
}

int gcn64lib_getControllerType(gcn64_hdl_t hdl, int chn)
{
	unsigned char cmd[32];
	int n;

	cmd[0] = RQ_GCN64_GET_CONTROLLER_TYPE;
	cmd[1] = chn;

	n = gcn64_exchange(hdl, cmd, 2, cmd, sizeof(cmd));
	if (n<0)
		return n;
	if (n<3)
		return -1;

	return cmd[2];
}

const char *gcn64lib_controllerName(int type)
{
	switch(type) {
		case CTL_TYPE_NONE: return "No controller";
		case CTL_TYPE_N64: return "N64 Controller";
		case CTL_TYPE_GC: return "GC Controller";
		case CTL_TYPE_GCKB: return "GC Keyboard";
		default:
			return "Unknown";
	}
}

int gcn64lib_getSignature(gcn64_hdl_t hdl, char *dst, int dstmax)
{
	unsigned char cmd[40];
	int n;

	if (dstmax <= 0)
		return -1;

	cmd[0] = RQ_GCN64_GET_SIGNATURE;

	n = gcn64_exchange(hdl, cmd, 1, cmd, sizeof(cmd));
	if (n<0)
		return n;

	dst[0] = 0;
	if (n > 1) {
		strncpy(dst, (char*)cmd+1, n);
	}
	dst[dstmax-1] = 0;

	return 0;
}

int gcn64lib_forceVibration(gcn64_hdl_t hdl, unsigned char channel, unsigned char vibrate)
{
	unsigned char cmd[3];
	int n;

	cmd[0] = RQ_GCN64_SET_VIBRATION;
	cmd[1] = channel;
	cmd[2] = vibrate;

	n = gcn64_exchange(hdl, cmd, 3, cmd, sizeof(cmd));
	if (n<0)
		return n;

	return 0;
}

int gcn64lib_rawSiCommand(gcn64_hdl_t hdl, unsigned char channel, unsigned char *tx, unsigned char tx_len, unsigned char *rx, unsigned char max_rx)
{
	unsigned char cmd[3 + tx_len];
	unsigned char rep[3 + 64];
	int cmdlen, rx_len, n;

	cmd[0] = RQ_GCN64_RAW_SI_COMMAND;
	cmd[1] = channel;
	cmd[2] = tx_len;
	memcpy(cmd+3, tx, tx_len);
	cmdlen = 3 + tx_len;

	n = gcn64_exchange(hdl, cmd, cmdlen, rep, sizeof(rep));
	if (n<0)
		return n;

	rx_len = rep[2];
	memcpy(rx, rep + 3, rx_len);

	return rx_len;
}

int gcn64lib_16bit_scan(gcn64_hdl_t hdl, unsigned short min, unsigned short max)
{
	int id, n;
	unsigned char buf[64];

	for (id = min; id<=max; id++) {
		buf[0] = id >> 8;
		buf[1] = id & 0xff;
		n = gcn64lib_rawSiCommand(hdl, 0, buf, 2, buf, sizeof(buf));
		if (n > 0) {
			printf("CMD 0x%04x answer: ", id);
			printHexBuf(buf, n);
		}
	}

	return 0;
}

int gcn64lib_8bit_scan(gcn64_hdl_t hdl, unsigned char min, unsigned char max)
{
	int id, n;
	unsigned char buf[64];

	for (id = min; id<=max; id++) {
		buf[0] = id;
		n = gcn64lib_rawSiCommand(hdl, 0, buf, 1, buf, sizeof(buf));
		if (n > 0) {
			printf("CMD 0x%02x answer: ", id);
			printHexBuf(buf, n);
		}
	}

	return 0;
}

int gcn64lib_bootloader(gcn64_hdl_t hdl)
{
	unsigned char cmd[4];
	int cmdlen;

	cmd[0] = RQ_GCN64_JUMP_TO_BOOTLOADER;
	cmdlen = 1;

	gcn64_exchange(hdl, cmd, cmdlen, cmd, sizeof(cmd));

	return 0;
}

int gcn64lib_n64_expansionWrite(gcn64_hdl_t hdl, unsigned short addr, unsigned char data[32])
{
	unsigned char cmd[40];
	int cmdlen;
	int n;

	cmd[0] = N64_EXPANSION_WRITE;
	cmd[1] = addr>>8; // Address high byte
	cmd[2] = addr&0xff; // Address low byte
	memcpy(cmd + 3, data, 0x20);
	cmdlen = 3 + 0x20;

	n = gcn64lib_rawSiCommand(hdl, 0, cmd, cmdlen, cmd, sizeof(cmd));
	if (n != 1) {
		printf("write block returned != 1 (%d)\n", n);
		return -1;
	}

	return cmd[0];
}


