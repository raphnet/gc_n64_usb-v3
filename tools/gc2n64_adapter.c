/*	gc_n64_usb : Gamecube or N64 controller to USB adapter firmware
	Copyright (C) 2007-2015  Raphael Assenat <raph@raphnet.net>

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
#define _GNU_SOURCE // for memmem
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gcn64lib.h"
#include "gc2n64_adapter.h"
#include "hexdump.h"
#include "ihex.h"
#include "delay.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

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

int gc2n64_adapter_storeCurrentMapping(gcn64_hdl_t hdl, int channel, int dst_slot)
{
	int n;
	unsigned char cmd[3];

	cmd[0] = 'R';
	cmd[1] = 0x04; // Save current mapping
	cmd[2] = dst_slot;

	n = gcn64lib_rawSiCommand(hdl, channel, cmd, sizeof(cmd), cmd, 1);
	if (n<0) {
		return n;
	}
	if (n != 1) {
		fprintf(stderr, "Communication error while storing mapping\n");
		return -1;
	}

	if (cmd[0] == 0x00) {
		return gc2n64_adapter_waitNotBusy(hdl, channel, 0);
	}
	else {
		fprintf(stderr, "storeCurrentMapping: Command NACKed\n");
		return -1;
	}
}

int gc2n64_adapter_setMapping(gcn64_hdl_t hdl, int channel, struct gc2n64_adapter_mapping *mapping)
{
	unsigned char buf[64];
	unsigned char mapdata[64];
	int i, n;
	int maplen, togo, done, chunk;


	maplen = mapping->n_pairs * 2;

	if (maplen > sizeof(mapdata)) {
		fprintf(stderr, "Mapping too large\n");
		return -1;
	}

	for (i=0; i<mapping->n_pairs; i++) {
		mapdata[i*2] = mapping->pairs[i].gc;
		mapdata[i*2 + 1] = mapping->pairs[i].n64;
	}

	printf("Map data : ");
	printHexBuf(mapdata, maplen);

	togo = maplen;
	done = 0;
	chunk = 0;
	while (togo) {
		int len;

		if (togo > 32) {
			len = 32;
		} else {
			len = togo;
		}

		buf[0] = 'R';
		buf[1] = 0x03; // set mapping
		buf[2] = chunk;
		memcpy(buf + 3, mapdata + done, len);
		done+= len;

//		printf("Mapping chunk : ");
//		printHexBuf(buf, len + 2);

		n = gcn64lib_rawSiCommand(hdl, channel, buf, len + 3, buf, 1);
		if (n<0) {
			return n;
		}
		if (n != 1) {
			fprintf(stderr, "Communication error setting mapping\n");
			return -1;
		}

		togo -= len;
		chunk++;
	}

	return 0;
}

int gc2n64_adapter_getMapping(gcn64_hdl_t hdl, int channel, int mapping_id, struct gc2n64_adapter_mapping *dst_mapping)
{
	unsigned char buf[64];
	unsigned char cmd[4];
	int n;
	int mapping_size;
	int togo;

	cmd[0] = 'R';
	cmd[1] = 0x02; // Get mapping
	cmd[2] = mapping_id;
	cmd[3] = 0; // chunk 0 (size)

	n = gcn64lib_rawSiCommand(hdl, channel, cmd, 4, buf, 1);
	if (n<0)
		return n;

	if (n == 1) {
		int i, pos;
		mapping_size = buf[0];
//		printf("Mapping %d size: %d\n", mapping_id, mapping_size);

		togo = mapping_size;
		for (pos=0, i=0; pos<mapping_size; i++) {
			cmd[0] = 'R';
			cmd[1] = 0x02; // Get mapping
			cmd[2] = mapping_id;
			cmd[3] = i+1; // chunk 1 is first 32 byte block, 2nd is next 32 bytes, etc
//			printf("Getting block %d\n", i+1);
			n = gcn64lib_rawSiCommand(hdl, channel, cmd, 4, buf + pos, togo > 32 ? 32 : togo);
			if (n<0) {
				return n;
			}
//			printf("ret: %d\n", n);
			if (n==0)
				break;
			pos += n;
			togo -= n;
		}

		//printf("Received %d bytes\n", pos);
		if (n%2) {
			fprintf(stderr, "Error: Odd length mapping received\n");
			printHexBuf(buf, pos);
			return -1;
		}

		// TODO : Decode this to dst_mapping
		dst_mapping->n_pairs = pos/2;
		for (i=0; i<dst_mapping->n_pairs; i++) {
			dst_mapping->pairs[i].gc = buf[i*2];
			dst_mapping->pairs[i].n64 = buf[i*2+1];
		}
	}

	return 0;
}

const char *gc2n64_adapter_getMappingSlotName(unsigned char id, int default_context)
{
	switch (id)
	{
		case MAPPING_SLOT_BUILTIN_CURRENT:
			if (default_context) {
				return "[Built-in default]";
			} else {
				return "[Current mapping]";
			}
		case MAPPING_SLOT_DPAD_UP: return "[D-Pad UP]";
		case MAPPING_SLOT_DPAD_DOWN: return "[D-Pad DOWN]";
		case MAPPING_SLOT_DPAD_LEFT: return "[D-Pad LEFT]";
		case MAPPING_SLOT_DPAD_RIGHT: return "[D-Pad RIGHT]";
	}
	return "Invalid ID";
}

const char *gc2n64_adapter_getGCname(unsigned char id)
{
	const char *names[] = {
		"A","B","Z","Start",
		"L","R",
		"C-stick up (50% threshold)",
		"C-stick down (50% threshold)",
		"C-stick left (50% threshold)",
		"C-stick right (50% threshold)",
		"Dpad-up","Dpad-down","Dpad-left","Dpad-right",
		"Joystick left-right axis","Joystick up-down axis",
		// Extras
		"X","Y",
		"Joystick up (50% threshold)", "Joystick down (50% threshold)",
		"Joystick left (50% threshold)", "Joystick right (50% threshold)",
		"Analogic L slider (50% threshold)",
		"Analogic R slider (50% threshold)",
		"C-stick left-right axis","C-stick up-down axis",
	};

	if (id == 0xff)
		return "None";

	if (id < 0 || id >= ARRAY_SIZE(names)) {
		return "Error";
	}
	return names[id];
}

const char *gc2n64_adapter_getN64name(unsigned char id)
{
	const char *names[] = {
		"A","B","Z","Start","L","R",
		"C-up","C-down","C-left","C-right",
		"Dpad-up","Dpad-down","Dpad-left","Dpad-right",
		"Joystick left-right axis","Joystick up-down axis",
		"Joystick up", "Joystick down",
		"Joystick left", "Joystick right",
		"None"
	};

	if (id == 0xff)
		return "None";

	if (id < 0 || id >= ARRAY_SIZE(names)) {
		return "Error";
	}
	return names[id];
}

struct gc2n64_adapter_mapping *gc2n64_adapter_loadMapping(const char *srcfile)
{
	FILE *fptr;
	struct gc2n64_adapter_mapping *map = NULL;;
	char linebuf[64];
	int line = 0, pair = 0;

	fptr = fopen(srcfile, "r");
	if (!fptr) {
		perror("fopen");
		return NULL;
	}

	map = malloc(sizeof(struct gc2n64_adapter_mapping));
	if (!map) {
		perror("malloc");
		goto err;
	}

	do {
		if (fgets(linebuf, sizeof(linebuf), fptr)) {
			int gc, n64, n;
			line++;

			if (line == 1) {
				const char *magic = "# gc2n64 mapping";
				if (strncmp(magic, linebuf, strlen(magic))) {
					fprintf(stderr, "Does not appear to be a valid mapping file\n");
					goto err;
				}
				continue;
			}

			n = sscanf(linebuf, "%03d;%03d", &gc, &n64);
			if (n != 2) {
	//			printf("Ignoring line %d\n", line);
			} else {
	//			printf("%d -> %d\n", gc, n64);
				map->pairs[pair].gc = gc;
				map->pairs[pair].n64 = n64;

				pair++;
				if (pair >= GC2N64_MAX_MAPPING_PAIRS) {
					fprintf(stderr, "too many pairs, cannot load mapping.\n");
					goto err;
				}
			}
		}
	} while (!feof(fptr));

	map->n_pairs = pair;

	fclose(fptr);
	return map;

err:
	if (map) {
		free(map);
	}
	fclose(fptr);
	return NULL;
}

int gc2n64_adapter_saveMapping(struct gc2n64_adapter_mapping *map, const char *dstfile)
{
	FILE *fptr;
	int i;

	fptr = fopen(dstfile, "w");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	fprintf(fptr, "# gc2n64 mapping\n");
	for (i=0; i<map->n_pairs; i++) {
		fprintf(fptr, "%03d;%03d # %s -> %s\n",
			map->pairs[i].gc, map->pairs[i].n64,
				gc2n64_adapter_getGCname(map->pairs[i].gc),
					gc2n64_adapter_getN64name(map->pairs[i].n64));
	}
	fflush(fptr);
	fclose(fptr);

	return 0;
}

void gc2n64_adapter_printMapping(struct gc2n64_adapter_mapping *map)
{
	int i;
	int is_default;

	for (i=0; i<map->n_pairs; i++) {
		// Do not display the terminator
		if (map->pairs[i].gc == 0xff || map->pairs[i].n64 == 0xff) {
			break;
		}

		/* 0 .. 15 is a 1:1 (same button name) mapping by default */
		if (map->pairs[i].gc < 16) {
			if (map->pairs[i].gc == map->pairs[i].n64) {
				is_default = 1;
			}
			else {
				is_default = 0;
			}
		}
		else {
			// 16 and above maps to NONE by default
			if (map->pairs[i].n64 == 20) {
				is_default = 1;
			} else {
				is_default = 0;
			}
		}

		if (!is_default) {
			printf("%s -> %s, ", gc2n64_adapter_getGCname(map->pairs[i].gc),
											gc2n64_adapter_getN64name(map->pairs[i].n64));
		}
	}
}

