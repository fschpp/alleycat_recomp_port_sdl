#include "cat_state.h"
#include "sound.h"
#include "cga.h"
#include "level_collision.h"
#include "gen/level_geometry.h"
#include "gen/ds_pool.h"
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

/* play_hit_sound now comes from the real sound.asm port (src/sound.c). */

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

/* --- T10: window landing (nivel 0) ------------------------------------- */

#define DS_THROW_COL_DATA   0x1016u  /* 15 bytes mutables (throw_col_data[]) */
#define DS_WINDOW_ROW_OFFS  0x1025u  /* 3 bytes const: 00 05 0a */

static uint16_t read_bios_tick_lc(void) { /* int 0x1a, ver alley.c */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms / 55);
}

/* Lectura de un byte DS en [bx+si+0x1016]. Normalmente cae dentro de
 * throw_col_data[15]; si el índice se sale (cat_x fuera de rango, el original
 * leería DS vecino) se devuelve el byte const de ds_pool, o 0 fuera del pool. */
static uint8_t ds_read_throw(uint32_t idx) {
    uint32_t off = DS_THROW_COL_DATA + idx;
    if (idx < sizeof(throw_col_data)) return throw_col_data[idx];
    if (off < DS_POOL_SIZE) return ds_pool[off];
    return 0;
}

/* pixel_to_bitmask — literal.
 *   mov cl,3 / shr bx,cl      ; bx = px>>3
 *   mov ch,bl                 ; ch = (px>>3) low byte
 *   mov cl,3 / shr bx,cl      ; bx = px>>6  (índice de byte)
 *   mov cl,ch / and cl,7      ; cl = (px>>3)&7
 *   mov ch,0x80 / shr ch,cl   ; máscara */
uint8_t pixel_to_bitmask(uint16_t px, uint16_t *byte_index) {
    uint16_t bx = (uint16_t)(px >> 3);
    uint8_t ch = (uint8_t)bx;
    bx = (uint16_t)(bx >> 3);
    uint8_t cl = (uint8_t)(ch & 7);
    if (byte_index) *byte_index = bx;
    return (uint8_t)(0x80u >> cl);
}

/* check_window_landing — literal port de level_physics.asm L209-262. */
bool check_window_landing(void) {
    uint8_t dl = (uint8_t)(cat_y & 0xf8);
    uint16_t bx = 0;
    if (dl == 0x08) goto lab_17c9;
    bx++;
    if (dl == 0x28) goto lab_17c9;
    bx++;
    if (dl != 0x48) return false;           /* lab_1810: clc; ret */

lab_17c9: {
        uint16_t ax = (uint16_t)cat_x;
        if (bx != current_floor) goto lab_17fc;
        if (window_column > 0x3) goto lab_17fc;   /* ja: sin signo */
        if ((uint8_t)bx == 1) goto lab_17ee;
        {
            uint8_t cl = (uint8_t)(4 - window_column);
            cl = (uint8_t)(cl << 1);
            cl = (uint8_t)(cl << 1);
            ax = (uint16_t)(ax + cl);       /* ch = 0 (mov cx,4) */
        }
        goto lab_17fc;
lab_17ee:
        {
            uint8_t cl = (uint8_t)(window_column + 1);
            cl = (uint8_t)(cl << 1);
            cl = (uint8_t)(cl << 1);
            ax = (uint16_t)(ax - cl);       /* ch = 0 (sub ch,ch) */
        }
lab_17fc: {
            uint16_t si = ds_pool[DS_WINDOW_ROW_OFFS + (bx & 0xff)]; /* mov bl,[bx+window_row_offset] */
            uint16_t idx_bx;
            uint8_t ch = pixel_to_bitmask((uint16_t)(ax + 0xa), &idx_bx);
            if (!(ds_read_throw((uint32_t)idx_bx + si) & ch)) return false; /* lab_1810 */
        }
    }
    /* lab_1812 */
    cat_y = dl;
    cat_y_bottom = (uint8_t)(dl + 0x32);
    cat_x = (int16_t)((uint16_t)cat_x & 0xfff8);
    at_platform = 1;
    return true;
}

/* --- T11: check_stairs_collision (nivel 7) ------------------------------- */

#define DS_WINDOW_ROW_Y_TABLE      0x2bd4u  /* 7 bytes const: b0 98 80 68 50 38 20 */
#define DS_WINDOW_ROW_COL_OFFSET   0x2bdbu  /* 7 bytes const: 00 12 24 36 48 5a 6c */

/* check_stairs_collision — literal port de level_objects.asm L302-342.
 * El bucle `mov cx,7 / mov bx,cx / dec bx / ... / loop` prueba las filas
 * bx = 6,5,...,0 (el loop sale cuando cx llega a 0, tras probar bx=0). */
bool check_stairs_collision(void) {
    uint8_t al = (uint8_t)((uint8_t)(cat_y - 5) & 0xf8);
    uint16_t cx = 7;
    uint16_t bx;
    do {                                      /* lab_3104 */
        bx = (uint16_t)(cx - 1);
        if (al == ds_pool[DS_WINDOW_ROW_Y_TABLE + bx]) goto lab_3111;
    } while (--cx != 0);
    return false;                             /* jmp lab_314d: clc */

lab_3111: {
        uint8_t ch = al;
        uint16_t ax = (uint16_t)((uint16_t)cat_x + 7);
        ax = (uint16_t)(ax >> 4);
        if (ax < 2) ax = 0; else ax = (uint16_t)(ax - 2);   /* sub ax,2 / jnc / sub ax,ax */
        if (ax >= 0x12) ax = 0x11;                          /* cmp ax,0x12 / jc (sin signo) */
        ax = (uint16_t)(ax + ds_pool[DS_WINDOW_ROW_COL_OFFSET + bx]);
        if (window_open_state[ax] != 0) return false;       /* lab_314d: clc */
        ch = (uint8_t)(ch + 5);
        cat_y = ch;
        cat_y_bottom = (uint8_t)(ch + 0x32);
        return true;
    }
}

/* check_level_collision — literal port of level_physics.asm's dispatcher.
 *
 * Nivel 0 (lab_161e) ya completo: rama cat_y&0xf8==0x60 (game_mode),
 * check_door_position y check_window_landing. Nivel 7 (check_stairs_collision)
 * T11 hecho (check_stairs_collision). */
bool check_level_collision(void) {
    if (level_number == 7) {
        return check_stairs_collision();
    }
    if (level_number != 0) {
        return check_level_platform();
    }
    /* lab_161e */
    uint8_t al = (uint8_t)(cat_y & 0xf8);
    if (al != 0x60) {
        if (check_door_position()) return true;   /* jc lab_1656: ret con carry */
        return check_window_landing();
    }
    /* lab_1630 */
    if (game_mode >= 2) return false;             /* jnc lab_1655: clc */
    cat_y = al;
    cat_y_bottom = (uint8_t)(al + 0x32);
    if (game_mode != 1) {
        game_mode = 1;
        mode_start_tick = read_bios_tick_lc();
    }
    return true;
}
