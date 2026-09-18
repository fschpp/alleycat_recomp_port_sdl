#ifndef CGA_H
#define CGA_H

#include <stdint.h>
#include <stddef.h>

/* CGA mode 4 (320x200, 4 colors, 2 bits/pixel).
 * Real CGA memory is 16KB at B800:0000, split into two interleaved banks:
 *   bank 0 (even scanlines): offset 0x0000-0x1F3F
 *   bank 1 (odd  scanlines): offset 0x2000-0x3F3F
 * Each scanline is 80 bytes (320 px / 4 px-per-byte). This mirrors exactly
 * what src/cga.asm's blit routines walk over (the "xor di,0x2000 / add
 * di,0x50" bank-flip dance you see in every blit loop). */
#define CGA_WIDTH        320
#define CGA_HEIGHT       200
#define CGA_BYTES_PER_ROW 80
#define CGA_BANK_SIZE    0x2000
#define CGA_MEM_SIZE     0x4000

/* IMPORTANT: "width" throughout these blit routines is a WORD count, not
 * a byte count. The original's blit loops walk with `lodsw`/`stosw` (word
 * ops), so a dims byte of e.g. 3 means 3 WORDS = 6 bytes = 24 pixels per
 * row. This was verified by cross-referencing sprite pointer-table deltas
 * against declared dims in the resolved data segment (tools/resolve_data_segment.py
 * + tools/extract_walk_sprites.py) — every single frame boundary in the
 * 13-frame walk cycle lines up exactly under width_words*2, with zero gap
 * or overlap, including the very last frame ending precisely at the start
 * of the next table. Do NOT pass a raw byte width to these functions —
 * always width_words (matching the original's dims tables directly). */
#define CGA_WORD_WIDTH_TO_BYTES(w) ((w) * 2)

extern uint8_t cga_mem[CGA_MEM_SIZE];

/* Set on init_cga(); mirrors [rng_seed] in the original */
extern uint16_t rng_seed;

void cga_init(void);

/* --- calc_cga_addr ---
 * row, col: pixel row/col (col need not be byte-aligned; internally the
 * original packs 4 pixels/byte, so col is masked to get bit position).
 * Returns byte offset into cga_mem, and *bit_shift gets the 2-bit pixel
 * shift (0,2,4,6) within that byte for use with blit routines that write
 * a whole byte at a time - kept mainly for callers that address a single
 * pixel rather than a byte-aligned sprite blit. */
size_t calc_cga_addr(uint8_t row, uint16_t col, uint8_t *bit_shift);

/* --- blit_to_cga ---
 * Straight copy (no masking) of a width_words x height block of CGA-encoded
 * bytes from `src` into cga_mem at byte offset `dst_offset`.
 * width is in WORDS (2 bytes each, matching the original dims tables), height is in scanlines. */
void blit_to_cga(const uint8_t *src, size_t dst_offset, uint8_t width_words, uint8_t height);

/* --- blit_bytes_to_cga ---
 * Byte-granular sibling of blit_to_cga for draw_block_list's REP MOVSB
 * semantics — width_bytes is a raw BYTE count here, unlike every other
 * blit_* function in this header where width is a WORD count (§4). See
 * PROGRESS.md §5w. */
void blit_bytes_to_cga(const uint8_t *src, size_t dst_offset, uint8_t width_bytes, uint8_t height);

/* --- save_from_cga ---
 * Inverse of blit_to_cga: copies a block OUT of cga_mem into `dst`.
 * Used to save the background before drawing a sprite, so it can be
 * restored later (classic "dirty rectangle" sprite technique). */
void save_from_cga(uint8_t *dst, size_t src_offset, uint8_t width_words, uint8_t height);

/* --- blit_masked ---
 * AND-blit: for each source word, ANDs it against the corresponding dest
 * word (which was previously saved to `mask_save` for later restore) then
 * writes the source word straight through. In the original this is used
 * together with blit_transparent as the "erase mask" pass of two-pass
 * transparent sprite drawing. mask_save must be big enough to hold
 * width_words*height words (it stores original dest words). */
void blit_masked(const uint8_t *src, size_t dst_offset, uint8_t width_words, uint8_t height,
                  uint16_t *mask_save);

/* --- blit_transparent --- FIXED, see PROGRESS.md §5j AND §5p.
 * Single-word-per-column color-keyed transparent blit: any 2-bit CGA
 * pixel in `src` that's 0 (black) lets the background show through;
 * any non-zero pixel is drawn opaquely exactly as stored. Re-derived
 * bit-by-bit from cga.asm's real algorithm and verified by brute force
 * over all 16 (source-pixel, dest-pixel) combinations — see PROGRESS.md
 * for the derivation. `src` is ONE word per width unit (not an
 * interleaved mask/color pair, unlike an earlier incorrect version of
 * this function).
 * IMPORTANT (added in §5p, missed in the original §5j fix): the real
 * asm ALSO saves each original destination word into `mask_save` before
 * overwriting it, same pattern as blit_masked — used by callers like
 * draw_enemy to later restore the background via erase_enemy. Pass NULL
 * if the caller doesn't need this (matches blit_masked's convention). */
void blit_transparent(const uint8_t *src, size_t dst_offset, uint8_t width_words, uint8_t height,
                       uint16_t *mask_save);

/* --- copy_with_stride ---
 * Copies width_words*2*height bytes from src into cga_mem at dst_offset,
 * advancing src by (width_words*2 + extra_stride) each row instead of by
 * the CGA bank-interleave rule. Used for copying out of the linear sprite
 * ROM data (which isn't bank-interleaved) into CGA form. */
void copy_with_stride(const uint8_t *src, size_t dst_offset, uint8_t width_words, uint8_t height,
                       uint8_t extra_stride_bytes);

/* --- random (16-bit LFSR) --- */
uint16_t cga_random(void);

/* --- read_pit_counter ---
 * Original seeds the RNG from the raw PIT counter (a free-running hardware
 * timer, effectively "whatever value happens to be on the chip right now").
 * We substitute a high-resolution host clock read for the same effect. */
void seed_rng_from_clock(void);

#endif
