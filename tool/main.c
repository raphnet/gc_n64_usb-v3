/* wusbmote: Wiimote accessory to USB Adapter
 * Copyright (C) 2012-2014 Raphaël Assénat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The author may be contacted at raph@raphnet.net
 */
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>

#include "hexdump.h"
#include "version.h"
#include "gcn64.h"
#include "gcn64lib.h"
#include "gc2n64_adapter.h"
#include "mempak.h"
#include "../requests.h"
#include "../gcn64_protocol.h"

static void printUsage(void)
{
	printf("./gcn64_ctl [OPTION]... [COMMAND]....\n");
	printf("Control tool for WUSBmote adapter. Version %s\n", VERSION_STR);
	printf("\n");
	printf("Options:\n");
	printf("  -h, --help            Print help\n");
	printf("  -l, --list            List devices\n");
	printf("  -s serial             Operate on specified device (required unless -f is specified)\n");
	printf("  -f, --force           If no serial is specified, use first device detected.\n");
	printf("  -o, --outfile file    Output file for read operations (eg: --n64-mempak-dump)\n");
	//printf("  -i, --infile file     Input file for write operations (eg: --gc_to_n64_update)\n");
	printf("      --nonstop         Continue testing forever or until an error occurs.\n");
	printf("\n");
	printf("Configuration commands:\n");
	printf("  --get_version                      Read adapter firmware version\n");
	printf("  --set_serial serial                Assign a new device serial number\n");
	printf("  --get_serial                       Read serial from eeprom\n");
	printf("  --set_poll_rate ms                 Set time between controller polls in milliseconds\n");
	printf("  --get_poll_rate                    Read configured poll rate\n");
	printf("\n");
	printf("Advanced commands:\n");
	printf("  --bootloader                       Re-enumerate in bootloader mode\n");
	printf("  --suspend_polling                  Stop polling the controller\n");
	printf("  --resume_polling                   Re-start polling the controller\n");
	printf("\n");
	printf("Raw controller commands:\n");
	printf("  --n64_getstatus                    Read N64 controller status now\n");
	printf("  --gc_getstatus                     Read GC controller status now (turns rumble OFF)\n");
	printf("  --gc_getstatus_rumble              Read GC controller status now (turns rumble ON)\n");
	printf("  --n64_getcaps                      Get N64 controller capabilities (or status such as pak present)\n");
	printf("  --n64_mempak_dump                  Dump N64 mempak contents (Use with --outfile to write to file)\n");
	printf("  --n64_mempak_write file            Write file to N64 mempak\n");
	printf("\n");
	printf("GC to N64 adapter commands: (For GC to N64 adapter connected to GC/N64 to USB adapter)\n");
	printf("  --gc_to_n64_info                   Display info on adapter (version, config, etc)\n");
	printf("  --gc_to_n64_update file.hex        Update GC to N64 adapter firmware\n");
	printf("  --gc_to_n64_read_mapping id        Dump a mapping (Use with --outfile to write to file)\n");
	printf("  --gc_to_n64_load_mapping file      Load a mapping from a file and send it to the adapter\n");
	printf("  --gc_to_n64_store_current_mapping slot    Store the current mapping to one of the D-Pad slots.\n");
	printf("\n");
	printf("GC to N64 adapter, development/debug commands:\n");
	printf("  --gc_to_n64_echotest               Perform a communication test (usable with --nonstop)\n");
	printf("  --gc_to_n64_dump                   Display the firmware content in hex.\n");
	printf("  --gc_to_n64_enter_bootloader       Jump to the bootloader.\n");
	printf("  --gc_to_n64_boot_application       Exit bootloader and start application.\n");
	printf("\n");
	printf("Development/Experimental/Research commands: (use at your own risk)\n");
	printf("  --si_8bit_scan                     Try all possible 1-byte commands, to see which one a controller responds to.\n");
	printf("  --si_16bit_scan                    Try all possible 2-byte commands, to see which one a controller responds to.\n");
}


