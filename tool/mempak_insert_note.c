#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	const char *comment = NULL;
	int used_note_id = -1;
	int dst_id = -1;

	if (argc < 3) {
		print_usage();
		return 1;
	}

	while(1) {
		int c;

		c = getopt_long(argc, argv, "f:hc:d:", long_options, NULL);
		if (c==-1)
			break;

		switch(c)
		{
			case 'h':
				print_usage();
				return 0;
			case 'c':
				comment = optarg;
				if (strlen(optarg) > (MAX_NOTE_COMMENT_SIZE-2)) {
					fprintf(stderr, "Comment too long (%d characters max.)\n", (MAX_NOTE_COMMENT_SIZE-2));
					return -1;
				}
				break;
			case 'd':
				dst_id = atoi(optarg);
				break;
			case '?':
				fprintf(stderr, "Unknown argument. Try -h\n");
				return -1;
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

	if (comment) {
		if (mpk->file_format != MPK_FORMAT_N64) {
			printf("Warning: Ignoring comment since it cannot be stored in %s file format. Use the N64 format instead.\n", mempak_format2string(mpk->file_format));
		} else {
			strncpy(mpk->note_comments[used_note_id], comment, MAX_NOTE_COMMENT_SIZE);
			mpk->note_comments[used_note_id][256] = 0;
			mpk->note_comments[used_note_id][255] = 0;
		}
	}

	if (0 != mempak_saveToFile(mpk, pakfile, mpk->file_format)) {
		fprintf(stderr, "could not write to memory pak file\n");
	}

	mempak_free(mpk);

	return 0;
}
