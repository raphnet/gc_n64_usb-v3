/**
 * @file mempak.h
 * @brief Mempak Filesystem Routines
 * @ingroup mempak
 */
#ifndef __LIBDRAGON_MEMPAK_H
#define __LIBDRAGON_MEMPAK_H

#include "mempak.h"
#include <stdint.h>

/**
 * @addtogroup mempak
 * @{
 */

/** @brief Size in bytes of a Mempak block */
#define MEMPAK_BLOCK_SIZE   256

/**
 * @brief Structure representing a save entry in a mempak
 */
typedef struct entry_structure
{
    /** @brief Vendor ID */
    uint32_t vendor;
    /** @brief Game ID */
    uint16_t game_id;
    /** @brief Inode pointer */
    uint16_t inode;
    /** @brief Intended region */
    uint8_t region;
    /** @brief Number of blocks used by this entry.
     * @see MEMPAK_BLOCK_SIZE */
    uint8_t blocks;
    /** @brief Validity of this entry. */
    uint8_t valid;
    /** @brief ID of this entry */
    uint8_t entry_id;
    /**
     * @brief Name of this entry
     *
     * The complete list of valid ASCII characters in a note name is:
     *
     * <pre>
     * ABCDEFGHIJKLMNOPQRSTUVWXYZ!"#`*+,-./:=?\@
     * </pre>
     *
     * The space character is also allowed.  Any other character will be
     * converted to a space before writing to the mempak.
     *
     * @see #__n64_to_ascii and #__ascii_to_n64
     */
    char name[19];

	/** @brief A copy of the raw note data (from the note table). */
	unsigned char raw_data[32];
} entry_structure_t;

#ifdef __cplusplus
extern "C" {
#endif

int read_mempak_sector( mempak_structure_t *pak, int sector, uint8_t *sector_data );
int write_mempak_sector( mempak_structure_t *pak, int sector, uint8_t *sector_data );
int validate_mempak( mempak_structure_t *pak );
int get_mempak_free_space( mempak_structure_t *pak );
int get_mempak_entry( mempak_structure_t *pak, int entry, entry_structure_t *entry_data );
int format_mempak( mempak_structure_t *pak );
int read_mempak_entry_data( mempak_structure_t *pak, entry_structure_t *entry, uint8_t *data );
int write_mempak_entry_data( mempak_structure_t *pak, entry_structure_t *entry, uint8_t *data );
int delete_mempak_entry( mempak_structure_t *pak, entry_structure_t *entry );

int mempak_parse_entry( const uint8_t *tnote, entry_structure_t *note );

#ifdef __cplusplus
}
#endif

/** @} */ /* mempak */

#endif
