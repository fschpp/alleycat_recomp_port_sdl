#include "cat_state.h"
#include "cga.h"
#include "level_collision.h"
#include "fall_object.h"
#include "gen/fall_sprite.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

static uint8_t rnd_byte(void) {
    return (uint8_t)(cga_random() & 0xFF);
}

static uint16_t read_bios_tick(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms / 55);
}

/* reset_jump — literal port. */
void reset_jump(void) {
    fall_counter = 0;
}

/* pick_random_target — literal port, reusing the already-verified door
 * tables (floor_door_count/floor_first_door/door_position_table, §5n).
 * Picks a random door on the current difficulty's floor (never repeating
 * the last one picked), returning its (x,y) as (cx,dl). */
static void pick_random_target(uint16_t *out_x, uint8_t *out_y) {
    uint8_t bx = difficulty_level & 7;
    uint8_t cl = floor_door_count[bx];
    uint8_t dl;

    for (;;) {
        for (;;) {
            dl = (uint8_t)(rnd_byte() & 0x7);
            if (dl <= cl) break;
        }
        dl = (uint8_t)(dl + floor_first_door[bx]);
        if (dl != last_target_door) break;
    }
    last_target_door = dl;

    uint8_t door_byte = door_position_table[dl];
    uint8_t y = (door_byte & 0x80) ? 0x88 : 0x90;
    uint16_t x = (uint16_t)((door_byte & 0x7f) << 2);
    *out_x = x;
    *out_y = y;
}

/* check_jump_collision — literal port, reusing the already-verified
 * check_rect_collision. */
bool check_jump_collision(void) {
    if (fall_counter == 0) return false;

    uint8_t a_h = (uint8_t)(fall_sprite_dims >> 8); /* height, dynamic 1-13 */
    bool hit = check_rect_collision((int16_t)fall_target_x, fall_cur_y, 0x10, a_h,
                                     (uint16_t)cat_x, cat_y, 0x18, 0x0e);
    if (hit) fall_hit = 1;
    return hit;
}

/* erase_jump_sprite — literal port, using the already-verified blit_to_cga. */
static void erase_jump_sprite(void) {
    if (fall_counter == 0x1a) return; /* freshly spawned this tick: nothing drawn yet to erase */
    uint8_t width_words = (uint8_t)(fall_save_dims & 0xff);
    uint8_t height = (uint8_t)(fall_save_dims >> 8);
    blit_to_cga((const uint8_t *)fall_save_buf, fall_draw_pos, width_words, height);
}

/* animate_falling — literal port of the main per-tick dispatcher. See
 * PROGRESS.md §5u for the full derivation of the height-varying draw
 * (fixed 2-word-wide fall_sprite, height read 1..13 while "rising" then
 * 13..0 while "falling", from the SAME fixed 52-byte block each time —
 * not a pointer-offset crop like the dog's unported partial-reveal
 * effect, just a variable row count, which made this fully portable). */
void animate_falling(void) {
    uint16_t now = read_bios_tick();
    if (now == fall_last_tick) return;
    /* check_vsync: always "ready" in this port, see enemy.c's convention */
    fall_last_tick = now;

    if (check_jump_collision()) return;

    if (fall_counter == 0) {
        if (cat_y != 0x86 && cat_y != 0x8e) {
            uint8_t r = rnd_byte();
            if (r > 5) return;
        }
        uint16_t tx;
        uint8_t ty;
        pick_random_target(&tx, &ty);
        ty = (uint8_t)(ty + 3);
        fall_target_y = ty;
        uint8_t r = rnd_byte();
        tx = (uint16_t)(tx + (r & 0x7) + 6);
        fall_target_x = tx;
        fall_counter = 0x1b;
    }

    fall_counter--;
    uint16_t cx = fall_target_x;
    uint8_t dl = fall_target_y;
    uint16_t dims;

    if (fall_counter <= 0x0d) {
        dl = (uint8_t)(dl + 0x0c - fall_counter);
        dims = (uint16_t)((fall_counter << 8) | 0x02);
    } else {
        dl = (uint8_t)(dl + fall_counter - 0x0f);
        dims = (uint16_t)(((0x1b - fall_counter) << 8) | 0x02);
    }

    fall_sprite_dims = dims;
    fall_cur_y = dl;
    fall_cga_addr = (uint16_t)calc_cga_addr(fall_cur_y, cx, NULL);

    erase_jump_sprite();

    if (check_jump_collision() || fall_counter == 0) return;

    fall_draw_pos = fall_cga_addr;
    fall_save_dims = fall_sprite_dims;
    uint8_t height = (uint8_t)(fall_sprite_dims >> 8);
    if (height == 0) return; /* nothing to draw this frame */
    blit_transparent(fall_sprite_data, fall_draw_pos, 2, height, fall_save_buf);
}
