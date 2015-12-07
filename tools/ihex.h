#ifndef _ihex_h__
#define _ihex_h__

/* \return File size, or negative value on error.*/
int load_ihex(const char *file, unsigned char *dstbuf, int bufsize);

#endif // _ihex_h__

