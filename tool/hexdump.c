#include <stdio.h>

void printHexBuf(unsigned char *buf, int n)
{
	int i;

	for (i=0; i<n; i++) {
		printf("%02x ", buf[i]);
	}
	printf("\n");
}


