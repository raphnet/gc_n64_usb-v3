#ifndef _mempak_h__
#define _mempak_h__

#define MEMPAK_NUM_NOTES	16

#define MPK_SRC_RAW_IMAGE	0
#define MPK_SRC_DEX_IMAGE	1

typedef struct mempak_structure
{
	unsigned char data[0x8000];
	unsigned char source;
} mempak_structure_t;

mempak_structure_t *mempak_new(void);
mempak_structure_t *mempak_loadFromFile(const char *filename);

#define MPK_SAVE_FORMAT_MPK		0
#define MPK_SAVE_FORMAT_MPK4	1 // MPK + 3 times 32kB padding
#define MPK_SAVE_FORMAT_N64		2
int mempak_saveToFile(mempak_structure_t *mpk, const char *dst_filename, unsigned char format);
int mempak_exportNote(mempak_structure_t *mpk, int note_id, const char *dst_filename);
void mempak_free(mempak_structure_t *mpk);

#include "mempak_fs.h"

#endif // _mempak_h__

