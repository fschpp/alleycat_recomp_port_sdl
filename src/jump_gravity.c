#include "cat_state.h"
#include "cga.h"
#include "level_collision.h"
#include "jump_gravity.h"
#include "gen/enemy_verified_sprites.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stddef.h>

/* random() substitute — same convention as movement.c/enemy.c: low byte
 * of the shared 16-bit LFSR. */
static uint8_t rnd_byte(void) {
    return (uint8_t)(cga_random() & 0xFF);
}

/* BIOS int 0x1a tick substitute — same convention as enemy.c. */
static uint16_t read_bios_tick(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms / 55);
}

/* decode_enemy_params — literal port. Splits a packed spawn-parameter
 * byte into an (x,y) spawn position via the verified jump_spawn_x_table/
 * jump_spawn_y_table (4 entries each). */
static void decode_enemy_params(uint8_t dl, uint16_t *out_x, uint8_t *out_y) {
    *out_x = jump_spawn_x_table[dl & 0x3];
    *out_y = jump_spawn_y_table[(dl >> 2) & 0x3];
}

/* check_fish_collision — literal port. Tests the jumping fish's current
 * position against the cat via the already-verified check_rect_collision;
 * additionally latches door_hit_flag (the original's byte[0x551], an
 * extra "cat got hit" signal not yet consumed elsewhere in this port)
 * under specific extra conditions once a collision is found. */
static bool check_fish_collision(void) {
    if (!check_rect_collision((int16_t)jump_x, jump_y, 0x20, 0x0f,
                               (uint16_t)cat_x, cat_y, 0x18, 0x0e)) {
        return false;
    }
    if (in_level_mode == 1 && transitioning == 0 && (uint8_t)cat_y < 0x60 &&
        jump_anim_counter >= 0x5 && jump_anim_counter < 0x19) {
        door_hit_flag = 1;
    }
    return true;
}

/* check_trashcan_near — literal port. Tests the jumping fish's landing
 * spot against one of the 3 ground-patrol objects' positions (obj_x[1]
 * or obj_x[2], depending on jump_spawn_param's bit 2 — obj_x[0] is never
 * consulted here, matching the original exactly). */
static bool check_trashcan_near(void) {
    uint8_t al = jump_spawn_param;
    if (al >= 8) return false;

    uint8_t slot = (al & 0x4) ? 2 : 1;
    int32_t ax = (int32_t)obj_x[slot] + 0x10;
    if ((uint16_t)ax < jump_x) return false;

    ax -= 0x30;
    if (ax < 0) ax = 0;
    if ((uint16_t)ax > jump_x) return false;

    return true;
}

/* restore_gravity_bg — literal port, using the already-verified blit_to_cga. */
static void restore_gravity_bg(void) {
    uint8_t width_words = (uint8_t)(gravity_restore_dims & 0xff);
    uint8_t height = (uint8_t)(gravity_restore_dims >> 8);
    blit_to_cga((const uint8_t *)gravity_save_buf, gravity_prev_cga, width_words, height);
}

/* apply_cat_gravity — literal port of the falling-projectile animation.
 * Runs the fall arc (horizontal drift + vertical descent toward
 * gravity_target_height), and draws the projectile via the now-fixed
 * blit_transparent once armed.
 *
 * NOT ported (documented, not guessed): the original calls
 * check_dog_collision at this point — per §5o, that's actually level-0
 * gravity-fall LANDING detection, not the dog. This port always treats
 * the fall as "not yet landed on that special target" and proceeds with
 * the normal draw, which is correct for the vast majority of cases (the
 * projectile just falls toward gravity_target_height and eventually
 * lands via the height check below). */
void apply_cat_gravity(void) {
    if (gravity_y == 0) return;

    uint16_t now = read_bios_tick();
    if (now == gravity_last_tick) return;
    gravity_last_tick = now;

    if (idle_aggro_flag != 0) {
        uint16_t gx = (uint16_t)(gravity_x & 0xfff8);
        uint16_t cx = (uint16_t)((uint16_t)cat_x & 0xfff8);
        if (gx == cx) gravity_drift_dir = 0;
    }

    gravity_frame++;
    if (gravity_h_speed <= 1) gravity_h_speed--;

    {
        uint16_t ax = gravity_x;
        uint8_t dx = (uint8_t)(gravity_h_speed >> 3);
        uint8_t dir = (uint8_t)gravity_drift_dir;
        if (dir < 1) {
            /* dir == 0: no horizontal drift */
        } else if (dir == 1) {
            ax = (uint16_t)(ax + dx);
            if (ax >= 0x12f) ax = 0x12e;
        } else {
            /* dir == 0xff (-1) */
            if (ax >= dx) ax = (uint16_t)(ax - dx);
            else ax = 0;
        }
        gravity_x = ax;
    }

    uint8_t al = (uint8_t)(gravity_frame >> 1);
    al = (uint8_t)(al + gravity_y);
    uint8_t dl = al;

    if (al >= gravity_target_height) {
        gravity_y = 0;
        deduct_life = 0;
        restore_gravity_bg();
        return;
    }

    gravity_y = dl;
    gravity_save_dims = gravity_cur_dims;
    gravity_cga_addr = (uint16_t)calc_cga_addr(gravity_y, gravity_x, NULL);

    if (gravity_frame != 2) {
        restore_gravity_bg();
    }

    gravity_prev_cga = gravity_cga_addr;
    gravity_restore_dims = gravity_save_dims;

    if (gravity_cur_sprite != NULL) {
        blit_transparent(gravity_cur_sprite->data, gravity_cga_addr,
                          gravity_cur_sprite->width_words, gravity_cur_sprite->height,
                          gravity_save_buf);
    }
}

