#include "cat_state.h"
#include "cga.h"
#include "level_collision.h"
#include "gen/level_geometry.h"
#include <stdint.h>
#include <stdbool.h>

/* play_hit_sound (sound.asm) — plays a sound on door collision. No-op
 * until sound.asm is ported. */
static void play_hit_sound(void) { /* TODO: sound.asm */ }

uint16_t entrance_x = 0;
uint8_t  entrance_y = 0;
uint8_t  platform_cur_type = 0;
uint16_t platform_cur_width = 0;
uint8_t  door_hit_flag = 0;

/* check_rect_collision — literal port of level_objects.asm's real
 * algorithm (already commented in the disassembly repo itself). AABB
 * overlap test between rect A (ax=x, dl=y, si=width, cl=height) and
 * rect B (bx=x, dh=y, di=width, ch=height), matching the exact register
 * mapping confirmed against check_level_platform's real call site. */
bool check_rect_collision(int16_t a_x, uint8_t a_y, uint16_t a_w, uint8_t a_h,
                                  uint16_t b_x, uint8_t b_y, uint16_t b_w, uint8_t b_h) {
    uint16_t a_right = (uint16_t)a_x + a_w;
    if (a_right < b_x) return false; /* jc: unsigned */

    int32_t clamped_x = (int32_t)a_x - (int32_t)b_w;
    if (clamped_x < 0) clamped_x = 0;
    if ((uint16_t)clamped_x > b_x) return false; /* ja: unsigned */

    uint8_t a_bottom = (uint8_t)(a_y + a_h);
    if (a_bottom < b_y) return false;

    int16_t clamped_y = (int16_t)a_y - (int16_t)b_h;
    if (clamped_y < 0) clamped_y = 0;
    if ((uint8_t)clamped_y > b_y) return false;

    return true;
}

/* check_door_position — literal port of level_physics.asm. Scans
 * door_position_table starting at floor_first_door[difficulty_level]
 * until a 0x00 sentinel; each byte packs a floor-row flag (bit 7) and a
 * column (bits 0-6, <<2 for pixel X). On hit, snaps cat_y to the door's
 * row and plays the catch/hit sound the first time contact is made. */
bool check_door_position(void) {
    uint8_t cl = (uint8_t)(cat_y + 2);
    cl &= 0xf8;

    uint8_t bx = floor_first_door[difficulty_level & 7];

    for (;;) {
        uint8_t al = door_position_table[bx];
        if (al == 0) {
            door_contact = 0;
            return false;
        }
        bx++;

        uint8_t ch = (al & 0x80) ? 0x88 : 0x90;
        if (cl != ch) continue;

        uint16_t ax = (uint16_t)((al & 0x7f) << 2);
        uint16_t dx = (uint16_t)(cat_x & 0xfff8);
        if (dx < ax) continue;

        dx = (uint16_t)((cat_x - 0xf) & 0xfff8);
        if (dx > ax) continue;

        ch = (uint8_t)(ch - 2);
        cat_y = ch;
        cat_y_bottom = (uint8_t)(ch + 0x32);

        if (door_contact == 0) {
            door_contact = 1;
            play_hit_sound();
        }
        return true;
    }
}

/* check_fence_collision — literal port of level_objects.asm (level 3's
 * fixed fence obstacle — a single hardcoded rectangular region, no
 * table lookup needed). */
bool check_fence_collision(void) {
    uint16_t ax = (uint16_t)(cat_x & 0xfffc);
    if (ax < 0xa4 || ax > 0x118) return false;

    uint8_t dl = (uint8_t)(cat_y - 2);
    dl &= 0xf8;
    if (!(dl & 0x8)) return false;
    if (dl < 0x28 || dl > 0xa0) return false;

    cat_x = (int16_t)ax;
    dl = (uint8_t)(dl + 2);
    cat_y = dl;
    cat_y_bottom = (uint8_t)(dl + 0x32);
    return true;
}

/* check_level_platform — literal port of level_physics.asm. Scans the
 * current level's platform list (level_platform_index[level_number] as
 * the starting index into the shared platform_y_table/type_table/
 * width_table/x_left arrays) for a platform whose Y-band matches
 * cat_y&0xf8 AND whose horizontal test passes (see below — NOT a simple
 * "cat_x within [x_left, x_left+width)" range; the original tests the
 * platform's left edge against a window derived from cat_x and the
 * platform's own width). Level 3 additionally checks the fence first.
 * When in_level_mode is already 1 (already climbing), an extra
 * check_rect_collision against a fixed "entrance" rectangle is performed
 * first (matching the original's dedicated door/entrance re-check while
 * already inside). */
bool check_level_platform(void) {
    if (level_number == 3 && check_fence_collision()) return true;

    if (in_level_mode == 1) {
        if (check_rect_collision((int16_t)(entrance_x - 4), (uint8_t)(entrance_y - 8), 0xc, 0x10,
                                  (uint16_t)cat_x, cat_y, 0x18, 0xe)) {
            return true;
        }
    }

    uint8_t cl = (uint8_t)(cat_y & 0xf8);
    uint8_t bx = (uint8_t)level_platform_index[level_number & 7];

    for (;;) {
        uint8_t ch = platform_y_table[bx];
        if (ch == 0) return false;

        uint8_t al = platform_type_table[bx];
        platform_cur_type = al;
        platform_cur_width = platform_width_table[bx];
        uint16_t x_left = platform_x_left[bx];
        uint8_t this_bx = bx;
        bx++;

        if (cl != ch) continue;

        uint16_t dx = (uint16_t)(cat_x & 0xfff8);
        if (dx < x_left) continue; /* jc: cat is left of the platform's left edge */

        int32_t clamped = (int32_t)cat_x - (int32_t)platform_cur_width;
        if (clamped < 0) clamped = 0;
        dx = (uint16_t)((uint16_t)clamped & 0xfffc);
        if (dx > x_left) continue; /* ja */

        /* match */
        cat_y = ch;
        cat_y_bottom = (uint8_t)(ch + 0x32);
        at_platform = platform_cur_type;
        if (platform_cur_type != 0) {
            cat_x = (int16_t)(cat_x & 0xfffc);
        }

        if (level_number == 4) {
            /* original: after the loop's "inc bx" already happened, it
             * does dec bx (back to this_bx), sub bx,0x27, and only sets
             * l3_platform_id if that falls in [0,0x10). */
            int16_t rel = (int16_t)this_bx - 0x27;
            if (rel >= 0 && rel < 0x10) {
                l3_platform_id = (uint8_t)(rel + 1);
            }
        }
        return true;
    }
}

/* check_level_collision — literal port of level_physics.asm's dispatcher.
 *
 * NOT PORTED (needs the window-animation state machine — window_open_state/
 * window_row_offset/current_floor/window_column, itself dependent on
 * spawn_window_event, still a stub per §5h/§5m): check_stairs_collision
 * (level 7) and check_window_landing (level 0). Both are honestly left
 * unported rather than guessed — calling check_level_collision on level 0
 * or 7 currently just returns "no collision" via this stub path. */
bool check_level_collision(void) {
    if (level_number == 7) {
        /* check_stairs_collision — not ported, see above */
        return false;
    }
    if (level_number != 0) {
        return check_level_platform();
    }
    /* level 0: original checks `cat_y & 0xf8 == 0x60` for a specific
     * ledge, else falls to check_door_position/check_window_landing.
     * check_window_landing not ported (see above) — check_door_position
     * alone is still meaningful and ported. */
    return check_door_position();
}
