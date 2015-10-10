#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gcn64lib.h"
#include "gc2n64_adapter.h"
#include "hexdump.h"
#include "ihex.h"
#include "delay.h"

int gc2n64_adapter_echotest(gcn64_hdl_t hdl, int channel, int verbose)
{
	unsigned char cmd[30];
	unsigned char buf[30];
	int i, n;

	cmd[0] = 'R';
	cmd[1] = 0x00; // echo
	for (i=0; i<28; i++) {
		cmd[i+2] = 'A'+i;
	}

	n = gcn64lib_rawSiCommand(hdl, channel, cmd, sizeof(buf), buf, sizeof(buf));
	if (n<0) {
		return n;
	}


	if (verbose) {
		if ((n != sizeof(buf)) || memcmp(cmd, buf, sizeof(buf))) {
			printf("Test failed\n");
			printf("    Sent [%d]: ", (int)sizeof(cmd)); printHexBuf(cmd, sizeof(cmd));
			printf("Received [%d]: ", n); printHexBuf(buf, n);
			return -1;
		}
	}
	return (n!= sizeof(buf)) || memcmp(cmd, buf, sizeof(buf));
}

int gc2n64_adapter_getMapping(gcn64_hdl_t hdl, int channel, int id)
{
	unsigned char buf[64];
	unsigned char cmd[4];
	int n;
	int mapping_size;

	cmd[0] = 'R';
	cmd[1] = 0x02; // Get mapping
	cmd[2] = id;
	cmd[3] = 0; // chunk 0 (size)

	n = gcn64lib_rawSiCommand(hdl, channel, cmd, 4, buf, 4);
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
			n = gcn64lib_rawSiCommand(hdl, channel, cmd, 4, buf + pos, 32);
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

int gc2n64_adapter_getInfo(gcn64_hdl_t hdl, int channel, struct gc2n64_adapter_info *inf)
{
	unsigned char buf[32];
	int n;

	buf[0] = 'R';
	buf[1] = 0x01; // Get device info

	n = gcn64lib_rawSiCommand(hdl, channel, buf, 2, buf, sizeof(buf));
	if (n<0)
		return n;

	if (n > 0) {
		// On N64, when receiving an all 0xFF reply, catch it here.
		if (buf[0] == 0xff)
			return -1;

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

int gc2n64_adapter_boot_isBusy(gcn64_hdl_t hdl, int channel)
{
	unsigned char buf[64];
	int n;

	buf[0] = 'R';
	buf[1] = 0xf9;

	n = gcn64lib_rawSiCommand(hdl, channel, buf, 2, buf, 1);
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

int gc2n64_adapter_boot_waitNotBusy(gcn64_hdl_t hdl, int channel, int verbose)
{
	char spinner[4] = { '|','/','-','\\' };
	int busy, no_reply_count=0;
	int c=0;

	while ((busy = gc2n64_adapter_boot_isBusy(hdl, channel)))
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

int gc2n64_adapter_boot_eraseAll(gcn64_hdl_t hdl, int channel)
{
	unsigned char buf[64];
	int n;

	buf[0] = 'R';
	buf[1] = 0xf0;

	n = gcn64lib_rawSiCommand(hdl, channel, buf, 2, buf, 1);
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

int gc2n64_adapter_boot_readBlock(gcn64_hdl_t hdl, int channel, unsigned int block_id, unsigned char dst[32])
{
	unsigned char buf[32];
	int n;

	buf[0] = 'R';
	buf[1] = 0xf1;
	buf[2] = block_id >> 8;
	buf[3] = block_id & 0xff;

	n = gcn64lib_rawSiCommand(hdl, channel, buf, 4, buf, sizeof(buf));
	if (n<0)
		return n;

	if (n != 32) {
		fprintf(stderr, "Invalid answer\n");
		return -1;
	}

	memcpy(dst, buf, 32);

	return 0;
}

int gc2n64_adapter_dumpFlash(gcn64_hdl_t hdl, int channel)
{
	int i;
	unsigned char buf[0x10000];
	struct gc2n64_adapter_info inf;

	i = gc2n64_adapter_getInfo(hdl, channel, &inf);
	if (i)
		return i;

	if (!inf.in_bootloader) {
		fprintf(stderr, "dumpFlash: Nnot in bootloader\n");
		return -1;
	}

	// Atmega168 : 16K
	for (i=0; i<16*1024; i+= 32)
	{
		gc2n64_adapter_boot_readBlock(hdl, channel, i/32, buf + i);
		printf("0x%04x: ", i);
		printHexBuf(buf + i, 32);
	}
	return 0;
}

int gc2n64_adapter_enterBootloader(gcn64_hdl_t hdl, int channel)
{
	unsigned char buf[4];
	int n;
	int t = 1000; // > 100ms timeout


	/* The bootloader starts the application automatically if it is
	 * installed. To prevent the application from being restarted right
	 * away when are entering the bootloader, the bootloader waits
	 * 50 ms at startup, and if it receives the 'enter bootloader' command
	 * within this window, the application is not started.
	 *
	 * Also, contrary to the application, the bootloader actually answers
	 * this command. So it doubles as a handshake to know the bootloader has
	 * started and is ready to receive instructions.
	 *
	 * */
	do {
		buf[0] = 'R';
		buf[1] = 0xff;

		n = gcn64lib_rawSiCommand(hdl, channel, buf, 2, buf, sizeof(buf));
		if (n<0) {
			return n;
		}

		if (buf[0] == 0xff && buf[1] == 0xff) {
			n = 0;
		}
		_delay_us(1000);
		t--;
		if (!t) {
			fprintf(stderr, "Timeout waiting for bootloader\n");
			return -1;
		}
	}
	while(n==0);

	return 0;
}

int gc2n64_adapter_bootApplication(gcn64_hdl_t hdl, int channel)
{
	unsigned char buf[2];
	int n;

	buf[0] = 'R';
	buf[1] = 0xfe;

	n = gcn64lib_rawSiCommand(hdl, channel, buf, 2, buf, 1);
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
int gc2n64_adapter_sendFirmwareBlocks(gcn64_hdl_t hdl, int channel, unsigned char *firmware, int len)
{
	unsigned char buf[64];
	int i, block_id;
	int n;

	for (i=0; i<len; i+=32) {
		block_id = i / 32;
		buf[0] = 'R';
		buf[1] = 0xf2;
		buf[2] = block_id >> 8;
		buf[3] = block_id & 0xff;
		memcpy(buf + 4, firmware+i, 32);

		printf("Block %d / %d\r", block_id+1, len / 32); fflush(stdout);

		n = gcn64lib_rawSiCommand(hdl, channel, buf, 4 + 32, buf, 4);
		if (n<0) {
			fprintf(stderr, "\nRaw command failed\n");
			return n;
		}

		if (n != 4) {
			fprintf(stderr, "\nInvalid upload block answer\n");
			return -1;
		}

		// [0] ACK (should be 0x00)
		// [1] Need to poll?
		// [2] Block ID high
		// [3] Block ID low

		if (buf[0] != 0x00) {
			fprintf(stderr, "Busy\n");
			return -1;
		}

		if (buf[1]) {
			if (gc2n64_adapter_boot_waitNotBusy(hdl, channel, 1)) {
				fprintf(stderr, "Error waiting not busy\n");
				return -1;
			}
		}

//		printf("\n");
//		printf("Block ID: 0x%04x\n", (buf[2]<<8) | buf[3]);
	}

	return 0;
}

int gc2n64_adapter_verifyFirmware(gcn64_hdl_t hdl, int channel, unsigned char *firmware, int len)
{
	unsigned char buf[32];
	int i;

	for (i=0; i<len; i+=32) {

		gc2n64_adapter_boot_readBlock(hdl, channel, i/32, buf);
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

int gc2n64_adapter_waitForBootloader(gcn64_hdl_t hdl, int channel, int timeout_s)
{
	struct gc2n64_adapter_info inf;
	int i;
	int n;

	for (i=0; i<=timeout_s; i++) {
		n = gc2n64_adapter_getInfo(hdl, channel, &inf);
		// Errors (caused by timeouts) are just ignored since they are expected.
		if (n == 0) {
			gc2n64_adapter_printInfo(&inf);
			if (inf.in_bootloader)
				return 0;
		}
		_delay_s(1);
	}

	return -1;
}

#define FIRMWARE_BUF_SIZE	0x10000
int gc2n64_adapter_updateFirmware(gcn64_hdl_t hdl, int channel, const char *hexfile)
{
	unsigned char *buf;
	int max_addr;
	int ret = 0, res;
	struct gc2n64_adapter_info inf;

	////////////////////
	printf("step [1/7] : Load .hex file...\n");
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
	printf("step [2/7] : Get adapter info...\n");
	res = gc2n64_adapter_getInfo(hdl, channel, &inf);
	if (res < 0) {
		fprintf(stderr, "Failed to read adapter info\n");
		return -1;
	}
	gc2n64_adapter_printInfo(&inf);

	if (inf.in_bootloader) {
		printf("step [3/7] : Enter bootloader... Skipped. Already in bootloader.\n");
	} else {
		printf("step [3/7] : Enter bootloader...\n");
		res = gc2n64_adapter_enterBootloader(hdl, channel);
		if (res < 0) {
			fprintf(stderr, "Failed to enter the bootloader\n");
			return -1;
		}

		// Re-read the info structure, as we will need the bootloader start address.
		res = gc2n64_adapter_getInfo(hdl, channel, &inf);
		if (res < 0) {
			fprintf(stderr, "Failed to read info after enterring bootloader\n");
			return -1;
		}
	}

	////////////////////
	printf("step [4/7] : Erase current firmware... "); fflush(stdout);
	gc2n64_adapter_boot_eraseAll(hdl, channel);

	if (gc2n64_adapter_boot_waitNotBusy(hdl, channel, 1)) {
		return -1;
	}
	printf("Ok\n");

	////////////////////
	// We need to add a marker at the end of the application area (just before the
	// bootloader) so the bootloader knows an application is installed.
	if (max_addr >= inf.bootldr.bootloader_start_address - 4) {
		fprintf(stderr, "No space for marker - application too large. Aborting\n");
		return -1;
	}
	buf[inf.bootldr.bootloader_start_address - 4] = 0x12;
	buf[inf.bootldr.bootloader_start_address - 3] = 0x34;
	buf[inf.bootldr.bootloader_start_address - 2] = 0x56;
	buf[inf.bootldr.bootloader_start_address - 1] = 0x78;

	printf("step [5/7] : Write new firmware...\n");
	// Note: We write up to the bootloader, even if the firmware was shorter (it usually is).
	// This is to make sure that the marker we placed at the end gets written.
	res = gc2n64_adapter_sendFirmwareBlocks(hdl, channel, buf, inf.bootldr.bootloader_start_address);
	if (res < 0) {
		return -1;
	}

	printf("step [6/7] : Verify firmware...\n");
	res = gc2n64_adapter_verifyFirmware(hdl, channel, buf, inf.bootldr.bootloader_start_address);
	if (res < 0) {
		printf("Verify failed : Update failed\n");
		return -1;
	}

	printf("step [7/7] : Launch new firmware.\n");
	gc2n64_adapter_bootApplication(hdl, channel);

err:
	free(buf);
	return ret;
}