#define OPT_OUTFILE					'o'
#define OPT_INFILE					'i'
#define OPT_SET_SERIAL				257
#define OPT_GET_SERIAL				258
#define OPT_BOOTLOADER				300
#define OPT_N64_GETSTATUS			301
#define OPT_GC_GETSTATUS			302
#define OPT_GC_GETSTATUS_RUMBLE		303
#define OPT_N64_MEMPAK_DUMP			304
#define OPT_N64_GETCAPS				305
#define OPT_SUSPEND_POLLING			306
#define OPT_RESUME_POLLING			307
#define OPT_SET_POLL_INTERVAL			308
#define OPT_GET_POLL_INTERVAL			309
#define OPT_N64_MEMPAK_WRITE			310
#define OPT_SI8BIT_SCAN					311
#define OPT_SI16BIT_SCAN				312
#define OPT_GC_TO_N64_INFO				313
#define OPT_GC_TO_N64_TEST				314
#define OPT_GC_TO_N64_UPDATE			315
#define OPT_GC_TO_N64_DUMP				316
#define OPT_GC_TO_N64_ENTER_BOOTLOADER	317
#define OPT_GC_TO_N64_BOOT_APPLICATION	318
#define OPT_NONSTOP						319
#define OPT_GC_TO_N64_READ_MAPPING		320
#define OPT_GC_TO_N64_LOAD_MAPPING		321
#define OPT_GC_TO_N64_STORE_CURRENT_MAPPING	322
#define OPT_GET_VERSION				323

struct option longopts[] = {
	{ "help", 0, NULL, 'h' },
	{ "list", 0, NULL, 'l' },
	{ "force", 0, NULL, 'f' },
	{ "set_serial", 1, NULL, OPT_SET_SERIAL },
	{ "get_serial", 0, NULL, OPT_GET_SERIAL },
	{ "bootloader", 0, NULL, OPT_BOOTLOADER },
	{ "n64_getstatus", 0, NULL, OPT_N64_GETSTATUS },
	{ "gc_getstatus", 0, NULL, OPT_GC_GETSTATUS },
	{ "gc_getstatus_rumble", 0, NULL, OPT_GC_GETSTATUS_RUMBLE },
	{ "n64_getcaps", 0, NULL, OPT_N64_GETCAPS },
	{ "n64_mempak_dump", 0, NULL, OPT_N64_MEMPAK_DUMP },
	{ "suspend_polling", 0, NULL, OPT_SUSPEND_POLLING },
	{ "resume_polling", 0, NULL, OPT_RESUME_POLLING },
	{ "outfile", 1, NULL, OPT_OUTFILE },
	{ "infile", 1, NULL, OPT_INFILE },
	{ "set_poll_rate", 1, NULL, OPT_SET_POLL_INTERVAL },
	{ "get_poll_rate", 0, NULL, OPT_GET_POLL_INTERVAL },
	{ "n64_mempak_write", 1, NULL, OPT_N64_MEMPAK_WRITE },
	{ "si_8bit_scan", 0, NULL, OPT_SI8BIT_SCAN },
	{ "si_16bit_scan", 0, NULL, OPT_SI16BIT_SCAN },
	{ "gc_to_n64_info", 0, NULL, OPT_GC_TO_N64_INFO },
	{ "gc_to_n64_echotest", 0, NULL, OPT_GC_TO_N64_TEST },
	{ "gc_to_n64_update", 1, NULL, OPT_GC_TO_N64_UPDATE },
	{ "gc_to_n64_dump", 0, NULL, OPT_GC_TO_N64_DUMP },
	{ "gc_to_n64_enter_bootloader", 0, NULL, OPT_GC_TO_N64_ENTER_BOOTLOADER },
	{ "gc_to_n64_boot_application", 0, NULL, OPT_GC_TO_N64_BOOT_APPLICATION },
	{ "gc_to_n64_read_mapping", 1, NULL, OPT_GC_TO_N64_READ_MAPPING },
	{ "gc_to_n64_load_mapping", 1, NULL, OPT_GC_TO_N64_LOAD_MAPPING },
	{ "gc_to_n64_store_current_mapping", 1, NULL, OPT_GC_TO_N64_STORE_CURRENT_MAPPING },
	{ "nonstop", 0, NULL, OPT_NONSTOP },
	{ "get_version", 0, NULL, OPT_GET_VERSION },
	{ },
};

