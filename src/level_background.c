#include "cat_state.h"
#include "cga.h"
#include "level_background.h"
#include "gen/ds_pool.h"
#include <stdint.h>
#include <string.h>

/* Immediate DS-address-as-constant labels (same "mov reg,label" pattern
 * already identified in §5u/§5v — these encode plain 16-bit constants
 * via a label's own address, not data reads). Resolved once via
 * resolve_data_segment.py and hardcoded here as what they really are:
 * offset constants, not data. */
#define LEVEL_BLOCK_DATA_OFS 0x1184
#define DAT_2284_OFS         0x2284
#define DAT_22CB_OFS         0x22cb
#define CGA_BANK1_BASE_OFS   0x2000

/* Real DS offsets for the various tile-list tables and door sprites,
 * resolved via resolve_data_segment.py against the verified data
 * segment — see PROGRESS.md §5w. */
#define LEVEL_BG_BLOCK_LIST   0x2570
#define LEVEL_TILE_LIST_A     0x2344
#define LEVEL3_EXTRA_BLOCKS   0x2624
#define BLOCK_PAIR_LIST_A     0x2384
#define BLOCK_PAIR_LIST_B     0x238c
#define PLATFORM_SPRITE_LIST  0x263c
#define LEDGE_SPRITE_LIST     0x2646
#define BORDER_BLOCK_LIST     0x251c
#define LEVEL6_DOOR_SPRITE    0x1fa0
#define LEVEL5_DOOR_SPRITE    0x1fe0
#define LEVEL2_BLOCK_TYPES    0x2656

/* Level 3's fence-pattern data (7 tile variants, 16 bytes each = 0x70
 * total) and level 4's random-floor-tile data — resolved via
 * resolve_data_segment.py against src/level_objects.asm's
 * draw_level3_bg/init_level4_bg (see PROGRESS.md). */
#define LEVEL3_FENCE_TILES    0x3730
#define LEVEL4_ROW_TILE_COUNTS 0x3cae   /* dat_3cae: 17 bytes, one per row */
#define LEVEL4_TILE_A         0x3aea    /* dat_3aea */
#define LEVEL4_TILE_B         0x3af8    /* dat_3af8 */
#define LEVEL4_TILE_C         0x3b02    /* dat_3b02 */
#define LEVEL4_BLOCKS_1       0x3c22    /* dat_3c22 */
#define LEVEL4_BLOCKS_2       0x3c3e    /* dat_3c3e */
#define LEVEL4_BLOCKS_3       0x3c9a    /* dat_3c9a */
#define LEVEL4_BLOCKS_4       0x3c56    /* dat_3c56 */
#define LEVEL4_SIGN_SPRITE    0x3caa    /* dat_3caa */

static uint16_t g_block_prev_dl = 0xff;

/* random() substitute — same convention as every other ported system. */
static uint8_t rnd_byte(void) {
    return (uint8_t)(cga_random() & 0xFF);
}

/* draw_block_list — literal port of alley_drawing.asm's generic
 * table-driven tile blitter. `list_ofs` points into ds_pool at a
 * {dims_word, {source_ofs,dest_delta}...,0xFFFF} structure: dims_word's
 * high byte is the row count, low byte the column count IN BYTES (this
 * one function uses REP MOVSB, byte granularity — unlike every other
 * blit_* in this project, which works in words, see §4/blit_bytes_to_cga's
 * comment). For each entry, blits ds_pool[source_ofs] to
 * cga_mem[base_offset + dest_delta], byte-exact (opaque, no
 * transparency — appropriate for solid background tiles). */
static void draw_block_list(uint16_t base_offset, uint16_t list_ofs) {
    uint8_t rows = ds_pool[list_ofs + 1];
    uint8_t cols_bytes = ds_pool[list_ofs];
    list_ofs += 2;

    for (;;) {
        uint16_t source_ofs = (uint16_t)(ds_pool[list_ofs] | (ds_pool[list_ofs + 1] << 8));
        if (source_ofs == 0xFFFF) return;
        uint16_t dest_delta = (uint16_t)(ds_pool[list_ofs + 2] | (ds_pool[list_ofs + 3] << 8));

        uint16_t dest = (uint16_t)(base_offset + dest_delta);
        blit_bytes_to_cga(&ds_pool[source_ofs], dest, cols_bytes, rows);

        list_ofs += 4;
    }
}

/* draw_vertical_line — literal port: draws a solid-color vertical line
 * (0x5f = 95 pixels tall) at cga offset `pos`, color byte `color`. */
