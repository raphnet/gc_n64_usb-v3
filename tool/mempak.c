#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "gcn64.h"
#include "mempak.h"
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
	unsigned char cmd[64];
	int cmdlen;
	int n;
	uint16_t addr_crc;

	addr_crc = __calc_address_crc(addr);

	cmd[0] = RQ_GCN64_RAW_SI_COMMAND;
	cmd[1] = 3;
	cmd[2] = N64_EXPANSION_READ;
	cmd[3] = addr_crc>>8; // Address high byte
	cmd[4] = addr_crc&0xff; // Address low byte
	memcpy(cmd + 5, data, 0x20);

	cmdlen = 5 + 0x20;

	n = gcn64_exchange(hdl, cmd, cmdlen, cmd, sizeof(cmd));
	if (n != 35)
		return -1;

	return 0x20;
}

int mempak_readBlock(gcn64_hdl_t hdl, unsigned short addr, unsigned char dst[32])
{
	unsigned char cmd[64];
	int cmdlen;
	int n;
	uint16_t addr_crc;
	unsigned char crc;

	addr_crc = __calc_address_crc(addr);

	cmd[0] = RQ_GCN64_RAW_SI_COMMAND;
	cmd[1] = 3;
	cmd[2] = N64_EXPANSION_READ;
	cmd[3] = addr_crc>>8; // Address high byte
	cmd[4] = addr_crc&0xff; // Address low byte
	cmdlen = 5;

	//printf("Addr 0x%04x with crc -> 0x%04x\n", addr, addr_crc);

	n = gcn64_exchange(hdl, cmd, cmdlen, cmd, sizeof(cmd));
	if (n != 35)
		return -1;

	memcpy(dst, cmd + 2, 0x20);

	crc = __calc_data_crc(dst);
	if (crc != cmd[34]) {
		fprintf(stderr, "Bad CRC reading address 0x%04x\n", addr);
		return -1;
	}

	return 0x20;
}

int mempak_init(gcn64_hdl_t hdl)
{
	unsigned char buf[0x20];

	memset(buf, 0xfe, 32);
	mempak_writeBlock(hdl, 0x8000, buf);
	memset(buf, 0x80, 32);
	mempak_writeBlock(hdl, 0x8000, buf);

	return 0;
}

#define DUMP_SIZE	0x8000

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

void mempak_dump(gcn64_hdl_t hdl)
{
	unsigned char cardbuf[0x8000];
	int i,j;

	printf("Init pak\n");
	mempak_init(hdl);

	printf("Reading card...\n");
	mempak_readAll(hdl, cardbuf);

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
}

#define NUM_COPIES 4

int mempak_dumpToFile(gcn64_hdl_t hdl, const char *out_filename)
{
	unsigned char cardbuf[0x8000];
	FILE *fptr;
	int copies;

	printf("Init pak\n");
	mempak_init(hdl);
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

