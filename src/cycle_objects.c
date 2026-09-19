#include "cat_state.h"
#include "alley.h"
#include "sound.h"
#include "cga.h"
#include "level_collision.h"
#include "cycle_objects.h"
#include "gen/object_sprites_ex.h"
#include "gen/obj_hit_sprites.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* BIOS `int 0x1a` tick substitute — same convention as enemy.c's
 * read_bios_tick / cga.c's RNG seeding. */
static uint16_t read_bios_tick(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms / 55); /* ~18.2 ticks/sec */
}

static uint8_t cycle_random_byte(void) {
    return (uint8_t)(cga_random() & 0xFF);
}

/* Stubs — score.asm/sound.asm not ported yet. Named distinctly from any
 * other file's stubs (no shared static-shadowing risk, see PROGRESS.md
 * §5q for why that matters). */
static void cycle_add_score(uint8_t points) { (void)points; /* TODO: score.asm */ }
/* sound.asm is ported now (src/sound.c), so these forward to the real
 * thing. Kept as distinctly-named wrappers rather than renaming every call
 * site, so the §5q shadowing hazard stays impossible here.
 * cycle_start_tone's two arguments are the original's AX and BX — see
 * sound.h on why start_tone takes both. */
static void cycle_start_tone(uint16_t ax_freq, uint16_t bx_freq) { start_tone(ax_freq, bx_freq); }
static void cycle_play_random_noise(void) { play_random_noise(); }
static void cycle_silence_speaker(void) { silence_speaker(); }
/* restore_alley_buffer/save_alley_buffer — real background-save pipeline
 * still isn't wired (see alley_movement.c/game_setup.c's own stubs of
 * the same functions, PROGRESS.md §5f/§5h); local no-op stand-ins here,
 * matching the same documented simplification rather than reaching for
 * another file's static (which would risk exactly the kind of silent
 * shadowing bug §5q found and fixed). */
static void cycle_restore_alley_buffer(void) { restore_alley_buffer(); }
static void cycle_save_alley_buffer(void) { save_alley_buffer(); }

/* Scratch save buffer for the level-2-climb-entry "collision" splash
 * sprite (8w x 6h = 48 words). The original passes a literal low address
 * (bp=0xe) here as its mask_save scratch pointer — not meaningful in
 * this port, so a real backing array is used instead, same convention as
 * obj_save_buf/enemy_save_buf. */
static uint16_t collision_save_buf[48];

/* init_objects — literal port (objects.asm). */
void init_cycle_objects(void) {
    obj_slot = 0;
    uint16_t ax = 0;
    int8_t dl = 1;
    if ((uint16_t)cat_x <= 0xa0) {
        ax = 0x12c;
        dl = -1;
    }
    obj_x[0] = obj_x[1] = obj_x[2] = ax;
    obj_dir[0] = obj_dir[1] = obj_dir[2] = dl;
    obj_hidden[0] = obj_hidden[1] = obj_hidden[2] = 1;
    obj_hit[0] = obj_hit[1] = obj_hit[2] = 0;
}

/* check_cycle_gravity_hit — literal port. Early-outs (false) whenever no
 * projectile is in flight, which is always true right now since
 * apply_cat_gravity/update_cat_jump (level_physics.asm) aren't ported —
 * see PROGRESS.md §5s. */
static bool check_cycle_gravity_hit(void) {
    if (gravity_y == 0) return false;
    uint16_t slot = obj_slot;
    return check_rect_collision((int16_t)obj_x[slot], obj_y[slot], 0x10, 0x08,
                                 gravity_x, gravity_y, 0x10, 0x0c);
}

/* erase_cycle_sprite — literal port. */
static void erase_cycle_sprite(void) {
    uint16_t slot = obj_slot;
    blit_to_cga((const uint8_t *)obj_save_buf[slot], obj_draw_pos[slot], 2, 8);
}

/* check_cycle_cat_collision — literal port. On a fresh (not-yet-flagged)
 * hit with the cat past a minimum height, either triggers the level-entry
 * "collision" sprite/climb-transition (if at_platform) or the knockback
 * animation + score + tone. */
