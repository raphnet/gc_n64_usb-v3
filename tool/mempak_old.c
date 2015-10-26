#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "hexdump.h"
#include "gcn64.h"
#include "gcn64lib.h"
#include "mempak_old.h"
#include "../gcn64_protocol.h"
#include "../requests.h"

/* __calc_address_crc is from libdragon which is public domain. */

/**
 * @brief Calculate the 5 bit CRC on a mempak address
 *
 * This function, given an address intended for a mempak read or write, will
 * calculate the CRC on the address, returning the corrected address | CRC.
 *
 * @param[in] address
 *            The mempak address to calculate CRC over
 *
 * @return The mempak address | CRC
 */
static uint16_t __calc_address_crc( uint16_t address )
{
    /* CRC table */
    uint16_t xor_table[16] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x15, 0x1F, 0x0B, 0x16, 0x19, 0x07, 0x0E, 0x1C, 0x0D, 0x1A, 0x01 };
    uint16_t crc = 0;
	int i;

    /* Make sure we have a valid address */
    address &= ~0x1F;

    /* Go through each bit in the address, and if set, xor the right value into the output */
    for( i = 15; i >= 5; i-- )
    {   
        /* Is this bit set? */
        if( ((address >> i) & 0x1) )
        {
           crc ^= xor_table[i];
        }
    }   

    /* Just in case */
    crc &= 0x1F;

    /* Create a new address with the CRC appended */
    return address | crc;
}

/* __calc_data_crc is from libdragon which is public domain. */

/**
 * @brief Calculate the 8 bit CRC over a 32-byte block of data
 *
 * This function calculates the 8 bit CRC appropriate for checking a 32-byte
 * block of data intended for or retrieved from a mempak.
 *
 * @param[in] data
 *            Pointer to 32 bytes of data to run the CRC over
 *
 * @return The calculated 8 bit CRC over the data
 */
static uint8_t __calc_data_crc( uint8_t *data )
{
    uint8_t ret = 0;
	int i,j;

    for( i = 0; i <= 32; i++ )
    {
        for( j = 7; j >= 0; j-- )
        {
            int tmp = 0;

            if( ret & 0x80 )
            {
                tmp = 0x85;
            }

            ret <<= 1;

            if( i < 32 )
            {
                if( data[i] & (0x01 << j) )
                {
                    ret |= 0x1;
                }
            }

            ret ^= tmp;
        }
    }

    return ret;
}


int mempak_writeBlock(gcn64_hdl_t hdl, unsigned short addr, unsigned char data[32])
{
	unsigned char cmd[40];
	int cmdlen;
	int n;
	uint16_t addr_crc;

	addr_crc = __calc_address_crc(addr);

	cmd[0] = N64_EXPANSION_WRITE;
	cmd[1] = addr_crc>>8; // Address high byte
	cmd[2] = addr_crc&0xff; // Address low byte
	memcpy(cmd + 3, data, 0x20);
	cmdlen = 3 + 0x20;

	n = gcn64lib_rawSiCommand(hdl, 0, cmd, cmdlen, cmd, sizeof(cmd));
	if (n != 1) {
		printf("write block returned != 1 (%d)\n", n);
		return -1;
	}

	return cmd[0];
}

int mempak_readBlock(gcn64_hdl_t hdl, unsigned short addr, unsigned char dst[32])
{
	unsigned char cmd[64];
	//int cmdlen;
	int n;
	uint16_t addr_crc;
	unsigned char crc;

	addr_crc = __calc_address_crc(addr);

	cmd[0] = N64_EXPANSION_READ;
	cmd[1] = addr_crc>>8; // Address high byte
	cmd[2] = addr_crc&0xff; // Address low byte

	n = gcn64lib_rawSiCommand(hdl, 0, cmd, 3, cmd, sizeof(cmd));
	if (n != 33) {
		printf("Hey! %d\n", n);
		return -1;
	}

	memcpy(dst, cmd, 0x20);

	crc = __calc_data_crc(dst);
	if (crc != cmd[32]) {
		fprintf(stderr, "Bad CRC reading address 0x%04x\n", addr);
		return -1;
	}

	return 0x20;
}

