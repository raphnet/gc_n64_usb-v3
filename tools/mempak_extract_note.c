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
#include <stdlib.h>
#include "mempak.h"

int main(int argc, char **argv)
{
	const char *infile;
	const char *outfile;
	int note_id;
	mempak_structure_t *mpk;

	if (argc < 4) {
		printf("Usage: ./mempak_extract_note in_file note_id out_file\n");
		printf("\n");
		printf("Where:\n");
		printf("  in_file     The input file (full mempak image .mpk/.n64)\n");
		printf("  note_id     The id of the note (as shown by mempak_ls)\n");
		printf("  out_file    The output filename (eg: abc.note)\n");
		return 1;
	}

	infile = argv[1];
	note_id = atoi(argv[2]);
	outfile = argv[3];

	mpk = mempak_loadFromFile(infile);
	if (!mpk) {
		return 1;
	}

	if (mempak_exportNote(mpk, note_id, outfile)) {
		fprintf(stderr, "could not export note\n");
	}

	printf("Exported note %d to file '%s'\n", note_id, outfile);

	mempak_free(mpk);

	return 0;
}