static bool check_cycle_cat_collision(void) {
    uint16_t slot = obj_slot;
    bool hit = check_rect_collision((int16_t)obj_x[slot], obj_y[slot], 0x10, 0x08,
                                     (uint16_t)cat_x, cat_y, 0x18, 0x0e);
    if (!hit) return false;

    if (obj_hit[slot] == 0 && cat_y_bottom >= 0x26) {
        if (at_platform != 0) {
            /* lab_260e: knockback + score + tone */
            uint16_t tick = read_bios_tick();
            obj_hit[slot] = 1;
            obj_dir[slot] = 1;
            obj_hit_tick[slot] = tick;
            cycle_restore_alley_buffer();
            const cat_walk_frame_t *frame = &obj_hit_sprite_frames[slot];
            blit_transparent(frame->data, obj_draw_pos[slot], frame->width_words,
                              frame->height, NULL);
            cycle_save_alley_buffer();
            cycle_add_score(obj_score[slot]);
            cycle_start_tone(0x3e8, 0x2ee);
            return true;
        } else {
            /* lab_25cf: not at a platform -> arm the level-2 climb-entry
             * transition and draw the "collision" splash sprite. */
            at_platform = 0;
            transition_timer = 0x11;
            in_level_mode = 1;
            scroll_direction = 0;
            uint16_t di = obj_draw_pos[slot];
            if (obj_x[slot] >= 0x10) di -= 4;
            obj_collision_pos = di;
            blit_transparent(collision_sprite_frame.data, di,
                              collision_sprite_frame.width_words,
                              collision_sprite_frame.height, collision_save_buf);
            /* original plays a short randomized-noise burst here timed
             * against the BIOS tick (~8 ticks), then restores the
             * background it saved via blit_transparent's mask_save side
             * effect (§5p) — NOT a re-draw of the sprite itself, which
             * would be wrong (caught while porting: an early draft of
             * this function passed collision_sprite_frame.data back into
             * blit_to_cga here by mistake, which would just redraw the
             * splash instead of erasing it). */
            cycle_silence_speaker();
            cycle_play_random_noise();
            blit_to_cga((const uint8_t *)collision_save_buf, obj_collision_pos,
                        collision_sprite_frame.width_words, collision_sprite_frame.height);
            return true;
        }
    }
    return false;
}

/* cycle_animations — literal, label-for-label port of objects.asm's
 * per-tick dispatcher. Rate-limited to once per new BIOS tick; advances
 * exactly one of the 3 object slots (round-robin) per call, matching the
 * original. See PROGRESS.md §5s for the full derivation, including the
 * frame-index arithmetic for cycle_walk_sprite's 4 poses. */
