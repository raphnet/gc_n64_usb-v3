#include <stdio.h>
#include <string.h>
#include "gcn64lib.h"
#include "gc2n64_adapter.h"
#include "hexdump.h"

int gc2n64_adapter_echotest(gcn64_hdl_t hdl, int verbose)
{
	unsigned char cmd[35];
	unsigned char buf[64];
	int i, n;

	cmd[0] = 'R';
	cmd[1] = 0x00; // echo
	for (i=0; i<33; i++) {
		cmd[i+2] = 'A'+i;
	}

	n = gcn64lib_rawSiCommand(hdl, 0, cmd, 35, buf, 35);
	if (n<0) {
		return n;
	}

	if (verbose) {
		printf("    Sent [%d]: ", 35); printHexBuf(cmd, 35);
		printf("Received [%d]: ", n); printHexBuf(buf, n);
		if (!memcmp(cmd, buf, 35)) {
			printf("Test OK\n");
		} else {
			printf("Test failed\n");
		}
	}

	return memcmp(cmd, buf, 32);
}

int gc2n64_adapter_getMapping(gcn64_hdl_t hdl, int id)
{
	unsigned char buf[64];
	unsigned char cmd[4];
	int n;
	int mapping_size;

	cmd[0] = 'R';
	cmd[1] = 0x02; // Get mapping
	cmd[2] = id;
	cmd[3] = 0; // chunk 0 (size)

	n = gcn64lib_rawSiCommand(hdl, 0, cmd, 4, buf, 4);
	if (n<0)
		return n;

	if (n == 1) {
		int i, pos;
		mapping_size = buf[0];
		printf("Mapping %d size: %d\n", id, mapping_size);

		for (pos=0, i=0; pos<mapping_size; i++) {
			cmd[0] = 'R';
			cmd[1] = 0x02; // Get mapping
			cmd[2] = id;
			cmd[3] = i+1; // chunk 1 is first 32 byte block, 2nd is next 32 bytes, etc
			printf("Getting block %d\n", i+1);
			n = gcn64lib_rawSiCommand(hdl, 0, cmd, 4, buf + pos, 32);
			if (n<0) {
				return n;
			}
			printf("ret: %d\n", n);
			if (n==0)
				break;
			pos += n;
		}

		printf("Received %d bytes\n", pos);
		printHexBuf(buf, pos);
	}

	return 0;
}


void gc2n64_adapter_printInfo(struct gc2n64_adapter_info *inf)
{
	if (!inf->in_bootloader) {
		printf("gc_to_n64 adapter info: {\n");

		printf("\tDefault mapping id: %d\n", inf->app.default_mapping_id);
		printf("\tDeadzone enabled: %d\n", inf->app.deadzone_enabled);
		printf("\tOld v1.5 conversion: %d\n", inf->app.old_v1_5_conversion);
		printf("\tFirmware version: %s\n", inf->app.version);
	} else {
		printf("gc_to_n64 adapter in bootloader mode: {\n");

		printf("\tBootloader firmware version: %s\n", inf->bootldr.version);
		printf("\tMCU page size: %d bytes\n", inf->bootldr.mcu_page_size);
		printf("\tBootloader code start address: 0x%04x\n", inf->bootldr.bootloader_start_address);
	}

	printf("}\n");
}

int gc2n64_adapter_getInfo(gcn64_hdl_t hdl, struct gc2n64_adapter_info *inf)
{
	unsigned char buf[64];
	int n;

	buf[0] = 'R';
	buf[1] = 0x01; // Get device info

	n = gcn64lib_rawSiCommand(hdl, 0, buf, 2, buf, sizeof(buf));
	if (n<0)
		return n;

	if (n > 0) {
		if (!inf)
			return 0;

		inf->in_bootloader = buf[0];

		if (!inf->in_bootloader) {
			inf->app.default_mapping_id = buf[2];
			inf->app.deadzone_enabled = buf[3];
			inf->app.old_v1_5_conversion = buf[4];
			inf->app.version[sizeof(inf->app.version)-1]=0;
			strncpy(inf->app.version, (char*)buf+10, sizeof(inf->app.version)-1);
		} else {
			inf->bootldr.mcu_page_size = buf[1];
			inf->bootldr.bootloader_start_address = buf[2] << 8 | buf[3];
			inf->bootldr.version[sizeof(inf->bootldr.version)-1]=0;
			strncpy(inf->bootldr.version, (char*)buf+10, sizeof(inf->bootldr.version)-1);
		}

	} else {
		printf("No answer (old version?)\n");
		return -1;
	}

	return 0;
}

int gc2n64_adapter_boot_eraseAll(gcn64_hdl_t hdl)
{
	unsigned char buf[64];
	int n;

	buf[0] = 'R';
	buf[1] = 0xf0;

	n = gcn64lib_rawSiCommand(hdl, 0, buf, 2, buf, sizeof(buf));
	if (n<0)
		return n;

	if (n != 3) {
		fprintf(stderr, "Invalid answer\n");
		return -1;
	}

	return 0;
}

int gc2n64_adapter_boot_readBlock(gcn64_hdl_t hdl, unsigned int block_id, unsigned char dst[32])
{
	unsigned char buf[64];
	int n;

	buf[0] = 'R';
	buf[1] = 0xf1;
	buf[2] = block_id >> 8;
	buf[3] = block_id & 0xff;

	n = gcn64lib_rawSiCommand(hdl, 0, buf, 4, buf, sizeof(buf));
	if (n<0)
		return n;

	if (n != 32) {
		fprintf(stderr, "Invalid answer\n");
		return -1;
	}

	memcpy(dst, buf, 32);

	return 0;
}

int gc2n64_adapter_dumpFlash(gcn64_hdl_t hdl)
{
	int i;
	unsigned char buf[0x2000];

	// Atmega8 : 128 pages of 32 words (64 bytes). 8K.
	for (i=0; i<0x2000; i+= 32)
	{
		gc2n64_adapter_boot_readBlock(hdl, i/32, buf + i);
		printf("0x%04x: ", i);
		printHexBuf(buf + i, 32);
	}
	return 0;
}

int gc2n64_adapter_updateFirmware(gcn64_hdl_t hdl, const char *hexfile)
{
	return 0;
}

