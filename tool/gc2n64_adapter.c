#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gcn64lib.h"
#include "gc2n64_adapter.h"
#include "hexdump.h"
#include "ihex.h"
#include "delay.h"

int gc2n64_adapter_echotest(gcn64_hdl_t hdl, int verbose)
{
	unsigned char cmd[35];
	unsigned char buf[64];
	int i, n;
	int times;

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
		if ((n != 35) || memcmp(cmd, buf, 35)) {
			printf("Test failed\n");
			printf("    Sent [%d]: ", 35); printHexBuf(cmd, 35);
			printf("Received [%d]: ", n); printHexBuf(buf, n);
			return -1;
		}
	}
	return (n!= 35) || memcmp(cmd, buf, 35);
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

int gc2n64_adapter_boot_isBusy(gcn64_hdl_t hdl)
{
	unsigned char buf[64];
	int n;

	buf[0] = 'R';
	buf[1] = 0xf9;

	n = gcn64lib_rawSiCommand(hdl, 0, buf, 2, buf, sizeof(buf));
	if (n<0)
		return n;

	if (n != 1) {
		return 2; // Busy inferred from lack of answer
	}

	if (buf[0] != 0x00) {
		return 1; // Busy
	}

	return 0; // Idle
}

int gc2n64_adapter_boot_waitNotBusy(gcn64_hdl_t hdl, int verbose)
{
	char spinner[4] = { '|','/','-','\\' };
	int busy, no_reply_count=0;
	int c=0;

	while ((busy = gc2n64_adapter_boot_isBusy(hdl)))
	{
		if (busy < 0) {
			return -1;
		}
		if (busy == 2) {
			no_reply_count++;
			if (no_reply_count > 200) {
				fprintf(stderr, "Adapter answer timeout\n");
				return -1;
			}
		}
		printf("%c\b", spinner[c%4]); fflush(stdout);
		c++;
		_delay_us(50000);
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

	if (n != 1) {
		fprintf(stderr, "Invalid answer. %d bytes received.\n", n);
		return -1;
	}

	if (buf[0] != 0x00) {
		fprintf(stderr, "eraseAll request NACK!\n");
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

int gc2n64_adapter_enterBootloader(gcn64_hdl_t hdl)
{
	unsigned char buf[64];
	int n;

	buf[0] = 'R';
	buf[1] = 0xff;

	n = gcn64lib_rawSiCommand(hdl, 0, buf, 4 + 32, buf, sizeof(buf));
		if (n<0)
			return n;

	// No answer since the effect is immediate.
	_delay_us(100000);

	return 0;
}

int gc2n64_adapter_bootApplication(gcn64_hdl_t hdl)
{
	unsigned char buf[64];
	int n;

	buf[0] = 'R';
	buf[1] = 0xfe;

	n = gcn64lib_rawSiCommand(hdl, 0, buf, 4 + 32, buf, sizeof(buf));
		if (n<0)
			return n;

	if (n != 1) {
		fprintf(stderr, "boot application: Invalid answer\n");
		return -1;
	}

	if (buf[0]) {
		fprintf(stderr, "Boot nack\n");
		return -1;
	}

	return 0;
}

// Note: eraseAll needs to be performed first
int gc2n64_adapter_sendFirmwareBlocks(gcn64_hdl_t hdl, unsigned char *firmware, int len)
{
	unsigned char buf[64];
	int i, block_id;
	int n;

	for (i=0; i<len; i+=32) {
		block_id = i / 32;

		printf("Block %d / %d\r", block_id+1, len / 32); fflush(stdout);

		buf[0] = 'R';
		buf[1] = 0xf2;
		buf[2] = block_id >> 8;
		buf[3] = block_id & 0xff;
		memcpy(buf + 4, firmware+i, 32);

		n = gcn64lib_rawSiCommand(hdl, 0, buf, 4 + 32, buf, sizeof(buf));
		if (n<0)
			return n;

		if (n < 1) {
			fprintf(stderr, "Invalid upload block answer\n");
//			return -1;
			goto hope;
		}

		if (n == 1) {
			fprintf(stderr, "upload block: Busy\n");
			return -1;
		}

		if (n != 4) {
			fprintf(stderr, "Invalid upload block answer 2\n");
			return -1;
		}

		// [0] ACK (should be 0x00)
		// [1] Need to poll?
		// [2] Page address
		// [3] Page address

		if (buf[0] != 0x00) {
			fprintf(stderr, "Invalid upload block answer 3\n");
			return -1;
		}

		if (buf[1]) {
hope:
			if (gc2n64_adapter_boot_waitNotBusy(hdl, 1)) {
				return -1;
			}
		}
	}

	return 0;
}

int gc2n64_adapter_verifyFirmware(gcn64_hdl_t hdl, unsigned char *firmware, int len)
{
	unsigned char buf[32];
	int i;

	for (i=0; i<len; i+=32) {

		gc2n64_adapter_boot_readBlock(hdl, i/32, buf);
		if (memcmp(buf, firmware + i, 32)) {
			printf("\nMismatch in block address 0x%04x\n", i);
			printf("Written: "); printHexBuf(firmware + i, 32);
			printf("   Read: "); printHexBuf(buf, 32);
			return -1;
		} else {
			printf("Block %d / %d ok\r", i/32 + 1, len / 32); fflush(stdout);
		}
	}
	return 0;
}

int gc2n64_adapter_waitForBootloader(gcn64_hdl_t hdl, int timeout_s)
{
	struct gc2n64_adapter_info inf;
	int i;
	int n;

	for (i=0; i<=timeout_s; i++) {
		n = gc2n64_adapter_getInfo(hdl, &inf);
		// Errors (caused by timeouts) are just ignored since they are expected.
		if (n == 0) {
			if (inf.in_bootloader)
			return 0;
		}

		_delay_s(1);
	}

	return -1;
}

#define FIRMWARE_BUF_SIZE	0x10000
int gc2n64_adapter_updateFirmware(gcn64_hdl_t hdl, const char *hexfile)
{
	unsigned char *buf;
	int max_addr;
	int ret = 0, res;
	struct gc2n64_adapter_info inf;

	////////////////////
	printf("gc2n64 firmware update, step [1/7] : Load .hex file...\n");
	buf = malloc(FIRMWARE_BUF_SIZE);
	if (!buf) {
		perror("malloc");
		return -1;
	}
	memset(buf, 0xff, FIRMWARE_BUF_SIZE);

	max_addr = load_ihex(hexfile, buf, FIRMWARE_BUF_SIZE);
	if (max_addr < 0) {
		fprintf(stderr, "Update failed : Could not load hex file\n");
		ret = -1;
		goto err;
	}

	printf("Firmware size: %d bytes\n", max_addr+1);

	////////////////////
	printf("gc2n64 firmware update, step [2/7] : Get adapter info...\n");
	gc2n64_adapter_getInfo(hdl, &inf);
	gc2n64_adapter_printInfo(&inf);

	if (inf.in_bootloader) {
		printf("gc2n64 firmware update, step [3/7] : Enter bootloader... Skipped. Already in bootloader.\n");
	} else {
		printf("gc2n64 firmware update, step [3/7] : Enter bootloader...\n");
		gc2n64_adapter_enterBootloader(hdl);
		printf("waiting for bootloader...\n");
		res = gc2n64_adapter_waitForBootloader(hdl, 5);
		if (res<0) {
			fprintf(stderr, "Bootloader did not start?\n");
			return -1;
		}
	}

	////////////////////
	printf("gc2n64 firmware update, step [4/7] : Erase current firmware... "); fflush(stdout);
	gc2n64_adapter_boot_eraseAll(hdl);

	if (gc2n64_adapter_boot_waitNotBusy(hdl, 1)) {
		return -1;
	}
	printf("Ok\n");

	////////////////////
	printf("gc2n64 firmware update, step [5/7] : Write new firmware...\n");
	res = gc2n64_adapter_sendFirmwareBlocks(hdl, buf, inf.bootldr.bootloader_start_address);
	if (res < 0) {
		return -1;
	}

	printf("gc2n64 firmware update, step [6/7] : Verify firmware...\n");
	res = gc2n64_adapter_verifyFirmware(hdl, buf, max_addr);
	if (res < 0) {
		printf("Verify failed : Update failed\n");
		return -1;
	}

	printf("gc2n64 firmware update, step [7/7] : Launch new firmware.\n");
	gc2n64_adapter_bootApplication(hdl);

err:
	free(buf);
	return ret;
}


