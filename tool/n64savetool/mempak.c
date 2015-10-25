#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "mempak.h"

#define DEXDRIVE_DATA_OFFSET	0x1040
#define DEXDRIVE_COMMENT_OFFSET	0x40

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

static int mempak_findFreeNote(mempak_structure_t *mpk, entry_structure_t *entry_data, int *note_id)
{
	int i;

	if (!mpk)
		return -1;
	if (!entry_data)
		return -1;

	for (i=0; i<MEMPAK_NUM_NOTES; i++) {

		if (0 != get_mempak_entry(mpk, i, entry_data)) {
			return -1;
		}

		if (!entry_data->valid) {
			if (note_id) {
				*note_id = i;
			}
			return 0;
		}
	}

	return -1;
}

/**
 * \param mpk The memory pack to operate on
 * \param notefile The filename of the note to load
 * \param dst_note_id 0-15: (Over)write to specific note, -1: auto (first free)
 * \param note_id Stores the id of the note that was used
 */
int mempak_importNote(mempak_structure_t *mpk, const char *notefile, int dst_note_id, int *note_id)
{
	int free_blocks = get_mempak_free_space(mpk);
	FILE *fptr;
	unsigned char entry_data[32];
	unsigned char *data = NULL;
	entry_structure_t entry;
	int res;

	if (dst_note_id < -1 || dst_note_id > 15) {
		fprintf(stderr, "Out of bound dst_note_id\n");
		return -1;
	}

	printf("Current free blocks: %d\n", free_blocks);

	fptr = fopen(notefile, "rb");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	if (1 != fread(entry_data, 32, 1, fptr)) {
		perror("fread");
		fclose(fptr);
		return -1;
	}

	/* I follow the same convention as bryc's javascript mempak editor[1]
	 * by looking for an inode number of 0xCAFE.
 	 *
	 * [1] https://github.com/bryc/mempak
	 *
	 * If there are other note formats to support, this will need updating.
	 */
	if ((entry_data[0x06] == 0xCA) &&
		(entry_data[0x07] == 0xFE))
	{
		entry_structure_t oldentry;
		long filesize;

		// Fixup the inode number (0xCAFE is invalid)
		entry_data[0x06] = 0x00;
		entry_data[0x07] = 0x05; // BLOCK_VALID_FIRST;

		if (0 != mempak_parse_entry(entry_data, &entry)) {
			fprintf(stderr, "Error loading note (invalid)\n");
			fclose(fptr);
			return -1;
		}

		fseek(fptr, 0, SEEK_END);
		filesize = ftell(fptr);
		fseek(fptr, 32, SEEK_SET);

		// Remove the note header
		filesize -= 32;
		if (filesize % MEMPAK_BLOCK_SIZE) {
			fprintf(stderr, "Invalid note file size\n");
			fclose(fptr);
			return -1;
		}

		entry.blocks = filesize / MEMPAK_BLOCK_SIZE;

		printf("Note size: %d blocks\n", entry.blocks);
		printf("Note name: %s\n", entry.name);

		if (entry.blocks > free_blocks) {
			fprintf(stderr, "Not enough space (note is %d blocks and only %d free blocks in mempak)\n",
				entry.blocks, free_blocks);
			fclose(fptr);
			return -1;
		}

		data = calloc(1, entry.blocks * MEMPAK_BLOCK_SIZE);
		if (!data) {
			perror("calloc");
			fclose(fptr);
			return -1;
		}

		if (1 != fread(data, entry.blocks * MEMPAK_BLOCK_SIZE, 1, fptr)) {
			perror("could not load note data");
			free(data);
			fclose(fptr);
			return -1;
		}

		if (dst_note_id == -1) { // Auto (first free note)
			if (0 != mempak_findFreeNote(mpk, &oldentry, note_id)) {
				fprintf(stderr, "Could not find an empty note\n");
				free(data);
				fclose(fptr);
				return -1;
			}
		} else { // Specific note
			get_mempak_entry(mpk, dst_note_id, &oldentry);
			if (oldentry.valid) {
				printf("Overwriting note %d\n", dst_note_id);
				delete_mempak_entry(mpk, &oldentry);
			} else {
				fprintf(stderr, "No note id %d\n", dst_note_id);
				free(data);
				fclose(fptr);
				return -1;
			}
			if (note_id)
				*note_id = dst_note_id;
		}

		res = write_mempak_entry_data(mpk, &entry, data);
		if (res != 0) {
			fprintf(stderr, "Failed to write note (error %d)\n", res);
			free(data);
			fclose(fptr);
			return -1;
		}

		return 0;
	} else {
		fprintf(stderr, "Input file does not appear to be in a supported format.\n");
	}

	if (data)
		free(data);
	fclose(fptr);
	return -1;
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

	fptr = fopen(dst_filename, "wb");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	/* For compatibility with bryc's javascript mempak editor[1], I set
	 * the inode number to 0xCAFE.
	 *
	 * [1] https://github.com/bryc/mempak
	 */
	note_header.raw_data[0x06] = 0xCA;
	note_header.raw_data[0x07] = 0xFE;

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

	fptr = fopen(dst_filename, "wb");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	switch(format)
	{
		default:
			fclose(fptr);
			return -1;

		case MPK_FORMAT_MPK:
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			break;

		case MPK_FORMAT_MPK4:
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			fwrite(mpk->data, sizeof(mpk->data), 1, fptr);
			break;

		case MPK_FORMAT_N64:
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
	for (i=1; i<=4; i++) {
		if (file_size == 0x8000*i) {
			num_images = i;
			printf("MPK file Contains %d image(s)\n", num_images);
			if (file_size == 0x8000) {
				mpk->file_format = MPK_FORMAT_MPK;
			} else {
				mpk->file_format = MPK_FORMAT_MPK4;
			}
		}
	}

	if (num_images < 0) {
		char header[11];
		char *magic = "123-456-STD";
		/* If the size is not a fixed multiple, it could be a .N64 file */
		fread(header, 11, 1, fptr);
		if (0 == memcmp(header, magic, sizeof(header))) {
			printf(".N64 file detected\n");

			/* At 0x40 there are often comments in .N64 files.
			 * The actual memory card data starts at 0x1040.
			 * This means there are exactly 0x1000 bytes for
			 * one large comment, or, since 0x1000 / 256 = 16,
			 * more likely one comment per note? That's what
			 * I'm assuming here. */
			fseek(fptr, DEXDRIVE_COMMENT_OFFSET, SEEK_SET);
#if MAX_NOTE_COMMENT_SIZE != 257
#error
#endif
			for (i=0; i<16; i++) {
				fread(mpk->note_comments[i], 256, 1, fptr);
				/* The comments appear to be zero terminated, but I don't
				 * know if the original tool allowed entering a maximum
				 * of 256 or 255 bytes. So to be safe, I use buffers of
				 * 257 bytes */
				mpk->note_comments[i][256] = 0;
			}

			offset = DEXDRIVE_DATA_OFFSET;
			mpk->file_format = MPK_FORMAT_N64;
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

const char *mempak_format2string(int fmt)
{
	switch(fmt)
	{
		case MPK_FORMAT_MPK: return "MPK";
		case MPK_FORMAT_MPK4: return "MPK4";
		case MPK_FORMAT_N64: return "N64";
		case MPK_FORMAT_INVALID: return "Invalid";
		default:
			return "Unknown";
	}
}

int mempak_string2format(const char *str)
{
	if (0 == strcasecmp(str, "mpk"))
		return MPK_FORMAT_MPK;

	if (0 == strcasecmp(str, "mpk4"))
		return MPK_FORMAT_MPK4;

	if (0 == strcasecmp(str, "n64"))
		return MPK_FORMAT_N64;

	return MPK_FORMAT_INVALID;
}