void gc2n64_adapter_printInfo(struct gc2n64_adapter_info *inf)
{
	int i;

	if (!inf->in_bootloader) {
		printf("gc_to_n64 adapter info: {\n");

		printf("\tDefault mapping id: %d (%s)\n", inf->app.default_mapping_id, gc2n64_adapter_getMappingSlotName(inf->app.default_mapping_id, 1) );
		printf("\tDeadzone enabled: %d\n", inf->app.deadzone_enabled);
		printf("\tOld v1.5 conversion: %d\n", inf->app.old_v1_5_conversion);
		printf("\tFirmware version: %s\n", inf->app.version);
		printf("\tUpgradable: %s\n", inf->app.upgradeable ? "Yes":"No (Atmega8)");
		for (i=0; i<GC2N64_NUM_MAPPINGS; i++) {
			printf("\tMapping %d (%-13s): { ", i, gc2n64_adapter_getMappingSlotName(i, 0));
			gc2n64_adapter_printMapping(&inf->app.mappings[i]);
			printf(" }\n");
		}
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
			inf->app.default_mapping_id = buf[1];
			inf->app.deadzone_enabled = buf[2];
			inf->app.old_v1_5_conversion = buf[3];
			inf->app.upgradeable = buf[9];
			inf->app.version[sizeof(inf->app.version)-1]=0;
			strncpy(inf->app.version, (char*)buf+10, sizeof(inf->app.version)-1);
		} else {
			inf->bootldr.mcu_page_size = buf[1];
			inf->bootldr.bootloader_start_address = buf[2] << 8 | buf[3];
			inf->bootldr.version[sizeof(inf->bootldr.version)-1]=0;
			strncpy(inf->bootldr.version, (char*)buf+10, sizeof(inf->bootldr.version)-1);
		}

		for (n=0; n<GC2N64_NUM_MAPPINGS; n++) {
			gc2n64_adapter_getMapping(hdl, channel, n, &inf->app.mappings[n]);
		}

	} else {
		printf("No answer (old version?)\n");
		return -1;
	}

	return 0;
}