int mempak_init(gcn64_hdl_t hdl)
{
	unsigned char buf[0x20];
	int res;

	memset(buf, 0xfe, 32);
	res = mempak_readBlock(hdl, 0x8001, buf);

	if (res == 0xe1) {
		return 0;
	}

	return -1;
}

#define DUMP_SIZE	0x8000

int mempak_writeAll(gcn64_hdl_t hdl, unsigned char srcbuf[0x8000])
{
	unsigned short addr;
	unsigned char readback[0x20];
	int res;

	for (addr = 0x0000; addr < DUMP_SIZE; addr+= 0x20)
	{
		printf("Writing address 0x%04x / 0x8000\r", addr); fflush(stdout);
		res = mempak_writeBlock(hdl, addr, &srcbuf[addr]);
		if (res < 0) {
			fprintf(stderr, "Write error\n");
			return -1;
		}

		if (0x20 != mempak_readBlock(hdl, addr, readback)) {
			// TODO : Why not retry?
			fprintf(stderr, "readback failed\n");
			return -2;
		}

	}
	printf("\nDone!\n");

	return 0;
}


int mempak_readAll(gcn64_hdl_t hdl, unsigned char dstbuf[0x8000])
{
	unsigned short addr;

	for (addr = 0x0000; addr < DUMP_SIZE; addr+= 0x20)
	{
		printf("Reading address 0x%04x / 0x8000\r", addr); fflush(stdout);
		if (mempak_readBlock(hdl, addr, &dstbuf[addr]) != 0x20) {
			fprintf(stderr, "Error: Short read\n");
			return -1;
		}
	}
	printf("\nDone!\n");

	return 0;
}

int mempak_dump(gcn64_hdl_t hdl)
{
	unsigned char cardbuf[0x8000];
	int i,j;

	printf("Init pak\n");
	mempak_init(hdl);

	printf("Reading card...\n");
	i = mempak_readAll(hdl, cardbuf);
	if (i<0) {
		return i;
	}

	for (i=0; i<DUMP_SIZE; i+=0x20) {
		printf("%04x: ", i);
		for (j=0; j<0x20; j++) {
			printf("%02x ", cardbuf[i+j]);
		}
		printf("    ");

		for (j=0; j<0x20; j++) {
			printf("%c", isprint(cardbuf[i+j]) ? cardbuf[i+j] : '.' );
		}
		printf("\n");
	}

	return 0;
}

#define NUM_COPIES 4

int mempak_dumpToFile(gcn64_hdl_t hdl, const char *out_filename)
{
	unsigned char cardbuf[0x8000];
	FILE *fptr;
	int copies;

	printf("Init pak\n");
//	mempak_init(hdl);
	printf("Reading card...\n");
	mempak_readAll(hdl, cardbuf);

	printf("Writing to file '%s'\n", out_filename);
	fptr = fopen(out_filename, "w");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	for (copies = 0; copies < NUM_COPIES; copies++) {
		if (1 != fwrite(cardbuf, sizeof(cardbuf), 1, fptr)) {
			perror("fwrite");
			fclose(fptr);
			return -2;
		}
	}

	printf("Done\n");

	fclose(fptr);
	return 0;
}

int mempak_writeFromFile(gcn64_hdl_t hdl, const char *in_filename)
{
	unsigned char cardbuf[0x8000];
	FILE *fptr;
	long file_size;
	int i;
	int num_images = -1;
	long offset = 0;

	fptr = fopen(in_filename, "rb");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	fseek(fptr, 0, SEEK_END);
	file_size = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);

	printf("File size: %ld bytes\n", file_size);

	/* Raw binary images. Those can contain more than one card's data. For
	 * instance, Mupen64 seems to contain four saves. */
	for (i=1; i<=4; i++) {
		if (file_size == 0x8000*i) {
			num_images = i;
			printf("MPK file Contains %d image(s)\n", num_images);
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
			offset = 0x1040; // Thanks to N-Rage`s Dinput8 Plugin sources
		}
	}

	fseek(fptr, offset, SEEK_SET);
	fread(cardbuf, sizeof(cardbuf), 1, fptr);
	fclose(fptr);


	printf("Init pak\n");
	mempak_init(hdl);
	printf("Writing card...\n");
	mempak_writeAll(hdl, cardbuf);

	return 0;
}


