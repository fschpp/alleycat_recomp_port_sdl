#include "cat_state.h"
#include "cga.h"
#include "level_collision.h"
#include "level_objects.h"
#include "enemy.h"
#include "gen/ds_pool.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* BIOS `int 0x1a` tick substitute — same convention used throughout this
 * port (enemy.c, level3_enemy.c, cga.c's RNG seeding). */
static uint16_t read_bios_tick(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms / 55); /* ~18.2 ticks/sec */
}

static uint8_t objects_random_byte(void) {
    return (uint8_t)(cga_random() & 0xFF);
}

/* --- ds_pool offsets, all confirmed against cat.asm's raw db runs by
 * chaining sizes from known-good anchors (l3_platform_id/dat_39cc style
 * cross-check — see PROGRESS.md). No new extraction tooling needed, same
 * as §5x/§5y: everything is already inside the wholesale-embedded
 * ds_pool, just indexed at its real DS offset. ---
 *
 *   dat_3260 (0x3260): a 13-entry (12 + 0x0000 terminator) WORD table of
 *   pointers to 7 unique 120-byte bounce-animation frames living at
 *   [0x2ea0, 0x31e8) — the sequence rises through all 7 then falls back
 *   through 6..1, forming a symmetric up/down wobble, then loops.
 *   l1_sprite_data (0x31e8, 120 bytes = 2 words x 30 rows): NOT sprite
 *   pixel data despite the name — it's the runtime background-save
 *   buffer for the thrown object's OR-blit (all-zero in the original
 *   .data, confirmed by inspection), so it's a plain static array here
 *   instead, same as l3_bird_mask_save in level3_enemy.c.
 *   l1_obj_sprites (0x32b8, 50 bytes = 5 frames x 1 word x 5 rows): the
 *   real footprint-decal tile bitmaps, indexed by wear level 0-4.
 *   dat_32f2 (0x32f2): 7-entry word table, thrown-object spawn interval
 *   in ticks per level (levels 0-6 only — level 7 has its own separate
 *   spawn_thrown_object system, out of scope here, see level_objects.h).
 */
#define L1_THROWN_SPRITE_PTRS 0x3260
#define L1_OBJ_SPRITES         0x32b8
#define L1_RATE_TABLE          0x32f2
#define L1_FOOTPRINT_ROW_BASE  0x1e00 /* dat_1e00 — a CGA offset constant, not sprite data */

/* --- runtime state --- */
static uint16_t l1_sprite_data[60]; /* dat_31e8-equivalent: 2 words x 30 rows background save */
static uint8_t  l1_bg_sprite[40];   /* per-column footprint/impact wear counter, 0-4 */
static uint16_t l1_last_col;        /* dat_32b6 */
static uint16_t l1_anim_frame;      /* dat_327a — index (0,2,4,...) into L1_THROWN_SPRITE_PTRS */
static uint8_t  l1_dir_x;           /* dat_3280: 0=none, 1=right, 0xff=left */
static uint8_t  l1_dir_y;           /* dat_3281: 0=none, 1=down, 0xff=up */
static uint16_t l1_draw_addr;       /* dat_3282 — last drawn CGA addr, for erase */
static uint16_t l1_next_draw_addr;  /* dat_3284 */
static uint8_t  l1_undrawn;         /* dat_3286: nonzero = nothing drawn yet, skip erase */
static uint16_t l1_saved_x;         /* dat_32ef */
static uint8_t  l1_saved_y;         /* dat_32f1 */
static uint16_t l1_last_tick;       /* dat_328c */
static uint16_t l1_col_index;       /* dat_328a — clamped [0, 0x26] column into l1_bg_sprite */
static uint8_t  l1_noise_counter;   /* dat_32ea */
static uint8_t  l1_noise_accum;     /* dat_32eb */
static uint8_t  l1_bias_mag;        /* dat_32ec */
static uint8_t  l1_bias_x;          /* dat_32ed */
static uint8_t  l1_bias_y;          /* dat_32ee */

/* dat_410 — a slow-drift "how long since the level started" tick
 * reference, read but never written by tick_thrown_objects itself, so it
 * must be set elsewhere (unported). Latched here at init_thrown_objects
 * time (called once per level entry, per entry.asm) as the closest
 * faithful equivalent — a documented simplification, see PROGRESS.md. */
static uint16_t l1_level_start_tick;