static void draw_vertical_line(uint16_t pos, uint8_t color) {
    for (int i = 0; i < 0x5f; i++) {
        cga_mem[pos] = color;
        pos ^= CGA_BANK_SIZE;
        if (!(pos & CGA_BANK_SIZE)) pos = (uint16_t)(pos + CGA_BYTES_PER_ROW);
    }
}

/* draw_level_border — literal port: border tiles + 2 rectangular blanked
 * regions (36 words each) + 2 vertical divider lines. */
static void draw_level_border(uint16_t bar_base) {
    draw_block_list(bar_base, BORDER_BLOCK_LIST);

    uint16_t di = (uint16_t)(bar_base + 0x284);
    for (int i = 0; i < 0x24; i++) {
        cga_mem[di] = 0; cga_mem[di + 1] = 0;
        di = (uint16_t)(di + 2);
    }

    di = (uint16_t)(bar_base + LEVEL_BLOCK_DATA_OFS);
    for (int i = 0; i < 0x24; i++) {
        cga_mem[di] = 0; cga_mem[di + 1] = 0;
        di = (uint16_t)(di + 2);
    }

    draw_vertical_line((uint16_t)(bar_base + DAT_2284_OFS), 0x2a);
    draw_vertical_line((uint16_t)(bar_base + DAT_22CB_OFS), 0xa8);
}

/* draw_block_pair — literal port. Draws two fixed sub-lists
 * (block_pair_list_a then block_pair_list_b) at the same base position.
 * Used by levels 1 and 4. */
static void draw_block_pair(uint16_t base) {
    draw_block_list(base, BLOCK_PAIR_LIST_A);
    draw_block_list(base, BLOCK_PAIR_LIST_B);
}

/* draw_door_frame — literal port. Iterates 4 list pointers stored as an
 * UNNAMED inline word array immediately after draw_temp (bytes
 * 0x2636-0x263c in the verified data segment — the disassembler never
 * gave this array its own label; verified those 4 words are all
 * plausible in-range DS offsets before trusting this). NOT the same
 * table as draw_block_pair's block_pair_list_a/b — an earlier draft of
 * this port incorrectly assigned those to draw_door_frame instead;
 * corrected after re-checking the real assembly (`[si+draw_temp]`, not
 * `block_pair_list_a`/`_b`) — see PROGRESS.md §5w. */
static void draw_door_frame(uint16_t base) {
    static const uint16_t DOOR_FRAME_LIST_BASE = 0x2636; /* draw_temp+2 */
    for (int si = 8; si > 0; si -= 2) {
        uint16_t list_ptr = (uint16_t)(ds_pool[DOOR_FRAME_LIST_BASE + si - 2] |
                                        (ds_pool[DOOR_FRAME_LIST_BASE + si - 1] << 8));
        draw_block_list(base, list_ptr);
    }
}

static void draw_platform(uint16_t base) {
    for (int si = 0xa; si > 0; si -= 2) {
        uint16_t list_ptr = (uint16_t)(ds_pool[PLATFORM_SPRITE_LIST + si] |
                                        (ds_pool[PLATFORM_SPRITE_LIST + si + 1] << 8));
        draw_block_list(base, list_ptr);
    }
}

static void draw_ledge(uint16_t base) {
    for (int si = 0x8; si > 0; si -= 2) {
        uint16_t list_ptr = (uint16_t)(ds_pool[LEDGE_SPRITE_LIST + si] |
                                        (ds_pool[LEDGE_SPRITE_LIST + si + 1] << 8));
        draw_block_list(base, list_ptr);
    }
}

/* draw_level2_background — literal port of level_number==2's randomized
 * block-puzzle background: clears 2 CGA banks to a striped pattern, then
 * places 40 (0x28) randomly-chosen block tiles (from 4 possible types),
 * never repeating the immediately-previous type, recording each choice
 * into level2_block_types for later gameplay logic (not consumed by
 * anything ported yet). */
