#define PROGMEM
#include <stdio.h>
#include <stdint.h>
#include "../reportdesc.c"


int main(void)
{
	int i;

	for (i=0; i<sizeof(gcn64_usbHidReportDescriptor); i++)
	{
		printf("%02x ", gcn64_usbHidReportDescriptor[i]);
	}
	printf("\n");

	return 0;
}