static bool check_thrown_cat_hit(void) {
    if (enemy_active != 0) return false;

    /* level_number==6 special case (check_thrown_near_cat, gated on the
     * unported dat_44bd flag) always takes the normal path below since
     * that flag can never be set yet — documented simplification. */

    bool hit = check_rect_collision((int16_t)thrown_obj_x, thrown_obj_y, 0x10, 0x1e,
                                     cat_x, cat_y, 0x18, 0x0e);
    if (!hit) return false;

    /* level==4 with an active door-open animation skips the auto-walk
     * trigger; l3_door_anim_frame belongs to the unported level-3/4 door
     * subsystem so it defaults to 0 (never skips) — documented
     * simplification. start_auto_walk (game_loop.asm) isn't ported
     * either; both are no-ops here. */
    return true;
}

static void erase_l1_thrown(void) {
    if (l1_undrawn == 0) {
        blit_to_cga((const uint8_t *)l1_sprite_data, l1_draw_addr, 2, 30);
    }
}

static void draw_l1_thrown(void) {
    uint16_t frame_ofs = (uint16_t)(ds_pool[L1_THROWN_SPRITE_PTRS + l1_anim_frame] |
                                     (ds_pool[L1_THROWN_SPRITE_PTRS + l1_anim_frame + 1] << 8));
    if (frame_ofs == 0) {
        /* end of the pointer table — loop back to the first frame */
        l1_anim_frame = 0;
        frame_ofs = (uint16_t)(ds_pool[L1_THROWN_SPRITE_PTRS] | (ds_pool[L1_THROWN_SPRITE_PTRS + 1] << 8));
    }
    l1_draw_addr = l1_next_draw_addr;
    l1_undrawn = 0;
    blit_or(&ds_pool[frame_ofs], l1_draw_addr, 2, 30, l1_sprite_data);
}

static void draw_footprint_tile(uint8_t frame, uint16_t col) {
    const uint8_t *tile = &ds_pool[L1_OBJ_SPRITES + (size_t)frame * 10];
    size_t dst = L1_FOOTPRINT_ROW_BASE + (size_t)col * 2;
    blit_to_cga(tile, dst, 1, 5);
}

void update_footprint(void) {
    if (cat_y < 0xb4) return;
    if (scroll_direction == 0) return;

    uint16_t col = (uint16_t)((cat_x + 0xc) >> 3);
    if (col > 0x27) return;
    if (col == l1_last_col) return;
    l1_last_col = col;

    uint8_t wear = l1_bg_sprite[col];
    if (wear >= 4) return;
    wear++;
    l1_bg_sprite[col] = wear;
    draw_footprint_tile(wear, col);
}

void init_thrown_objects(void) {
    for (int i = 0; i < 40; i++) l1_bg_sprite[i] = 0;
    l1_last_col = 0xffff;
    l1_anim_frame = 0;
    thrown_obj_x = 0;
    thrown_obj_y = 0xa0;
    l1_undrawn = 1;
    l1_dir_x = 0;
    l1_dir_y = 0;
    l1_noise_accum = objects_random_byte();
    l1_noise_counter = 0x6c;
    l1_level_start_tick = read_bios_tick();
}

