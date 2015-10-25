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

	printf("Mempak image loaded. Image type %d\n", mpk->file_format);

	if (0 != validate_mempak(mpk)) {
		printf("Mempak invalid (not formatted or corrupted)\n");
		goto done;
	}

	printf("Mempak content is valid\n");

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
				printf("Invalid\n");
			}

		}
	}

done:
	mempak_free(mpk);
	return 0;
}