int gc2n64_adapter_isBusy(gcn64_hdl_t hdl, int channel)
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

int gc2n64_adapter_waitNotBusy(gcn64_hdl_t hdl, int channel, int verbose)
{
	char spinner[4] = { '|','/','-','\\' };
	int busy, no_reply_count=0;
	int c=0;

	while ((busy = gc2n64_adapter_isBusy(hdl, channel)))
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
			if (gc2n64_adapter_waitNotBusy(hdl, channel, 1)) {
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
	const char *signature = "41d938a8-6f8a-11e5-a45e-001bfca3c593";

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

	// look for the signature somewhere in the file to make sure
	// this firmware is intended for this product
	if (!memmem(buf, max_addr + 1, signature, strlen(signature))) {
		fprintf(stderr, "Update aborted : Signature not found. This hex file is not for this adapter.\n");
		ret = -1;
		printHexBuf(buf + 0x1bde, 30);
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
		// Catch Atmega8 adapters programmed with a new firmware but without bootloader.
		if (!inf.app.upgradeable) {
			fprintf(stderr, "Error : This adapter is not upgradable. (i.e. No bootloader on Atmega8)\n");
			ret = -1;
			goto err;
		}

		printf("step [3/7] : Enter bootloader...\n");
		res = gc2n64_adapter_enterBootloader(hdl, channel);
		if (res < 0) {
			fprintf(stderr, "Failed to enter the bootloader\n");
			ret = -1;
			goto err;
		}

		// Re-read the info structure, as we will need the bootloader start address.
		res = gc2n64_adapter_getInfo(hdl, channel, &inf);
		if (res < 0) {
			fprintf(stderr, "Failed to read info after enterring bootloader\n");
			ret = -1;
			goto err;
		}
	}

	////////////////////
	printf("step [4/7] : Erase current firmware... "); fflush(stdout);
	gc2n64_adapter_boot_eraseAll(hdl, channel);

	if (gc2n64_adapter_waitNotBusy(hdl, channel, 1)) {
		ret = -1;
		goto err;
	}
	printf("Ok\n");


	printf("step [5/7] : Write new firmware...\n");
	// Note: We write up to the bootloader, even if the firmware was shorter (it usually is).
	// This is to make sure that the marker we placed at the end gets written.
	res = gc2n64_adapter_sendFirmwareBlocks(hdl, channel, buf, inf.bootldr.bootloader_start_address);
	if (res < 0) {
		ret = -1;
		goto err;
	}

	printf("step [6/7] : Verify firmware...\n");
	res = gc2n64_adapter_verifyFirmware(hdl, channel, buf, inf.bootldr.bootloader_start_address);
	if (res < 0) {
		printf("Verify failed : Update failed\n");
		ret = -1;
		goto err;
	}

	printf("step [7/7] : Launch new firmware.\n");
	gc2n64_adapter_bootApplication(hdl, channel);

err:
	free(buf);
	return ret;
}


