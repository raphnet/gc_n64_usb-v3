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