void update_cycle_objects(void) {
    uint16_t tick_now = read_bios_tick();
    if (tick_now == obj_last_tick) return;
    obj_last_tick = tick_now;

    if (transitioning != 0) return;

    obj_slot = (uint16_t)((obj_slot + 1) % 3);

    if (check_cycle_gravity_hit()) return;
    if (check_cycle_cat_collision()) return;

    uint16_t slot = obj_slot;

    if (obj_hit[slot] != 0) {
        uint16_t tick2 = read_bios_tick();
        uint16_t dx = (uint16_t)(tick2 - obj_hit_tick[slot]);
        if (dx < 0x36) return;
        uint16_t ax = 0;
        int8_t dl = 1;
        if ((uint16_t)cat_x <= 0xa0) { ax = 0x12c; dl = -1; }
        obj_x[slot] = ax;
        obj_hit[slot] = 0;
        obj_dir[slot] = dl;
    }

    obj_prev_dir[slot] = obj_dir[slot];

    /* lab_23eb / lab_2403 / lab_240f / lab_2418: obj_speed computation +
     * the "same floor as cat, currently stopped -> random re-pick"
     * shortcut. When cycle_active is set, the original skips the
     * current_floor compare entirely and always falls into lab_2418;
     * otherwise it only falls into lab_2418 when on the cat's floor,
     * jumping straight to lab_2425 (below) if not. */
    uint8_t rnd = 0;
    if (cycle_active != 0) {
        obj_speed = 0xc;
        goto lab_2418;
    } else {
        uint16_t ax = 8;
        if (cat_y > 0x60) ax >>= 1;
        obj_speed = ax;
        if (slot != current_floor) goto lab_2425;
        /* fall through to lab_2418 */
    }
lab_2418:
    if (obj_dir[slot] != 0) goto lab_2425;
    rnd = cycle_random_byte();
    goto lab_248a;

lab_2425:
    if (at_platform != 0) {
        uint8_t al = obj_y[slot];
        if (al <= cat_y) {
            al = (uint8_t)(al + 0x10);
            if (al >= cat_y) {
                rnd = cycle_random_byte();
                if (rnd <= obj_chase_table[difficulty_level & 7]) {
                    obj_speed = 0xc;
                    int8_t dir = 1;
                    if (obj_x[slot] >= (uint16_t)cat_x) dir = -1;
                    obj_dir[slot] = dir;
                    goto lab_2496;
                }
            }
        }
    }
    {
        uint8_t cl = 0x18;
        if (cat_y > 0x60) {
            cl = 0x28;
            if (obj_dir[slot] == 0) cl = 0x10;
        }
        rnd = cycle_random_byte();
        if (rnd > cl) goto lab_2496;
        if (obj_dir[slot] != 0) {
            obj_dir[slot] = 0;
            goto lab_2496;
        }
        goto lab_248a;
    }

lab_248a:
    {
        /* al = rnd & 1; jnz lab_2492 (keep al as-is, i.e. al=1 -> dir=1);
         * else al=0xff (dir=-1). NOTE: al=1 here means obj_dir=1... but
         * the original's `mov byte [bx+obj_dir],al` at lab_2492 stores
         * al directly, and al was left as the masked 0/1 value on the
         * jnz-taken path — re-checking: `and al,0x1` sets al to 0 or 1;
         * `jnz lab_2492` (al!=0, i.e. al==1) jumps keeping al=1 -> dir=1.
         * The fall-through (al==0) sets al=0xff -> dir=-1. */
        uint8_t al = (uint8_t)(rnd & 0x1);
        obj_dir[slot] = (al != 0) ? 1 : -1;
        goto lab_2496;
    }

lab_2496: {
    int8_t dl = obj_dir[slot];
    int32_t ax = obj_x[slot];
    if ((uint8_t)dl == 0) {
        /* dl == 0: stopped, ax unchanged */
    } else if (dl == 1) {
        ax += obj_speed;
        if (ax >= 0x12f) { ax = 0x12e; dl = -1; }
    } else {
        /* dl == -1 (0xff): moving left */
        ax -= obj_speed;
        if (ax < 0) { ax = 0; dl = 1; }
    }
    obj_x[slot] = (uint16_t)ax;
    obj_dir[slot] = dl;
}

    {
        uint8_t bit_shift;
        size_t addr = calc_cga_addr(obj_y[slot], obj_x[slot], &bit_shift);
        obj_cga_tmp = (uint16_t)addr;
    }

    slot = obj_slot;
    if (obj_hidden[slot] == 0) {
        if (obj_dir[slot] != 0 || obj_prev_dir[slot] != 0) {
            erase_cycle_sprite();
        }
    }

    if (check_cycle_gravity_hit()) return;
    if (check_cycle_cat_collision()) return;

    slot = obj_slot;
    obj_hidden[slot] = 0;

    if (obj_dir[slot] != 0) {
        uint8_t idx = 0;
        obj_anim_toggle[slot]++;
        if ((obj_anim_toggle[slot] & 1) == 0) idx = 1;
        if (obj_dir[slot] == -1) idx = (uint8_t)(idx + 2);
        const cat_walk_frame_t *frame = &cycle_walk_sprite_frames[idx];
        obj_draw_pos[slot] = obj_cga_tmp;
        blit_masked(frame->data, obj_draw_pos[slot], frame->width_words, frame->height,
                    obj_save_buf[slot]);
    } else if (obj_prev_dir[slot] != 0) {
        const cat_walk_frame_t *frame = &cycle_idle_sprite_frame;
        obj_draw_pos[slot] = obj_cga_tmp;
        blit_masked(frame->data, obj_draw_pos[slot], frame->width_words, frame->height,
                    obj_save_buf[slot]);
    }
    /* else: dir==0 and prev_dir==0 -> stays fully idle/undrawn, matching
     * lab_254c's plain ret with no draw call. */
}
