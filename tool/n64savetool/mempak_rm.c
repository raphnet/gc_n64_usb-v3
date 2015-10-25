#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "mempak.h"

static void print_usage(void)
{
	printf("Usage: ./mempak_rm pakfile note_id\n");
	printf("\n");
	printf("Options:\n");
	printf("   -h, --help                   Display help\n");
}


int main(int argc, char **argv)
{
	const char *pakfile;
	mempak_structure_t *mpk;
	struct option long_options[] = {
		{ "help", no_argument, 0, 'h' },
		{ }, // terminator
	};
	int noteid;
	entry_structure_t entry;

	if (argc < 3) {
		print_usage();
		return 1;
	}

	while(1) {
		int c;

		c = getopt_long(argc, argv, "h", long_options, NULL);
		if (c==-1)
			break;

		switch(c)
		{
			case 'h':
				print_usage();
				return 0;
			case '?':
				fprintf(stderr, "Unknown argument. Try -h\n");
				return -1;
		}
	}

	pakfile = argv[optind];
	noteid = atoi(argv[optind+1]);

	if (noteid < 0 || noteid > 15) {
		fprintf(stderr, "Note id out of range (0-15)\n");
		return -1;
	}

	mpk = mempak_loadFromFile(pakfile);
	if (!mpk) {
		return -1;
	}
	printf("Loaded pakfile in %s format.\n", mempak_format2string(mpk->file_format));

	if (0 != get_mempak_entry(mpk, noteid, &entry)) {
		fprintf(stderr, "Could not get note id %d\n", noteid);
		mempak_free(mpk);
		return -1;
	}

	if (!entry.valid) {
		fprintf(stderr, "Note %d is already free\n", noteid);
		mempak_free(mpk);
		return -1;
	}

	printf("Deleting note %d (%d blocks)\n", noteid, entry.blocks);

	if (0 != delete_mempak_entry(mpk, &entry)) {
		fprintf(stderr, "Error deleting entry\n");
		mempak_free(mpk);
		return -1;
	}

	if (0 != mempak_saveToFile(mpk, pakfile, mpk->file_format)) {
		fprintf(stderr, "could not write to memory pak file\n");
	}

	mempak_free(mpk);

	return 0;
}
