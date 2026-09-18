#include "cga.h"
#include <string.h>
#include <time.h>

uint8_t cga_mem[CGA_MEM_SIZE];
uint16_t rng_seed = 0xFA59; /* fallback seed used by the original when PIT reads 0 */

void cga_init(void) {
    memset(cga_mem, 0, sizeof(cga_mem));
    seed_rng_from_clock();
}

size_t calc_cga_addr(uint8_t row, uint16_t col, uint8_t *bit_shift) {
    size_t offset = (size_t)(row >> 1) * CGA_BYTES_PER_ROW;
    if (row & 1) offset += CGA_BANK_SIZE;
    offset += col >> 2;
    if (bit_shift) *bit_shift = (uint8_t)((col & 0x3) << 1);
    return offset;
}

/* Shared bank-flip step used by every blit_* loop in the original: after
 * finishing a scanline, XOR the offset with 0x2000 to flip banks; if that
 * cleared bit 13 (i.e. we're back in bank 0) it means we just wrapped from
 * an odd row to the next even row, so advance by one row's worth (80B). */
static inline size_t next_row_offset(size_t off) {
    off ^= CGA_BANK_SIZE;
    if (!(off & CGA_BANK_SIZE)) off += CGA_BYTES_PER_ROW;
    return off;
}

/* NOTE ON UNITS: width_words is a WORD count (verified against real sprite
 * pointer-table deltas — see include/cga.h). Every loop below therefore
 * walks `width_words` iterations of 2 bytes each = width_words*2 bytes/row,
 * exactly mirroring the original's `lodsw`/`stosw` word-at-a-time loops. */

void blit_to_cga(const uint8_t *src, size_t dst_offset, uint8_t width_words, uint8_t height) {
    size_t row_bytes = (size_t)width_words * 2;
    for (uint8_t y = 0; y < height; y++) {
        memcpy(&cga_mem[dst_offset], src, row_bytes);
        src += row_bytes;
        dst_offset = next_row_offset(dst_offset);
    }
}

/* blit_bytes_to_cga — byte-granular sibling of blit_to_cga, for the one
 * place in the codebase that uses REP MOVSB instead of REP MOVSW:
 * alley_drawing.asm's draw_block_list (background/level-room tile
 * blitter, see level_background.c / PROGRESS.md §5w). width_bytes here
 * really is a raw byte count, unlike every other blit_* function in this
 * file where width is a WORD count (§4) — do not confuse the two. */
void blit_bytes_to_cga(const uint8_t *src, size_t dst_offset, uint8_t width_bytes, uint8_t height) {
    for (uint8_t y = 0; y < height; y++) {
        memcpy(&cga_mem[dst_offset], src, width_bytes);
        src += width_bytes;
        dst_offset = next_row_offset(dst_offset);
    }
}

void save_from_cga(uint8_t *dst, size_t src_offset, uint8_t width_words, uint8_t height) {
    size_t row_bytes = (size_t)width_words * 2;
    for (uint8_t y = 0; y < height; y++) {
        memcpy(dst, &cga_mem[src_offset], row_bytes);
        dst += row_bytes;
        src_offset = next_row_offset(src_offset);
    }
}

void blit_masked(const uint8_t *src, size_t dst_offset, uint8_t width_words, uint8_t height,
                  uint16_t *mask_save) {
    for (uint8_t y = 0; y < height; y++) {
        size_t x = 0;
        for (uint8_t w = 0; w < width_words; w++, x += 2) {
            uint16_t dest_word;
            memcpy(&dest_word, &cga_mem[dst_offset + x], sizeof(dest_word));
            if (mask_save) *mask_save++ = dest_word;

            uint16_t src_word;
            memcpy(&src_word, src, sizeof(src_word));
            src += 2;

            uint16_t result = src_word & dest_word;
            memcpy(&cga_mem[dst_offset + x], &result, sizeof(result));
        }
        dst_offset = next_row_offset(dst_offset);
    }
}

