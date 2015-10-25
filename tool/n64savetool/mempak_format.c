#include <stdio.h>
#include "mempak.h"


int main(int argc, char **argv)
{
	mempak_structure_t *mpk;
	const char *outfile;
	unsigned char type;

	if (argc < 2) {
		printf("Usage: ./mempak_format file\n");
		printf("\n");
		printf("Raw files (.MPK, .BIN) and Dexdrive (.N64) formats accepted.\n");
		return 1;
	}

	outfile = argv[1];
	mpk = mempak_new();
	if (!mpk) {
		return 1;
	}

	type = MPK_FORMAT_N64;
	mempak_saveToFile(mpk, outfile, type);

	mempak_free(mpk);

	return 0;
}