static void draw_level2_background(void) {
    uint16_t di = 0;
    for (int i = 0; i < 0x50; i++) {
        cga_mem[di] = 0xaa; cga_mem[di + 1] = 0xaa;
        di = (uint16_t)(di + 2);
    }
    di = CGA_BANK1_BASE_OFS;
    for (int i = 0; i < 0x50; i++) {
        cga_mem[di] = 0xaa; cga_mem[di + 1] = 0xaa;
        di = (uint16_t)(di + 2);
    }

    g_block_prev_dl = 0xff;
    for (uint16_t block_count = 0; block_count < 0x28; block_count++) {
        uint8_t dl;
        for (;;) {
            dl = (uint8_t)(rnd_byte() & 0x18);
            if (dl != g_block_prev_dl) break;
        }
        g_block_prev_dl = dl;
        /* level2_block_types recorded for completeness, matching the
         * original — not yet consumed by any ported gameplay logic. */
        (void)LEVEL2_BLOCK_TYPES;

        uint16_t src_ofs = (uint16_t)(dl + 0x2020);
        uint16_t dest = (uint16_t)((block_count << 0) + 0xa0);
        blit_bytes_to_cga(&ds_pool[src_ofs], dest, 1, 4);
    }
}

/* draw_level3_bg — literal port of level_objects.asm's random fence-tile
 * generator, called at the end of level 3's background setup. Draws a
 * 16-column x 16-row grid of 8-row/1-word tiles starting at CGA offset
 * 0x66a, stride 0x140/row. Column 0 of every row is always tile 0x00;
 * the last column (byte offset 0x1e) is always tile 0x20 (fixed
 * end-caps). Interior columns pick from {0x10,0x30,0x40,0x50,0x60} via
 * a data-dependent RNG path that must consume exactly as many random()
 * calls as the original per column, since it shares the same LFSR
 * stream as everything else — see PROGRESS.md's "General lesson" on
 * not restructuring control flow away from a literal port. */
static void draw_level3_bg(void) {
    uint16_t row_base = 0x66a;

    for (int row_remaining = 0x10; row_remaining >= 1; row_remaining--) {
        uint8_t prev_tile = 0xff;

        for (uint16_t bx = 0;; bx += 2) {
            uint8_t al;
            if (bx == 0) {
                al = 0x00;
            } else if (bx == 0x1e) {
                al = 0x20;
            } else {
                int need_diff = (prev_tile == 0x50) || (row_remaining & 1);
                int got_al10 = 0;
                if (!need_diff) {
                    uint8_t r = rnd_byte();
                    if (r >= 0x40) {
                        al = 0x10;
                        got_al10 = 1;
                    } else {
                        need_diff = 1;
                    }
                }
                if (!got_al10) {
                    uint8_t r2 = rnd_byte();
                    al = (uint8_t)(((r2 - (uint8_t)bx) & 0x30) + 0x30);
                }
            }
            prev_tile = al;

            uint16_t src_ofs = (uint16_t)(LEVEL3_FENCE_TILES + al);
            uint16_t dest = (uint16_t)(row_base + bx);
            blit_to_cga(&ds_pool[src_ofs], dest, 1, 8);

            if (bx == 0x1e) break;
        }

        row_base = (uint16_t)(row_base + 0x140);
    }
}

/* init_level4_bg (background portion only) — literal port of
 * level_objects.asm's init_level4_bg, UP TO the masked sign-sprite blit.
 * Draws a 17-row randomized floor pattern (row tile-counts from
 * dat_3cae), then 4 static block-lists, then a small masked decorative
 * sprite. NOT ported: the routine's tail (initializing per-window
 * light/gap random state into dat_3ce3/dat_3ce4/dat_3cf3/dat_3cf4 from
 * [difficulty_level]) — that state is only consumed by an unported
 * level-4 object-update routine (level_objects.asm:1762), so it's left
 * out here, same pattern already used for init_level5_objects/
 * init_level6_objects elsewhere in this dispatcher. */
static void init_level4_bg(void) {
    uint16_t row_base = 0x506;

    for (int row = 0; row < 0x11; row++) {
        uint8_t tile_count = ds_pool[LEVEL4_ROW_TILE_COUNTS + row];
        uint16_t bx = 0;
        for (uint8_t i = 0; i < tile_count; i++) {
            uint8_t r = rnd_byte();
            uint16_t src_ofs;
            if (r > 0x30) {
                src_ofs = LEVEL4_TILE_A;
            } else if (r & 0x04) {
                src_ofs = LEVEL4_TILE_B;
            } else {
                src_ofs = LEVEL4_TILE_C;
            }
            uint16_t dest = (uint16_t)(row_base + bx);
            blit_to_cga(&ds_pool[src_ofs], dest, 1, 8);
            bx = (uint16_t)(bx + 2);
        }
        row_base = (uint16_t)(row_base + 0x140);
    }

    draw_block_list(0, LEVEL4_BLOCKS_1);
    draw_block_list(0, LEVEL4_BLOCKS_2);
    draw_block_list(0, LEVEL4_BLOCKS_3);
    draw_block_list(0, LEVEL4_BLOCKS_4);

    /* Masked decorative sign sprite: width=2 words, height=1 row.
     * mask_save is NULL — the original's [bp+0]=ds:0xe scratch save
     * isn't consumed by anything ported yet (see comment above). */
    blit_masked(&ds_pool[LEVEL4_SIGN_SPRITE], 0x8ec, 2, 1, NULL);
}

