#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "mempak.h"

static void print_usage(void)
{
	printf("Usage: ./mempak_insert_note pakfile notefile\n");
	printf("\n");
	printf("Options:\n");
	printf("   -h, --help                   Display help\n");
	printf("   -c, --comment Comment        Note comment (only works for .N64 files)\n");
	printf("   -d, --dst id                 Overwrite a specific note slot (0-15)\n");
}


int main(int argc, char **argv)
{
	const char *pakfile;
	const char *notefile;
	mempak_structure_t *mpk;
	struct option long_options[] = {
		{ "help", no_argument, 0, 'h' },
		{ "comment", required_argument, 0, 'c' },
		{ "dst", required_argument, 0, 'd' },
		{ }, // terminator
	};
	const char *comment = "";
	int used_note_id = -1;
	int dst_id = -1;

	if (argc < 3) {
		print_usage();
		return 1;
	}

	while(1) {
		int c;

		c = getopt_long(argc, argv, "f:h", long_options, NULL);
		if (c==-1)
			break;

		switch(c)
		{
			case 'h':
				print_usage();
				return 0;
			case 'c':
				comment = optarg;
				break;
			case 'd':
				dst_id = atoi(optarg);
				break;
		}
	}


	pakfile = argv[optind];
	notefile = argv[optind+1];

	mpk = mempak_loadFromFile(pakfile);
	if (!mpk) {
		return 1;
	}
	printf("Loaded pakfile in %s format.\n", mempak_format2string(mpk->file_format));

	if (mempak_importNote(mpk, notefile, dst_id, &used_note_id)) {
		fprintf(stderr, "could not export note\n");
		mempak_free(mpk);
		return 0;
	}

	printf("Note imported and written to slot %d\n", used_note_id);

	if (0 != mempak_saveToFile(mpk, pakfile, mpk->file_format)) {
		fprintf(stderr, "could not write to memory pak file\n");
	}

	mempak_free(mpk);

	return 0;
}
