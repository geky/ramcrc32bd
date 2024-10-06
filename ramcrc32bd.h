/*
 * An example of crc32 based error-correcting block device in RAM
 *
 * Copyright (c) 2024, The littlefs authors.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RAMCRC32BD_H
#define RAMCRC32BD_H

#include "lfs.h"
#include "lfs_util.h"

#ifdef __cplusplus
extern "C"
{
#endif


// Block device specific tracing
#ifndef RAMCRC32BD_TRACE
#ifdef RAMCRC32BD_YES_TRACE
#define RAMCRC32BD_TRACE(...) LFS_TRACE(__VA_ARGS__)
#else
#define RAMCRC32BD_TRACE(...)
#endif
#endif

// rambd config
struct ramcrc32bd_config {
    // Size of a codeword in bytes.
    //
    // A crc32 is 4 bytes, so read_size and prog_size = code_size - 4.
    lfs_size_t code_size;

    // Size of an erase operation in bytes.
    //
    // Must be a multiple of code_size.
    lfs_size_t erase_size;

    // Number of erase blocks on the device.
    lfs_size_t erase_count;

    // Number of bit errors to try to correct.
    //
    // There is a tradeoff here. Every bit errors you try to correct is two
    // fewer bit errors you can detect reliably. That being said, recovering
    // from errors is usually more useful than bailing on errors.
    //
    // By default, when zero, tries to correct as many errors as possible.
    // -1 disables error correction and errors on any errors.
    lfs_ssize_t error_correction;

    // Optional statically allocated buffer for the block device.
    void *buffer;
};

// rambd state
typedef struct ramcrc32bd {
    uint8_t *buffer;
    const struct ramcrc32bd_config *cfg;
} ramcrc32bd_t;


// Create a RAM block device
int ramcrc32bd_create(const struct lfs_config *cfg,
        const struct ramcrc32bd_config *bdcfg);

// Clean up memory associated with block device
int ramcrc32bd_destroy(const struct lfs_config *cfg);

// Read a block
int ramcrc32bd_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size);

// Program a block
//
// The block must have previously been erased.
int ramcrc32bd_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size);

// Erase a block
//
// A block must be erased before being programmed. The
// state of an erased block is undefined.
int ramcrc32bd_erase(const struct lfs_config *cfg, lfs_block_t block);

// Sync the block device
int ramcrc32bd_sync(const struct lfs_config *cfg);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