/* update_cat_jump — literal port of the main per-tick dispatcher: spawns
 * the jumping fish (randomly, more often when the cat is idle too long),
 * animates its jump arc, and on landing near the cat, arms a gravity
 * toss (apply_cat_gravity takes over from there). */
void update_cat_jump(void) {
    jump_tick_delay--;
    if (jump_tick_delay != 0) return;
    jump_tick_delay = 0x0d;
    /* check_vsync: always "ready" in this port, see enemy.c's convention */

    if (jump_anim_counter != 0) {
        check_fish_collision();
    }

    if (gravity_y != 0) return;

    if (jump_anim_counter == 0) {
        if ((uint8_t)cat_y > 0x60) return;
        idle_aggro_flag = 0;

        if (game_mode == 1 /* && byte[0x418]==0, unidentified flag, treated as 0 */) {
            uint16_t now = read_bios_tick();
            /* [0x556] unidentified tick-reference; treated as 0, so this
             * always measures "time since our own clock start" rather
             * than a real reference point — a documented simplification. */
            if (now >= 0x48) idle_aggro_flag = 1;
        }

        uint8_t r = rnd_byte();
        uint8_t roll;
        if (idle_aggro_flag != 0) {
            roll = (uint8_t)(r & 0x3);
        } else {
            roll = (uint8_t)(r & 0xf);
            if (roll >= 0xc) return;
        }

        jump_spawn_param = roll;
        decode_enemy_params(roll, &jump_x, &jump_y);
        if (check_fish_collision()) return; /* spawn position blocked, try again next tick */

        jump_anim_counter = 0x1d;
        jump_toss_delay = jump_pause_by_diff[difficulty_level & 7];
        jump_toss_remaining = 1;
    }

    if (check_fish_collision()) return;

    cycle_active = 0;
    if (!check_trashcan_near()) return;
    cycle_active = 1;

    if (jump_anim_counter == 0x10) {
        jump_toss_tick = read_bios_tick();
    }

    if (jump_anim_counter != 0x0f) goto animate_arc;

    {
        uint16_t now = read_bios_tick();
        uint16_t dt = (uint16_t)(now - jump_toss_tick);
        if (dt < jump_toss_delay) goto animate_arc;
        if (jump_toss_remaining == 0) return;
        if (gravity_y != 0) return;
        /* byte[0x418] unidentified flag, treated as 0 */

        jump_toss_remaining--;
        deduct_life = 1;
        gravity_y = jump_y;

        uint8_t r = rnd_byte();
        uint16_t gx = (uint16_t)((r & 0xf) + jump_x);
        gravity_x = gx;
        gravity_drift_dir = (gx < (uint16_t)cat_x) ? 1 : 0xff;

        r = rnd_byte();
        uint8_t bx = (uint8_t)(r & 0x6);
        gravity_cur_sprite = &gravity_sprite[bx >> 1];
        gravity_target_height = gravity_height_table[bx >> 1];
        gravity_h_speed = 0x20;
        gravity_frame = 1;
        dog_catch_flag = 0;
    }
    return;

animate_arc:
    jump_anim_counter--;
    {
        uint8_t dl = jump_y;
        if (jump_anim_counter <= 0x0e) {
            dl = (uint8_t)(dl + 0x0e - jump_anim_counter);
        } else {
            dl = (uint8_t)(dl + jump_anim_counter - 0x0e);
        }
        jump_draw_y = dl;
        /* original then draws via a jump_land_sprite_data lookup for the
         * landing effect and otherwise a normal sprite blit — not ported:
         * this demo doesn't yet have the fish's own walking/jumping
         * sprite bitmap extracted (a distinct sprite category from
         * death/gravity/enemy/cycle), so the arc animates the underlying
         * state correctly but has no visible sprite of its own yet. */
    }
}
