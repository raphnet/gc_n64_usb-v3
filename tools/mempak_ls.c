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
#include <stdio.h>
#include <string.h>
#include "mempak.h"

int main(int argc, char **argv)
{
	const char *infile;
	mempak_structure_t *mpk;
	int note;
	int res;

	if (argc < 2) {
		printf("Usage: ./mempak_ls file\n");
		printf("\n");
		printf("Raw files (.MPK, .BIN) and Dexdrive (.N64) formats accepted.\n");
		return 1;
	}

	infile = argv[1];
	mpk = mempak_loadFromFile(infile);
	if (!mpk) {
		return 1;
	}

	printf("Mempak image loaded. Image type %d (%s)\n", mpk->file_format, mempak_format2string(mpk->file_format));

	if (0 != validate_mempak(mpk)) {
		printf("Mempak invalid (not formatted or corrupted)\n");
		goto done;
	}

	printf("Mempak content is valid\n");
	printf("Block usage: %d / %d\n", 123-get_mempak_free_space(mpk), 123);

	for (note = 0; note<MEMPAK_NUM_NOTES; note++) {
		entry_structure_t note_data;

		printf("Note %d: ", note);
		res = get_mempak_entry(mpk, note, &note_data);
		if (res) {
			printf("Error!\n");
		} else {
			if (note_data.valid) {
				printf("%s (%d blocks) ", note_data.name, note_data.blocks);
//				printf("%08x ", note_data.vendor);
//				printf("%04x ", note_data.game_id);
//				printf("%02x ", note_data.region);
				if (strlen(mpk->note_comments[note]) > 0) {
					printf("{ %s }", mpk->note_comments[note]);
				}
				printf("\n");
			} else {
				printf("Free\n");
			}

		}
	}

done:
	mempak_free(mpk);
	return 0;
}