/* draw_level_background — literal port of score.asm's top dispatcher.
 * NOT ported: level 7 (delegates to draw_love_scene_bg, the victory
 * epilogue — roadmap item (e), separately scoped). Levels 1/3/4 added
 * in this pass; levels 2/5/6 were already ported. Note level_number is
 * never 0 here — the alley (level 0) doesn't call this at all, it uses
 * its own separate setup_alley path (see game_setup.c); the jump table
 * in entry.asm routes BOTH level-select indices 0 and 1 to the same
 * "level_number=1" handler, so level 1 is this dispatcher's fallthrough
 * default case. */
void draw_level_background(void) {
    if (level_number <= 0) {
        /* Defensive: the original never calls this for level_number==0
         * (the alley) — entry.asm's level-select jump table routes BOTH
         * indices 0 and 1 to the "level_number=1" indoor-room handler
         * (see below), and the alley itself uses its own separate
         * background path (setup_alley, not setup_level). Guard here so
         * an accidental call with level_number==0 can't draw an indoor
         * room's border over the alley scene. */
        return;
    }
    if (level_number == 2) {
        draw_level2_background();
        return;
    }
    if (level_number == 7) {
        /* draw_love_scene_bg — not ported, see roadmap item (e) */
        return;
    }
    if (level_number == 6) {
        draw_level_border(0);
        draw_block_list(0x64a, LEVEL_BG_BLOCK_LIST);
        entrance_x = 0x48;
        entrance_y = 0x38;
        draw_door_frame(0xdd2);
        draw_platform(0xdf6);
        blit_to_cga(&ds_pool[LEVEL6_DOOR_SPRITE], 0x67e, 2, 0x10);
        draw_block_list(0xb84, LEVEL_TILE_LIST_A);
        /* init_level6_objects — not ported (level-6-specific object
         * spawn setup, out of scope for this background-drawing pass) */
        return;
    }
    if (level_number == 5) {
        draw_level_border(0x640);
        draw_block_list(0xcb6, LEVEL_BG_BLOCK_LIST);
        entrance_x = 0xf8;
        entrance_y = 0x60;
        draw_door_frame(0x140e);
        draw_platform(0x1434);
        draw_platform(0x143e);
        draw_ledge(0x16a0);
        draw_block_list(0x1184, LEVEL_TILE_LIST_A);
        blit_to_cga(&ds_pool[LEVEL5_DOOR_SPRITE], 0xdd6, 2, 0x10);
        return;
    }
    if (level_number == 4) {
        draw_level_border(0x640);
        draw_block_list(0xcba, LEVEL_BG_BLOCK_LIST);
        entrance_x = 0x108;
        entrance_y = 0x60;
        draw_door_frame(0x1439);
        draw_block_pair(0x16c0);
        init_level4_bg();
        return;
    }
    if (level_number == 3) {
        draw_level_border(0x640);
        draw_block_list(0xc90, LEVEL_BG_BLOCK_LIST);
        entrance_x = 0x60;
        entrance_y = 0x60;
        draw_door_frame(0x140c);
        draw_platform(0x1418);
        draw_block_list(0x1184, LEVEL_TILE_LIST_A);
        draw_block_list(0x11a2, LEVEL_TILE_LIST_A);
        draw_block_list(0, LEVEL3_EXTRA_BLOCKS);
        draw_level3_bg();
        return;
    }
    /* Default case (level_number == 1): the fallthrough branch, see
     * comment above draw_level_background. */
    draw_level_border(0x640);
    draw_block_list(0xca0, LEVEL_BG_BLOCK_LIST);
    entrance_x = 0xa0;
    entrance_y = 0x60;
    draw_door_frame(0x1406);
    draw_block_list(0x11c4, LEVEL_TILE_LIST_A);
    draw_platform(0x1422);
    draw_ledge(0x1690);
    draw_block_pair(0x16b6);
}