/* blit_transparent — FIXED (see PROGRESS.md §5j). Re-derived bit-by-bit
 * from cga.asm's real algorithm (one word per column via lodsw, then a
 * data-dependent XOR-0x33cc / test-0x30c0 loop) and verified by brute
 * force over all 16 (2-bit source, 2-bit dest) combinations. The net
 * effect per 2-bit CGA pixel:
 *   - source pixel == 0 (black)      -> result = dest   (background shows through)
 *   - source pixel != 0 (any color)  -> result = source  (opaque exact overwrite)
 * i.e. black is the chroma-key transparent color. Implemented directly
 * from that derived truth table rather than replicating the original's
 * iterative bit-mask dance, since they're provably equivalent and this
 * is far more readable — but the derivation that got here is preserved
 * in PROGRESS.md for anyone who wants to re-verify against the asm. */
void blit_transparent(const uint8_t *src, size_t dst_offset, uint8_t width_words, uint8_t height,
                       uint16_t *mask_save) {
    for (uint8_t y = 0; y < height; y++) {
        size_t x = 0;
        for (uint8_t w = 0; w < width_words; w++, x += 2) {
            uint16_t dest_word;
            memcpy(&dest_word, &cga_mem[dst_offset + x], sizeof(dest_word));
            if (mask_save) *mask_save++ = dest_word; /* §5p: matches the real asm's
                                                        * "mov word[bp],bx" side effect,
                                                        * missed in the original §5j fix */

            uint16_t src_word;
            memcpy(&src_word, src, sizeof(src_word));
            src += 2;

            uint16_t result = 0;
            for (int shift = 0; shift < 16; shift += 2) {
                uint16_t mask = (uint16_t)(0x3 << shift);
                uint16_t src_px = src_word & mask;
                result |= (src_px == 0) ? (dest_word & mask) : src_px;
            }

            memcpy(&cga_mem[dst_offset + x], &result, sizeof(result));
        }
        dst_offset = next_row_offset(dst_offset);
    }
}

void copy_with_stride(const uint8_t *src, size_t dst_offset, uint8_t width_words, uint8_t height,
                       uint8_t extra_stride_bytes) {
    size_t row_bytes = (size_t)width_words * 2;
    for (uint8_t y = 0; y < height; y++) {
        memcpy(&cga_mem[dst_offset], src, row_bytes);
        src += row_bytes + extra_stride_bytes;
        dst_offset = next_row_offset(dst_offset);
    }
}

/* blit_or — OR-composite blit, see include/cga.h for the derivation. */
void blit_or(const uint8_t *src, size_t dst_offset, uint8_t width_words, uint8_t height,
             uint16_t *mask_save) {
    for (uint8_t y = 0; y < height; y++) {
        size_t x = 0;
        for (uint8_t w = 0; w < width_words; w++, x += 2) {
            uint16_t dest_word;
            memcpy(&dest_word, &cga_mem[dst_offset + x], sizeof(dest_word));
            if (mask_save) *mask_save++ = dest_word;

            uint16_t src_word;
            memcpy(&src_word, src, sizeof(src_word));
            src += 2;

            uint16_t result = (uint16_t)(src_word | dest_word);
            memcpy(&cga_mem[dst_offset + x], &result, sizeof(result));
        }
        dst_offset = next_row_offset(dst_offset);
    }
}

/* 16-bit LFSR, matching:
 *   dx = rng_seed; dl ^= dh; dl >>= 2; rcr rng_seed,1; dx = rng_seed
 * i.e. feedback bit = (low^high byte) >> 2 folded in through carry rotate. */
uint16_t cga_random(void) {
    uint16_t dx = rng_seed;
    uint8_t dl = (uint8_t)(dx & 0xFF);
    uint8_t dh = (uint8_t)(dx >> 8);
    dl = (uint8_t)(dl ^ dh);
    dl >>= 2;
    uint16_t carry_in = dl & 1;
    uint16_t result = (uint16_t)((rng_seed >> 1) | (carry_in << 15));
    rng_seed = result;
    return rng_seed;
}

void seed_rng_from_clock(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint16_t seed = (uint16_t)(ts.tv_nsec ^ (ts.tv_nsec >> 16));
    rng_seed = seed ? seed : 0xFA59;
}
