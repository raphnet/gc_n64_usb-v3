#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "gcn64lib.h"
#include "gcn64.h"
#include "mempak.h"
#include "mempak_gcn64usb.h"
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

int gcn64lib_mempak_readBlock(gcn64_hdl_t hdl, unsigned short addr, unsigned char dst[32])
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

int gcn64lib_mempak_detect(gcn64_hdl_t hdl)
{
	unsigned char buf[0x20];
	int res;

	printf("Init 1\n");
	memset(buf, 0xfe, 32);
	res = gcn64lib_expansionWrite(hdl, 0x8000, buf);
	if (res != 0xe1) {
		printf("res: %d\n", res);
		return 0;
	}

	printf("Init 2\n");
	memset(buf, 0x80, 32);
	res = gcn64lib_expansionWrite(hdl, 0x8000, buf);
	if (res != 0xe1) {
		printf("res: %d\n", res);
		return 0;
	}
	return -1;
}

int gcn64lib_mempak_writeBlock(gcn64_hdl_t hdl, unsigned short addr, unsigned char data[32])
{
	return gcn64lib_expansionWrite(hdl, __calc_address_crc(addr), data);
}

/**
 * \brief Read a physical mempak
 * \param hdl The Adapter handler
 * \param channel The adapter channel (for multi-port adapters)
 * \param pak Pointer to mempak_structure pointer to store the new mempak
 * \param progressCb Callback to notify read progress (called after each block)
 * \return 0: Success, -1: No mempak, -2: IO/error, -3: Other errors
 */
int gcn64lib_mempak_download(gcn64_hdl_t hdl, int channel, mempak_structure_t **mempak, void (*progressCb)(int cur_addr))
{
	mempak_structure_t *pak;
	unsigned short addr;

	if (!mempak) {
		return -3;
	}

	if (gcn64lib_mempak_detect(hdl)) {
		return -1;
	}

	pak = calloc(1, sizeof(mempak_structure_t));
	if (!pak) {
		return -3;
	}
	pak->file_format = MPK_FORMAT_MPK;

	for (addr = 0x0000; addr < MEMPAK_MEM_SIZE; addr+= 0x20)
	{
		if (gcn64lib_mempak_readBlock(hdl, addr, &pak->data[addr]) != 0x20) {
			fprintf(stderr, "Error: Short read\n");
			free(pak);
			return -2;
		}
		if (progressCb) {
			progressCb(addr);
		}
	}
	*mempak = pak;

	return 0;
}

int gcn64lib_mempak_upload(gcn64_hdl_t hdl, int channel, mempak_structure_t *pak, void (*progressCb)(int cur_addr))
{
	unsigned short addr;
	unsigned char readback[0x20];
	int res;

	if (!pak) {
		return -3;
	}
	if (gcn64lib_mempak_detect(hdl)) {
		return -1;
	}

	for (addr = 0x0000; addr < MEMPAK_MEM_SIZE; addr+= 0x20)
	{
		res = gcn64lib_mempak_writeBlock(hdl, addr, &pak->data[addr]);
		if (res < 0) {
			fprintf(stderr, "Write error\n");
			return -2;
		}

		if (0x20 != gcn64lib_mempak_readBlock(hdl, addr, readback)) {
			// TODO : Why not retry?
			fprintf(stderr, "readback failed\n");
			return -2;
		}

		if (memcmp(readback, &pak->data[addr], 0x20)) {
			fprintf(stderr, "Readback compare failed\n");
			return -2;
		}

		if (progressCb) {
			progressCb(addr);
		}
	}

	return 0;
}