void tick_thrown_objects(void) {
    uint16_t tick_now = read_bios_tick();
    uint16_t rate_idx = (uint16_t)((level_number & 7) * 2);
    uint16_t rate = (uint16_t)(ds_pool[L1_RATE_TABLE + rate_idx] |
                                (ds_pool[L1_RATE_TABLE + rate_idx + 1] << 8));

    if ((uint16_t)(tick_now - l1_last_tick) < rate) return;
    l1_last_tick = tick_now;

    if (check_thrown_cat_hit()) return;
    if (check_enemy_object_hit()) return;

    l1_noise_counter++;
    uint8_t r = objects_random_byte();
    l1_noise_accum = (uint8_t)(l1_noise_accum ^ (l1_noise_counter & r));

    /* direction bias toward the cat, computed fresh every tick (8/16-bit
     * wraparound arithmetic, matching the original's byte/word ops
     * exactly rather than using a wider signed type) */
    {
        uint16_t ax_val = (uint16_t)((uint16_t)thrown_obj_x - (uint16_t)cat_x);
        bool dx_borrow = (uint16_t)thrown_obj_x < (uint16_t)cat_x;
        uint16_t dx;
        if (!dx_borrow) {
            l1_bias_x = 0xff;
            dx = ax_val;
        } else {
            l1_bias_x = 1;
            dx = (uint16_t)(~ax_val);
        }

        uint8_t bl_val = (uint8_t)(thrown_obj_y + 0x14);
        bool dy_borrow = bl_val < cat_y;
        bl_val = (uint8_t)(bl_val - cat_y);
        uint8_t dy_dist;
        if (!dy_borrow) {
            l1_bias_y = 0xff;
            dy_dist = bl_val;
        } else {
            l1_bias_y = 1;
            dy_dist = (uint8_t)(~bl_val);
        }

        uint8_t al = (uint8_t)(dx >> 2);
        l1_bias_mag = (uint8_t)(al + (uint8_t)(dy_dist >> 1));
    }

    /* clamp the footprint-column cursor */
    if (l1_col_index >= 0x27) l1_col_index = 0x26;

    if (l1_bg_sprite[l1_col_index] != 0) {
        /* --- column already carries a footprint mark: steer toward it
         * and force a continued descent (lab_324b) --- */
        l1_dir_y = 1;
        uint16_t col_x = (uint16_t)(l1_col_index << 3);
        if (thrown_obj_x == (int16_t)col_x) {
            /* aligned with the marked column (lab_3269) */
            l1_dir_x = 0;
            if (thrown_obj_y == 0xa5) {
                /* reached the ground row */
                l1_dir_y = 0;
                if (l1_anim_frame == 6 || l1_anim_frame == 0x12) {
                    /* fully erase the falling sprite at its current
                     * (not next) position, "splash" the impact into the
                     * footprint tile, and mark undrawn so the tail
                     * erase_l1_thrown() below is a no-op this tick */
                    blit_to_cga((const uint8_t *)l1_sprite_data, l1_draw_addr, 2, 30);
                    if (l1_bg_sprite[l1_col_index] > 0) l1_bg_sprite[l1_col_index]--;
                    draw_footprint_tile(l1_bg_sprite[l1_col_index], l1_col_index);
                    l1_undrawn = 1;
                }
            }
        } else {
            l1_dir_x = (thrown_obj_x < (int16_t)col_x) ? 1 : 0xff;
        }
    } else {
        /* --- fresh column: move the cursor on, then pick a (possibly
         * biased, possibly random) new bounce direction --- */
        l1_col_index = (uint16_t)(l1_col_index - 1);

        uint16_t elapsed = (uint16_t)((tick_now - l1_level_start_tick) >> 3);
        uint8_t al = l1_bias_mag;
        if (al < (uint8_t)elapsed) {
            al = 0;
        } else {
            al = (uint8_t)(al - (uint8_t)elapsed);
        }

        if (al >= l1_noise_accum) {
            l1_dir_y = 1; /* always bounce down when "enough time" has passed */
            uint8_t r2 = objects_random_byte();
            if (r2 == 0) {
                l1_dir_x = 0;
            } else if (r2 > 7) {
                /* leave l1_dir_x unchanged */
            } else {
                l1_dir_x = (r2 & 1) ? 1 : 0xff;
            }
        } else {
            uint8_t masked = (uint8_t)(l1_noise_accum & 0x2f);
            if (masked == 0) {
                uint8_t r2 = objects_random_byte();
                l1_dir_x = (r2 & 1) ? 1 : 0xff;
                uint8_t r3 = objects_random_byte();
                l1_dir_y = (r3 & 1) ? 1 : 0xff;
            } else if ((l1_noise_accum & 0x7) == 0) {
                l1_dir_x = l1_bias_x;
                l1_dir_y = l1_bias_y;
            }
            /* else: leave l1_dir_x/l1_dir_y unchanged this tick */
        }
    }

    /* --- apply movement (shared tail, lab_32ac) --- */
    uint16_t new_x = thrown_obj_x;
    uint8_t new_y = thrown_obj_y;
    l1_saved_x = thrown_obj_x;
    l1_saved_y = thrown_obj_y;

    if (l1_dir_x == 1) {
        new_x = (uint16_t)(new_x + 8);
        if (new_x >= 0x131) new_x = 0x130;
    } else if (l1_dir_x == 0xff) {
        int32_t t = (int32_t)new_x - 8;
        new_x = (t < 0) ? 0 : (uint16_t)t;
    }
    new_x &= 0xfff8;
    thrown_obj_x = (int16_t)new_x;

    if (l1_dir_y == 1) {
        int32_t t = (int32_t)new_y + 2;
        new_y = (t >= 0xa6) ? 0xa5 : (uint8_t)t;
    } else if (l1_dir_y == 0xff) {
        int32_t t = (int32_t)new_y - 2;
        new_y = (t < 0) ? 0 : (uint8_t)t;
    }
    thrown_obj_y = new_y;

    l1_next_draw_addr = (uint16_t)calc_cga_addr(new_y, new_x, NULL);

    if (check_thrown_cat_hit() || check_enemy_object_hit()) {
        l1_dir_x = 0;
        l1_dir_y = 0;
        thrown_obj_x = (int16_t)l1_saved_x;
        thrown_obj_y = l1_saved_y;
        return;
    }

    erase_l1_thrown();
    l1_anim_frame = (uint16_t)(l1_anim_frame + 2);
    draw_l1_thrown();
}
