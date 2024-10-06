/*
 * An example of crc32 based error-correcting block device in RAM
 *
 * Copyright (c) 2024, The littlefs authors.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ramcrc32bd.h"

int ramcrc32bd_create(const struct lfs_config *cfg,
        const struct ramcrc32bd_config *bdcfg) {
    RAMCRC32BD_TRACE("ramcrc32bd_create(%p {.context=%p, "
                ".read=%p, .prog=%p, .erase=%p, .sync=%p}, "
                "%p {.code_size=%"PRIu32", "
                ".erase_size=%"PRIu32", .erase_count=%"PRIu32", "
                ".buffer=%p})",
            (void*)cfg, cfg->context,
            (void*)(uintptr_t)cfg->read, (void*)(uintptr_t)cfg->prog,
            (void*)(uintptr_t)cfg->erase, (void*)(uintptr_t)cfg->sync,
            (void*)bdcfg,
            bdcfg->code_size, bdcfg->erase_size,
            bdcfg->erase_count, bdcfg->buffer);
    ramcrc32bd_t *bd = cfg->context;
    bd->cfg = bdcfg;

    // The from code size to message size is a bit complicated, so let's make
    // sure things are configured correctly
    LFS_ASSERT(bdcfg->erase_size % bdcfg->code_size == 0);
    LFS_ASSERT(cfg->read_size % (bdcfg->code_size-sizeof(uint32_t)) == 0);
    LFS_ASSERT(cfg->prog_size % (bdcfg->code_size-sizeof(uint32_t)) == 0);
    LFS_ASSERT(cfg->block_size
            % (bdcfg->erase_size
                - ((bdcfg->erase_size/bdcfg->code_size)
                    * sizeof(uint32_t)))
            == 0);

    // Make sure the requested error correction is possible
    LFS_ASSERT(bdcfg->error_correction <= 0
            || bdcfg->error_correction < 1
            || cfg->read_size <= 536870907);
    LFS_ASSERT(bdcfg->error_correction <= 0
            || bdcfg->error_correction < 2
            || cfg->read_size <= 371);
    LFS_ASSERT(bdcfg->error_correction <= 0
            || bdcfg->error_correction < 3
            || cfg->read_size <= 21);

    // allocate buffer?
    if (bd->cfg->buffer) {
        bd->buffer = bd->cfg->buffer;
    } else {
        bd->buffer = lfs_malloc(bd->cfg->erase_size * bd->cfg->erase_count);
        if (!bd->buffer) {
            RAMCRC32BD_TRACE("ramcrc32bd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // zero for reproducibility
    memset(bd->buffer, 0, bd->cfg->erase_size * bd->cfg->erase_count);

    RAMCRC32BD_TRACE("ramcrc32bd_create -> %d", 0);
    return 0;
}

int ramcrc32bd_destroy(const struct lfs_config *cfg) {
    RAMCRC32BD_TRACE("ramcrc32bd_destroy(%p)", (void*)cfg);
    // clean up memory
    ramcrc32bd_t *bd = cfg->context;
    if (!bd->cfg->buffer) {
        lfs_free(bd->buffer);
    }
    RAMCRC32BD_TRACE("ramcrc32bd_destroy -> %d", 0);
    return 0;
}

int ramcrc32bd_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size) {
    RAMCRC32BD_TRACE("ramcrc32bd_read(%p, "
                "0x%"PRIx32", %"PRIu32", %p, %"PRIu32")",
            (void*)cfg, block, off, buffer, size);
    ramcrc32bd_t *bd = cfg->context;

    // check if read is valid
    LFS_ASSERT(block < cfg->block_count);
    LFS_ASSERT(off  % cfg->read_size == 0);
    LFS_ASSERT(size % cfg->read_size == 0);
    LFS_ASSERT(off+size <= cfg->block_size);

    // work on one codeword at a time
    uint8_t *buffer_ = buffer;
    while (size > 0) {
        // map off to codeword space
        lfs_off_t off_
                = (off / (bd->cfg->code_size-sizeof(uint32_t)))
                * bd->cfg->code_size;

        // read data
        memcpy(buffer_,
                &bd->buffer[block*bd->cfg->erase_size + off_],
                bd->cfg->code_size-sizeof(uint32_t));

        // read crc
        uint32_t crc;
        memcpy(&crc,
                &bd->buffer[block*bd->cfg->erase_size + off_
                    + bd->cfg->code_size-sizeof(uint32_t)],
                sizeof(uint32_t));
        crc = lfs_fromle32(crc);

        // does crc match?
        uint32_t crc_ = lfs_crc(0, buffer_,
                bd->cfg->code_size-sizeof(uint32_t));
        if (crc_ != crc) {
            // no? can we correct?

            // Thanks to Philip Koopman's exhaustive search work, we know our
            // CRC-32 has the following Hamming distances up to these sizes:
            //
            //   poly=0x104c11db7:
            //          up to               up to               can correct
            //   HD=3:  4294967263 bits     536870907 bytes     1 bit error
            //   HD=4:  91607 bits          11450 bytes         
            //   HD=5:  2974 bits           371 bytes           2 bit errors
            //   HD=6:  268 bits            33 bytes
            //   HD=7:  171 bits            21 bytes            3 bit errors
            //   HD=8:  91 bits             11 bytes
            //   HD=9:  57 bits             7 bytes             4 bit errors
            //   HD=10: 34 bits             4 bytes
            //   HD=11: 21 bits             2 bytes             5 bit errors
            //   HD=12: 12 bits             1 byte
            //   HD=13: 10 bits             1 byte              6 bit errors
            //   HD=14: 10 bits             1 byte
            //   HD=15: 10 bits             1 byte              7 bit errors
            //
            // Ref:
            // - https://users.ece.cmu.edu/~koopman/crc/crc32.html
            // - https://users.ece.cmu.edu/~koopman/crc/c32/0x82608edb_len.txt

            // try to fix 1 bit error
            //
            // this gets a bit funky since most CRCs are bit-reversed and
            // shift/overflow right
            if ((!bd->cfg->error_correction || bd->cfg->error_correction >= 1)
                    && cfg->read_size <= 536870907) {
                uint32_t e = 0x80000000;
                for (lfs_size_t i = 0; i < bd->cfg->code_size*8; i++) {
                    if (e == (crc_ ^ crc)) {
                        // found a 1 bit solution
                        lfs_size_t bit = bd->cfg->code_size*8-1 - i;
                        if (bit/8 < cfg->read_size) {
                            buffer_[bit/8] ^= 1 << (bit%8);
                        }
                        LFS_DEBUG("Found correctable ramcrc32bd error "
                                "0x%"PRIx32".%"PRIx32" %"PRIu32", "
                                "e={%"PRIu32"}",
                                block, off_,
                                bd->cfg->code_size
                                    - (uint32_t)sizeof(uint32_t),
                                bit);
                        goto fixed;
                    }

                    // shift our error bit
                    e = (e >> 1) ^ ((e & 1) ? 0xedb88320 : 0);
                }
            }

            // try to fix 2 bit errors
            if ((!bd->cfg->error_correction || bd->cfg->error_correction >= 2)
                    && cfg->read_size <= 371) {
                uint32_t e0 = 0x80000000;
                for (lfs_size_t i0 = 0; i0 < bd->cfg->code_size*8; i0++) {
                    uint32_t e1 = 0x80000000;
                    for (lfs_size_t i1 = 0; i1 < bd->cfg->code_size*8; i1++) {
                        if ((e0 ^ e1) == (crc_ ^ crc)) {
                            // found a 2 bit solution
                            lfs_size_t bit0 = bd->cfg->code_size*8-1 - i0;
                            lfs_size_t bit1 = bd->cfg->code_size*8-1 - i1;
                            if (bit0/8 < cfg->read_size) {
                                buffer_[bit0/8] ^= 1 << (bit0%8);
                            }
                            if (bit1/8 < cfg->read_size) {
                                buffer_[bit1/8] ^= 1 << (bit1%8);
                            }
                            LFS_DEBUG("Found correctable ramcrc32bd error "
                                    "0x%"PRIx32".%"PRIx32" %"PRIu32", "
                                    "e={%"PRIu32",%"PRIu32"}",
                                    block, off_,
                                    bd->cfg->code_size
                                        - (uint32_t)sizeof(uint32_t),
                                    bit0, bit1);
                            goto fixed;
                        }

                        // shift our error bit
                        e1 = (e1 >> 1) ^ ((e1 & 1) ? 0xedb88320 : 0);
                    }

                    // shift our error bit
                    e0 = (e0 >> 1) ^ ((e0 & 1) ? 0xedb88320 : 0);
                }
            }

            // try to fix 3 bit errors
            if ((!bd->cfg->error_correction || bd->cfg->error_correction >= 3)
                    && cfg->read_size <= 21) {
                uint32_t e0 = 0x80000000;
                for (lfs_size_t i0 = 0; i0 < bd->cfg->code_size*8; i0++) {
                  uint32_t e1 = 0x80000000;
                  for (lfs_size_t i1 = 0; i1 < bd->cfg->code_size*8; i1++) {
                    uint32_t e2 = 0x80000000;
                    for (lfs_size_t i2 = 0; i2 < bd->cfg->code_size*8; i2++) {
                      if ((e0 ^ e1 ^ e2) == (crc_ ^ crc)) {
                          // found a 3 bit solution
                          lfs_size_t bit0 = bd->cfg->code_size*8-1 - i0;
                          lfs_size_t bit1 = bd->cfg->code_size*8-1 - i1;
                          lfs_size_t bit2 = bd->cfg->code_size*8-1 - i2;
                          if (bit0/8 < cfg->read_size) {
                              buffer_[bit0/8] ^= 1 << (bit0%8);
                          }
                          if (bit1/8 < cfg->read_size) {
                              buffer_[bit1/8] ^= 1 << (bit1%8);
                          }
                          if (bit2/8 < cfg->read_size) {
                              buffer_[bit2/8] ^= 1 << (bit2%8);
                          }
                          LFS_DEBUG("Found correctable ramcrc32bd error "
                                  "0x%"PRIx32".%"PRIx32" %"PRIu32", "
                                  "e={%"PRIu32",%"PRIu32",%"PRIu32"}",
                                  block, off_,
                                  bd->cfg->code_size
                                    - (uint32_t)sizeof(uint32_t),
                                  bit0, bit1, bit2);
                          goto fixed;
                      }

                      // shift our error bit
                      e2 = (e2 >> 1) ^ ((e2 & 1) ? 0xedb88320 : 0);
                    }

                    // shift our error bit
                    e1 = (e1 >> 1) ^ ((e1 & 1) ? 0xedb88320 : 0);
                  }

                  // shift our error bit
                  e0 = (e0 >> 1) ^ ((e0 & 1) ? 0xedb88320 : 0);
                }
            }

            // no solution?
            LFS_DEBUG("Found uncorrectable ramcrc32bd crc mismatch "
                    "0x%"PRIx32".%"PRIx32" %"PRIu32", "
                    "crc32 %08"PRIx32" (!= %08"PRIx32")",
                    block, off_,
                    bd->cfg->code_size - (uint32_t)sizeof(uint32_t),
                    crc_, crc);
            return LFS_ERR_CORRUPT;
fixed:;
        }

        off += cfg->read_size;
        buffer_ += cfg->read_size;
        size -= cfg->read_size;
    }

    RAMCRC32BD_TRACE("ramcrc32bd_read -> %d", 0);
    return 0;
}

int ramcrc32bd_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size) {
    RAMCRC32BD_TRACE("ramcrc32bd_prog(%p, "
                "0x%"PRIx32", %"PRIu32", %p, %"PRIu32")",
            (void*)cfg, block, off, buffer, size);
    ramcrc32bd_t *bd = cfg->context;

    // check if prog is valid
    LFS_ASSERT(block < cfg->block_count);
    LFS_ASSERT(off  % cfg->prog_size == 0);
    LFS_ASSERT(size % cfg->prog_size == 0);
    LFS_ASSERT(off+size <= cfg->block_size);

    // work on one codeword at a time
    const uint8_t *buffer_ = buffer;
    while (size > 0) {
        // map off to codeword space
        lfs_off_t off_
                = (off / (bd->cfg->code_size-sizeof(uint32_t)))
                * bd->cfg->code_size;

        // calculate crc
        uint32_t crc = lfs_crc(0, buffer_,
                bd->cfg->code_size-sizeof(uint32_t));

        // program data
        memcpy(&bd->buffer[block*bd->cfg->erase_size + off_],
                buffer_, bd->cfg->code_size-sizeof(uint32_t));

        // program crc
        crc = lfs_tole32(crc);
        memcpy(&bd->buffer[block*bd->cfg->erase_size + off_
                    + bd->cfg->code_size-sizeof(uint32_t)],
                &crc,
                sizeof(uint32_t));

        off += cfg->prog_size;
        buffer_ += cfg->prog_size;
        size -= cfg->prog_size;
    }

    RAMCRC32BD_TRACE("ramcrc32bd_prog -> %d", 0);
    return 0;
}

int ramcrc32bd_erase(const struct lfs_config *cfg, lfs_block_t block) {
    RAMCRC32BD_TRACE("ramcrc32bd_erase(%p, 0x%"PRIx32" (%"PRIu32"))",
            (void*)cfg, block, ((ramcrc32bd_t*)cfg->context)->cfg->erase_size);

    // check if erase is valid
    LFS_ASSERT(block < cfg->block_count);

    // erase is a noop
    (void)block;

    RAMCRC32BD_TRACE("ramcrc32bd_erase -> %d", 0);
    return 0;
}

int ramcrc32bd_sync(const struct lfs_config *cfg) {
    RAMCRC32BD_TRACE("ramcrc32bd_sync(%p)", (void*)cfg);

    // sync is a noop
    (void)cfg;

    RAMCRC32BD_TRACE("ramcrc32bd_sync -> %d", 0);
    return 0;
}