static int listDevices(void)
{
	int n_found = 0;
	struct gcn64_list_ctx *listctx;
	struct gcn64_info inf;

	listctx = gcn64_allocListCtx();
	if (!listctx) {
		fprintf(stderr, "List context could not be allocated\n");
		return -1;
	}
	while (gcn64_listDevices(&inf, listctx))
	{
		n_found++;
		printf("Found device '%ls', serial '%ls'\n", inf.str_prodname, inf.str_serial);
	}
	gcn64_freeListCtx(listctx);
	printf("%d device(s) found\n", n_found);

	return n_found;
}

int main(int argc, char **argv)
{
	gcn64_hdl_t hdl;
	struct gcn64_list_ctx *listctx;
	int opt, retval = 0;
	struct gcn64_info inf;
	struct gcn64_info *selected_device = NULL;
	int verbose = 0, use_first = 0, serial_specified = 0;
	int nonstop = 0;
	int cmd_list = 0;
#define TARGET_SERIAL_CHARS 128
	wchar_t target_serial[TARGET_SERIAL_CHARS];
	const char *short_optstr = "hls:vfo:";
	const char *outfile = NULL;
	const char *infile = NULL;
	int gc2n64_channel = 0;

	while((opt = getopt_long(argc, argv, short_optstr, longopts, NULL)) != -1) {
		switch(opt)
		{
			case 's':
				{
					mbstate_t ps;
					memset(&ps, 0, sizeof(ps));
					if (mbsrtowcs(target_serial, (const char **)&optarg, TARGET_SERIAL_CHARS, &ps) < 1) {
						fprintf(stderr, "Invalid serial number specified\n");
						return -1;
					}
					serial_specified = 1;
				}
				break;
			case 'f':
				use_first = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'h':
				printUsage();
				return 0;
			case 'l':
				cmd_list = 1;
				break;
			case 'o':
				outfile = optarg;
				printf("Output file: %s\n", outfile);
				break;
			case 'i':
				infile = optarg;
				printf("Input file: %s\n", infile);
				break;
			case OPT_NONSTOP:
				nonstop = 1;
				break;
			case '?':
				fprintf(stderr, "Unrecognized argument. Try -h\n");
				return -1;
		}
	}

	gcn64_init(verbose);

	if (cmd_list) {
		printf("Simply listing the devices...\n");
		return listDevices();
	}

	if (!serial_specified && !use_first) {
		fprintf(stderr, "A serial number or -f must be used. Try -h for more information.\n");
		return 1;
	}

	listctx = gcn64_allocListCtx();
	while ((selected_device = gcn64_listDevices(&inf, listctx)))
	{
		if (serial_specified) {
			if (0 == wcscmp(inf.str_serial, target_serial)) {
				break;
			}
		}
		else {
			// use_first == 1
			printf("Will use device '%ls' serial '%ls'\n", inf.str_prodname, inf.str_serial);
			break;
		}
	}
	gcn64_freeListCtx(listctx);

	if (!selected_device) {
		if (serial_specified) {
			fprintf(stderr, "Device not found\n");
		} else {
			fprintf(stderr, "No device found\n");
		}
		return 1;
	}

	hdl = gcn64_openDevice(selected_device);
	if (!hdl) {
		printf("Error opening device. (Do you have permissions?)\n");
		return 1;
	}

	printf("Ready.\n");

	optind = 1;
	while((opt = getopt_long(argc, argv, short_optstr, longopts, NULL)) != -1)
	{
		unsigned char cmd[64] = { };
		int n;
		int cmdlen = 0;
		int do_exchange = 0;

		switch (opt)
		{
			case OPT_SET_POLL_INTERVAL:
				cmd[0] = atoi(optarg);
				printf("Setting poll interval to %d ms\n", cmd[0]);
				gcn64lib_setConfig(hdl, CFG_PARAM_POLL_INTERVAL0, cmd, 1);
				break;

			case OPT_GET_POLL_INTERVAL:
				n = gcn64lib_getConfig(hdl, CFG_PARAM_POLL_INTERVAL0, cmd, sizeof(cmd));
				if (n == 1) {
					printf("Poll interval: %d ms\n", cmd[0]);
				}
				break;

			case OPT_SET_SERIAL:
				printf("Setting serial...");
				if (strlen(optarg) != 6) {
					fprintf(stderr, "Serial number must be 6 characters\n");
					return -1;
				}
				gcn64lib_setConfig(hdl, CFG_PARAM_SERIAL, (void*)optarg, 6);
				break;

			case OPT_GET_SERIAL:
				n = gcn64lib_getConfig(hdl, CFG_PARAM_SERIAL, cmd, sizeof(cmd));
				if (n==6) {
					cmd[6] = 0;
					printf("Serial: %s\n", cmd);
				}
				break;

			case OPT_BOOTLOADER:
				printf("Sending 'jump to bootloader' command...");
				gcn64lib_bootloader(hdl);
				break;

			case OPT_SUSPEND_POLLING:
				gcn64lib_suspendPolling(hdl, 1);
				break;

			case OPT_RESUME_POLLING:
				gcn64lib_suspendPolling(hdl, 0);
				break;

			case OPT_N64_GETSTATUS:
				cmd[0] = N64_GET_STATUS;
				n = gcn64lib_rawSiCommand(hdl, 0, cmd, 1, cmd, sizeof(cmd));
				if (n >= 0) {
					printf("N64 Get status[%d]: ", n);
					printHexBuf(cmd, n);
				}
				break;

			case OPT_GC_GETSTATUS_RUMBLE:
			case OPT_GC_GETSTATUS:
				cmd[0] = GC_GETSTATUS1;
				cmd[1] = GC_GETSTATUS2;
				cmd[2] = GC_GETSTATUS3(opt == OPT_GC_GETSTATUS_RUMBLE);
				n = gcn64lib_rawSiCommand(hdl, 0, cmd, 3, cmd, sizeof(cmd));
				if (n >= 0) {
					printf("GC Get status[%d]: ", n);
					printHexBuf(cmd, n);
				}
				break;

			case OPT_N64_GETCAPS:
				cmd[0] = N64_GET_CAPABILITIES;
				//cmd[0] = 0xff;
				n = gcn64lib_rawSiCommand(hdl, 0, cmd, 1, cmd, sizeof(cmd));
				if (n >= 0) {
					printf("N64 Get caps[%d]: ", n);
					printHexBuf(cmd, n);
				}
				break;

			case OPT_N64_MEMPAK_DUMP:
				if (outfile) {
					mempak_dumpToFile(hdl, outfile);
				} else {
					mempak_dump(hdl);
				}
				break;

			case OPT_N64_MEMPAK_WRITE:
				printf("Input file: %s\n", optarg);
				mempak_writeFromFile(hdl, optarg);
				break;

			case OPT_SI8BIT_SCAN:
				gcn64lib_8bit_scan(hdl, 0, 255);
				break;

			case OPT_SI16BIT_SCAN:
				gcn64lib_16bit_scan(hdl, 0, 0xffff);
				break;

			case OPT_GC_TO_N64_INFO:
				{
					struct gc2n64_adapter_info inf;

					gc2n64_adapter_getInfo(hdl, gc2n64_channel, &inf);
					gc2n64_adapter_printInfo(&inf);
				}
				break;

			case OPT_GC_TO_N64_TEST:
				{
					int i=0;

					do {
						n = gc2n64_adapter_echotest(hdl, gc2n64_channel, 1);
						if (n != 0) {
							printf("Test failed\n");
							return -1;
						}
						usleep(1000 * (i));
						i++;
						if (i>100)
							i=0;
					} while (nonstop);
					printf("Test ok\n");
				}
				break;

			case OPT_GC_TO_N64_UPDATE:
				gc2n64_adapter_updateFirmware(hdl, gc2n64_channel, optarg);
				break;

			case OPT_GC_TO_N64_DUMP:
				gc2n64_adapter_dumpFlash(hdl, gc2n64_channel);
				break;

			case OPT_GC_TO_N64_ENTER_BOOTLOADER:
				gc2n64_adapter_enterBootloader(hdl, gc2n64_channel);
				gc2n64_adapter_waitForBootloader(hdl, gc2n64_channel, 5);
				break;

			case OPT_GC_TO_N64_BOOT_APPLICATION:
				gc2n64_adapter_bootApplication(hdl, gc2n64_channel);
				break;

			case OPT_GC_TO_N64_READ_MAPPING:
				{
					struct gc2n64_adapter_info inf;
					int map_id;

					map_id = atoi(optarg);
					if ((map_id <= 0) || (map_id > GC2N64_NUM_MAPPINGS)) {
						fprintf(stderr, "Invalid mapping id (1 to 4)\n");
						return -1;
					}

					gc2n64_adapter_getInfo(hdl, gc2n64_channel, &inf);
					printf("Mapping %d : { ", map_id);
					gc2n64_adapter_printMapping(&inf.app.mappings[map_id-1]);
					printf(" }\n");
					if (outfile) {
						printf("Writing mapping to file '%s'\n", outfile);
						gc2n64_adapter_saveMapping(&inf.app.mappings[map_id-1], outfile);
					}
				}
				break;

			case OPT_GC_TO_N64_LOAD_MAPPING:
				{
					struct gc2n64_adapter_mapping *mapping;

					printf("Reading mapping from file '%s'\n", optarg);
					mapping = gc2n64_adapter_loadMapping(optarg);
					if (!mapping) {
						fprintf(stderr, "Failed to load mapping\n");
						return -1;
					}

					printf("Mapping : { ");
					gc2n64_adapter_printMapping(mapping);
					printf(" }\n");

					gc2n64_adapter_setMapping(hdl, gc2n64_channel, mapping);

					free(mapping);
				}
				break;

			case OPT_GC_TO_N64_STORE_CURRENT_MAPPING:
				{
					int slot;

					slot = atoi(optarg);

					if (slot < 1 || slot > 4) {
						fprintf(stderr, "Mapping out of range (1-4)\n");
						return -1;
					}

					if (0 == gc2n64_adapter_storeCurrentMapping(hdl, gc2n64_channel, slot)) {
						printf("Stored mapping to slot %d (%s)\n", slot, gc2n64_adapter_getMappingSlotName(slot, 0));
					} else {
						printf("Error storing mapping\n");
					}
				}
				break;

			case OPT_GET_VERSION:
				{
					char version[64];

					if (0 == gcn64lib_getVersion(hdl, version, sizeof(version))) {
						printf("Firmware version: %s\n", version);
					}
				}
				break;
		}

		if (do_exchange) {
			int i;
			n = gcn64_exchange(hdl, cmd, cmdlen, cmd, sizeof(cmd));
			if (n<0)
				break;

			printf("Result: %d bytes: ", n);
			for (i=0; i<n; i++) {
				printf("%02x ", cmd[i]);
			}
			printf("\n");
		}
	}

	gcn64_closeDevice(hdl);
	gcn64_shutdown();

	return retval;
}
