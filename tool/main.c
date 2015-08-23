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

#include "version.h"
#include "gcn64.h"
#include "../requests.h"
#include "../gcn64_protocol.h"

static void printUsage(void)
{
	printf("./gcn64_ctl [OPTION]... [COMMAND]....\n");
	printf("Control tool for WUSBmote adapter. Version %s\n", VERSION_STR);
	printf("\n");
	printf("Options:\n");
	printf("  -h, --help          Print help\n");
	printf("  -l, --list          List devices\n");
	printf("  -s serial           Operate on specified device (required unless -f is specified)\n");
	printf("  -f, --force         If no serial is specified, use first device detected.\n");
	printf("  -o, --outfile       Output file for read operations (eg: --n64-mempak-dump)\n");
	printf("\n");
	printf("Configuration commands:\n");
	printf("  --set_serial serial                Assign a new device serial number\n");
	printf("  --get_serial                       Read serial from eeprom\n");
	printf("\n");
	printf("Advanced commands:\n");
	printf("  --bootloader                       Re-enumerate in bootloader mode\n");
	printf("  --suspend_polling                  Stop polling controller\n");
	printf("\n");
	printf("Raw controller commands:\n");
	printf("  --n64_getstatus                    Read N64 controller status now\n");
	printf("  --gc_getstatus                     Read GC controller status now (turns rumble OFF)\n");
	printf("  --gc_getstatus_rumble              Read GC controller status now (turns rumble ON)\n");
	printf("  --n64_getcaps                      Get N64 controller capabilities (or status such as pak present)\n");
	printf("  --n64_mempak_dump                  Dump (display) N64 mempak contents\n");
}

#define OPT_OUTFILE					'o'
#define OPT_SET_SERIAL				257
#define OPT_GET_SERIAL				258
#define OPT_BOOTLOADER				300
#define OPT_N64_GETSTATUS			301
#define OPT_GC_GETSTATUS			302
#define OPT_GC_GETSTATUS_RUMBLE		303
#define OPT_N64_MEMPAK_DUMP			304
#define OPT_N64_GETCAPS				305
#define OPT_SUSPEND_POLLING			306

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
	{ "outfile", 0, NULL, OPT_OUTFILE },
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
	int cmd_list = 0;
#define TARGET_SERIAL_CHARS 128
	wchar_t target_serial[TARGET_SERIAL_CHARS];
	const char *short_optstr = "hls:vfo:";
	const char *outfile = NULL;

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

		switch (opt)
		{
			case OPT_SET_SERIAL:
				printf("Setting serial...");
				if (strlen(optarg) != 6) {
					fprintf(stderr, "Serial number must be 6 characters\n");
					return -1;
				}
				cmd[0] = RQ_GCN64_SET_CONFIG_PARAM;
				cmd[1] = CFG_PARAM_SERIAL;
				memcpy(cmd + 2, optarg, 6);
				cmdlen = 8;
				break;

			case OPT_GET_SERIAL:
				cmd[0] = RQ_GCN64_GET_CONFIG_PARAM;
				cmd[1] = CFG_PARAM_SERIAL;
				cmdlen = 2;
				break;

			case OPT_BOOTLOADER:
				printf("Sending 'jump to bootloader' command...");
				cmd[0] = RQ_GCN64_JUMP_TO_BOOTLOADER;
				cmdlen = 1;
				break;

			case OPT_SUSPEND_POLLING:
				cmd[0] = RQ_GCN64_SUSPEND_POLLING;
				cmdlen = 1;
				break;

			case OPT_N64_GETSTATUS:
				cmd[0] = RQ_GCN64_RAW_SI_COMMAND;
				cmd[1] = 0x01; // Length of SI command
				cmd[2] = N64_GET_STATUS; // N64 GET status
				cmdlen = 3;
				break;

			case OPT_GC_GETSTATUS:
				cmd[0] = RQ_GCN64_RAW_SI_COMMAND;
				cmd[1] = 0x03; // Length of SI command
				cmd[2] = GC_GETSTATUS1;
				cmd[3] = GC_GETSTATUS2;
				cmd[4] = GC_GETSTATUS3(0);
				cmdlen = 5;
				break;

			case OPT_GC_GETSTATUS_RUMBLE:
				cmd[0] = RQ_GCN64_RAW_SI_COMMAND;
				cmd[1] = 0x03; // Length of SI command
				cmd[2] = GC_GETSTATUS1;
				cmd[3] = GC_GETSTATUS2;
				cmd[4] = GC_GETSTATUS3(0);
				cmdlen = 5;
				break;

			case OPT_N64_GETCAPS:
				cmd[0] = RQ_GCN64_RAW_SI_COMMAND;
				cmd[1] = 0x01; // Length of SI command
				cmd[2] = N64_GET_CAPABILITIES; // N64 GET status
				cmdlen = 3;
				break;

			case OPT_N64_MEMPAK_DUMP:
				mempak_dump(hdl);
				break;
		}

		if (cmd[0]) {
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
