#ifndef _mempak_h__
#define _mempak_h__

#define MEMPAK_NUM_NOTES	16

#define MAX_NOTE_COMMENT_SIZE	257 // including 0 termination

#define MPK_FORMAT_INVALID	0
#define MPK_FORMAT_MPK		1
#define MPK_FORMAT_MPK4		2 // MPK + 3 times 32kB padding
#define MPK_FORMAT_N64		3

typedef struct mempak_structure
{
	unsigned char data[0x8000];
	unsigned char file_format;

	char note_comments[MEMPAK_NUM_NOTES][MAX_NOTE_COMMENT_SIZE];
} mempak_structure_t;

mempak_structure_t *mempak_new(void);
mempak_structure_t *mempak_loadFromFile(const char *filename);

int mempak_saveToFile(mempak_structure_t *mpk, const char *dst_filename, unsigned char format);
int mempak_exportNote(mempak_structure_t *mpk, int note_id, const char *dst_filename);
int mempak_importNote(mempak_structure_t *mpk, const char *notefile, int dst_note_id, int *note_id);
void mempak_free(mempak_structure_t *mpk);

int mempak_string2format(const char *str);
const char *mempak_format2string(int fmt);

#include "mempak_fs.h"

#endif // _mempak_h__

