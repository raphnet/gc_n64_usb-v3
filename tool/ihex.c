#include <stdio.h>
#include <string.h>
#include "hexdump.h"

static unsigned char chk(unsigned char *buf, int len)
{
	int i;
	unsigned char r = 0;

	for (i=0; i<len; i++) {
		r += buf[i];
	}
	return r;
}

/* \return The highest address written to, or negative on errors.
 */
int load_ihex(const char *file, unsigned char *dstbuf, int bufsize)
{
	FILE *fptr;
	char linebuf[550];
	unsigned char databuf[2+1+255+1];
	int ret = 0;
	int line = 0;
	int eof_seen = 0;
	unsigned int max_address = 0;
	unsigned int offset = 0;

	fptr = fopen(file, "r");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	do {
		if (fgets(linebuf, sizeof(linebuf), fptr)) {
			unsigned int data_count;
			unsigned int address;
			int input_nibbles, input_bytes;
			int i;

			line++;

			if (linebuf[0] != ':') {
				fprintf(stderr, "Ignored invalid line %d\n", line);
				continue;
			}

			if (eof_seen) {
				fprintf(stderr, "extra data after EOF record in hex file\n");
				ret = -7;
				goto err;
			}

			// :10 0000 00 92C064C7ABC0AAC0A9C0A8C0A7C0A6C0 00
			//  ^  ^    ^  ^                                ^-- Checksum
			//  |  |    |  +----- Data [data_count]
			//  |  |    +---- Record type
			//  |  +------ Address
			//  +----- data_count
			//

			input_nibbles = strlen(linebuf) - 1;

			for (input_bytes=0,i=0; i<input_nibbles; i+=2) {
				unsigned int byte;
				if (1 != sscanf(linebuf + 1 + i, "%02x", &byte)) {
					break;
				}
				databuf[input_bytes] = byte;
				input_bytes++;
			}

			//printf("Input bytes: %d\n", input_bytes);
			//printHexBuf(databuf, input_bytes);

			// Validate the record checksum
			if (chk(databuf, input_bytes)) {
				fprintf(stderr, "Bad checksum at line %d\n", line);
				ret = -4;
				goto err;
			}

			// Data length sanity check
			data_count = databuf[0];
			if (input_bytes != 1+2+1+data_count+1) {
				fprintf(stderr, "Invalid record (less data than expected) at line %d\n", line);
				ret = -5;
				goto err;
			}

			address = databuf[1]<<8 | databuf[2];

			switch(databuf[3])
			{
				case 0x00: // Data
					if (address + offset + data_count > bufsize) {
						fprintf(stderr, "hex file too large\n");
						ret = -6;
						goto err;
					}
					if (address + offset + data_count > max_address) {
						max_address = address + offset + data_count;
					}
					memcpy(dstbuf + address + offset, databuf + 4, data_count);
					break;

				case 0x01: // EOF
					eof_seen = 1;
					break;

				case 0x04: // Extended linear address
					if (data_count != 2) {
						fprintf(stderr, "ihex parser: Malformatted 0x04 record at line %d\n", line);
						ret = -8;
						goto err;
					}
					offset = (databuf[4] << 24) | (databuf[5] << 16);
					//printf("OFfset: 0x%08x\n", offset);
					break;

				case 0x03: // Start segment address
				case 0x05: // Start linear address
					// Ignored
					break;

				default:
				case 0x02: // Extended segment address
					fprintf(stderr, "ihex parser: Unimplemented record type 0x%02x at line %d\n", databuf[3], line);
					ret = -2;
					goto err;
			}

		}
	} while (!feof(fptr));

	fclose(fptr);
	return max_address;

err:
	fclose(fptr);
	return ret;
}
