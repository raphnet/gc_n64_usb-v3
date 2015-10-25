#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mempak.h"

#define DEXDRIVE_DATA_OFFSET	0x1040

mempak_structure_t *mempak_new(void)
{
	mempak_structure_t *mpk;

	mpk = calloc(1, sizeof(mempak_structure_t));
	if (!mpk) {
		perror("calloc");
		return NULL;
	}

	format_mempak(mpk);

	return mpk;
}

int mempak_exportNote(mempak_structure_t *mpk, int note_id, const char *dst_filename)
{
	FILE *fptr;
	entry_structure_t note_header;
	unsigned char databuf[0x10000];

	if (!mpk)
		return -1;

	if (0 != get_mempak_entry(mpk, note_id, &note_header)) {
		fprintf(stderr, "Error accessing note\n");
		return -1;
	}

	if (!note_header.valid) {
		fprintf(stderr, "Invaid note\n");
		return -1;
	}

	if (0 != read_mempak_entry_data(mpk, &note_header, databuf)) {
		fprintf(stderr, "Error accessing note data\n");
		return -1;
	}

	fptr = fopen(dst_filename, "w");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	/* For compatibility with bryc's javascript mempak editor[1], I set
	 * the inode number to 0xCAFE.
	 *
	 * [1] https://github.com/bryc/mempak
	 */
	note_header.raw_data[0x07] = 0xCA;
	note_header.raw_data[0x08] = 0xFE;

	fwrite(note_header.raw_data, 32, 1, fptr);
	fwrite(databuf, MEMPAK_BLOCK_SIZE, note_header.blocks, fptr);
	fclose(fptr);

	return 0;
}

int mempak_saveToFile(mempak_structure_t *mpk, const char *dst_filename, unsigned char format)
{
	FILE *fptr;

	if (!mpk)
		return -1;

	fptr = fopen(dst_filename, "w");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	switch(format)
	{
		default:
			fclose(fptr);
			return -1;

		case MPK_SAVE_FORMAT_MPK:
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			break;

		case MPK_SAVE_FORMAT_MPK4:
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			break;

		case MPK_SAVE_FORMAT_N64:
			// Note: This should work well for files that will
			// be imported by non-official software which typically
			// only look for the 123-456-STD header and then
			// seek to the data.
			//
			// Real .N64 files contain more info. TODO: Support it
			fprintf(fptr, "123-456-STD");
			fseek(fptr, DEXDRIVE_DATA_OFFSET, SEEK_SET);
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			break;
	}

	return 0;
}

mempak_structure_t *mempak_loadFromFile(const char *filename)
{
	FILE *fptr;
	long file_size;
	int i;
	int num_images = -1;
	long offset = 0;
	mempak_structure_t *mpk;

	mpk = calloc(1, sizeof(mempak_structure_t));
	if (!mpk) {
		perror("calloc");
		return NULL;
	}

	fptr = fopen(filename, "rb");
	if (!fptr) {
		perror("fopen");
		free(mpk);
		return NULL;
	}

	fseek(fptr, 0, SEEK_END);
	file_size = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);

	printf("File size: %ld bytes\n", file_size);

	/* Raw binary images. Those can contain more than one card's data. For
	 * instance, Mupen64 seems to contain four saves. (I suppose each 32kB block is
	 * for the virtual mempak of one controller) */
	for (i=0; i<4; i++) {
		if (file_size == 0x8000*i) {
			num_images = i+1;
			printf("MPK file Contains %d image(s)\n", num_images);
			mpk->source = MPK_SRC_RAW_IMAGE;
		}
	}

	if (num_images < 0) {
		char header[11];
		char *magic = "123-456-STD";
		/* If the size is not a fixed multiple, it could be a .N64 file */
		fread(header, 11, 1, fptr);
		if (0 == memcmp(header, magic, sizeof(header))) {
			printf(".N64 file detected\n");
			// TODO : Extract comments and other info. from the header
			offset = DEXDRIVE_DATA_OFFSET; // Thanks to N-Rage`s Dinput8 Plugin sources
			mpk->source = MPK_SRC_DEX_IMAGE;
		}
	}

	fseek(fptr, offset, SEEK_SET);
	fread(mpk->data, sizeof(mpk->data), 1, fptr);
	fclose(fptr);

	return mpk;
}

void mempak_free(mempak_structure_t *mpk)
{
	if (mpk)
		free(mpk);
}

