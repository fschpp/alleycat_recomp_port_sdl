#include "cat_state.h"
#include "sound.h"
#include "cga.h"
#include "level_collision.h"
#include "level3_enemy.h"
#include "alley.h"
#include "gen/ds_pool.h"
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

/* Sprite data lives in the wholesale-embedded data segment (ds_pool) —
 * see PROGRESS.md Sec 5w for why no separate extraction was needed.
 *   dat_38bc (0x38bc): two 84-byte (3 words x 14 rows) wing-flap frames,
 *   selected by l3_bird_frame_toggle (0 or 0x54).
 *   dat_37c0 (0x37c0): the 252-byte (6 words x 21 rows) "buzz away"
 *   escape sprite, drawn once near the cat when the bird is caught. */
#define L3_BIRD_SPRITE_TABLE 0x38bc
#define L3_BIRD_ESCAPE_SPRITE 0x37c0
/* dat_39cc (0x39cc): 8-entry word table, bird horizontal speed per
 * difficulty level (2,4,6,8,10,12,14,16 — confirmed by cross-referencing
 * l3_platform_id's known address immediately following it). */
#define L3_BIRD_SPEED_TABLE 0x39cc

/* --- runtime state (dat_3964/3966/3967/3968/396a/l3_door_toggle/396d/
 * 396e/396f/39c3/39c5/39c6/39c8/39ca) --- */
static uint16_t l3_bird_x;              /* dat_3964 */
static uint8_t  l3_bird_y;              /* dat_3966 */
static uint8_t  l3_bird_dive_state;     /* dat_3967: 0=resting/ascending, 1=diving, 0xff=at-limit */
static uint16_t l3_bird_draw_addr;      /* dat_3968 — last drawn CGA addr, for erase */
static uint8_t  l3_bird_undrawn;        /* dat_396a: nonzero = nothing drawn yet, skip erase */
static uint16_t l3_bird_frame_toggle;   /* l3_door_toggle: 0 or 0x54, selects wing-flap frame */
static uint8_t  l3_bird_frame_timer;    /* dat_396d: counts 2,1 -> toggle frame */
static uint8_t  l3_bird_wobble_tick;    /* dat_396e */
static uint16_t l3_bird_mask_save[42];  /* dat_396f — 3 words x 14 rows background save */
static uint16_t l3_bird_saved_x;        /* dat_39c3 */
static uint8_t  l3_bird_saved_y;        /* dat_39c5 */
static uint16_t l3_bird_last_tick;      /* dat_39c8 */
static uint16_t l3_bird_next_draw_addr; /* dat_39ca */

/* [0x552] = object_hit, [0x553] = cat_caught (cat_state.h) — ver nota en level3_enemy.h (T20). */

/* Non-blocking stand-in for the original's synchronous 9-tick busy-wait
 * (init_buzz_sound + a spin loop on int 0x1a calling update_buzz_sound
 * until 9 ticks elapse). Blocking the render loop for ~0.5s isn't
 * compatible with this port's externally-paced frame loop, so the wait
 * is spread across per-frame calls instead — same simplification style
 * as int 0x1a itself (see PROGRESS.md). sound.asm isn't ported yet, so
 * the buzz plays silently either way. */
static bool     l3_bird_escaping = false;
static uint16_t l3_bird_escape_start_tick;

/* check_l3_enemy_thrown / check_l3_enemy_cat — literal ports, both
 * reusing the already-verified check_rect_collision with the exact
 * register mapping from the disassembly (a=bird 0x18x0x0e hitbox,
 * b=thrown-object 0x10x0x1e or cat 0x18x0x0e hitbox). */
static bool check_l3_enemy_thrown(void) {
    return check_rect_collision((int16_t)l3_bird_x, l3_bird_y, 0x18, 0x0e,
                                 (uint16_t)thrown_obj_x, thrown_obj_y, 0x10, 0x1e);
}

static bool check_l3_enemy_cat(void) {
    return check_rect_collision((int16_t)l3_bird_x, l3_bird_y, 0x18, 0x0e,
                                 (uint16_t)cat_x, cat_y, 0x18, 0x0e);
}

/* erase_level3_enemy / draw_level3_enemy — literal ports, reusing the
 * already-verified blit_to_cga/blit_transparent. */
static void erase_l3_bird(void) {
    if (l3_bird_undrawn == 0) {
        blit_to_cga((const uint8_t *)l3_bird_mask_save, l3_bird_draw_addr, 3, 14);
    }
}

static void draw_l3_bird(void) {
    l3_bird_draw_addr = l3_bird_next_draw_addr;
    l3_bird_undrawn = 0;
    const uint8_t *frame = &ds_pool[L3_BIRD_SPRITE_TABLE + l3_bird_frame_toggle];
    blit_transparent(frame, l3_bird_draw_addr, 3, 14, l3_bird_mask_save);
}

/* Triggered when the bird's hitbox overlaps the cat — the "buzz away"
 * escape: a one-shot sprite drawn near the cat (never erased via a
 * saved background, matching the original's mask_save pointer being a
 * throwaway literal 0xe rather than a real buffer) plus a sound cue. */
static void l3_start_escape(void) {
    if (cat_caught != 0) return;

    int32_t cx = (int32_t)cat_x - 0xc;
    if (cx < 0) cx = 0;
    if (cx >= 0x10f) cx = 0x10e;
    uint8_t dl = (cat_y >= 4) ? (uint8_t)(cat_y - 4) : 0;

    uint16_t addr = (uint16_t)calc_cga_addr(dl, (uint16_t)cx, NULL);
    blit_transparent(&ds_pool[L3_BIRD_ESCAPE_SPRITE], addr, 6, 0x15, NULL);

    init_buzz_sound();
    l3_bird_escaping = true;
    l3_bird_escape_start_tick = read_bios_tick();
}

void init_level3_enemy(void) {
    l3_bird_y = 0x8;
    l3_bird_undrawn = 1;
    l3_bird_dive_state = 0;
    l3_bird_frame_timer = 2;
    l3_bird_x = 0x118;
    l3_bird_frame_toggle = 0;
}

void update_level3_enemy(void) {
    uint16_t tick_now = read_bios_tick();

    if (l3_bird_escaping) {
        update_buzz_sound();
        if ((uint16_t)(tick_now - l3_bird_escape_start_tick) >= 9) {
            l3_bird_escaping = false;
            object_hit = 1;
        }
        return;
    }

    if ((uint16_t)(tick_now - l3_bird_last_tick) < 2) return;
    l3_bird_last_tick = tick_now;

    if (check_l3_enemy_thrown()) return;

    if (check_l3_enemy_cat()) {
        l3_start_escape();
        return;
    }

    /* --- normal movement --- */
    uint16_t speed_idx = (uint16_t)((difficulty_level & 7) * 2);
    uint16_t speed = (uint16_t)(ds_pool[L3_BIRD_SPEED_TABLE + speed_idx] |
                                 (ds_pool[L3_BIRD_SPEED_TABLE + speed_idx + 1] << 8));
    l3_bird_saved_x = l3_bird_x;
    l3_bird_saved_y = l3_bird_y;

    bool did_horizontal_move = false;

    if (l3_bird_y == 0x8) {
        uint16_t bird_grid = l3_bird_x & 0xfff8u;
        uint16_t cat_grid  = (uint16_t)cat_x & 0xfff8u;
        if (bird_grid == cat_grid) {
            /* aligned with the cat's column — start diving */
            l3_bird_dive_state = 1;
            l3_bird_wobble_tick = 1;
        } else {
            if (bird_grid < cat_grid) {
                l3_bird_x = (uint16_t)(l3_bird_x + speed);
            } else {
                int32_t t = (int32_t)l3_bird_x - (int32_t)speed;
                l3_bird_x = (t < 0) ? 0 : (uint16_t)t;
            }
            did_horizontal_move = true;
        }
    }

    if (!did_horizontal_move) {
        uint8_t al = l3_bird_y;
        l3_bird_wobble_tick++;
        uint8_t dl = (uint8_t)(((l3_bird_wobble_tick >> 2) & 0x3) + 2);

        if (l3_bird_dive_state == 1) {
            al = (uint8_t)(al + dl);
            if (al > cat_y) {
                l3_bird_dive_state = 0xff;
            } else {
                uint16_t dist = (uint16_t)(l3_bird_x - (uint16_t)cat_x);
                if ((uint16_t)l3_bird_x < (uint16_t)cat_x) dist = (uint16_t)(~dist);
                if (dist > 0x30) {
                    l3_bird_dive_state = 0xff;
                } else if (al >= 0xa0) {
                    al = 0x9f;
                    l3_bird_dive_state = 0xff;
                }
            }
        } else {
            if (al < dl) {
                al = 8;
                l3_bird_dive_state = 0;
            } else {
                uint8_t res = (uint8_t)(al - dl);
                if (res >= 9) al = res;
                else { al = 8; l3_bird_dive_state = 0; }
            }
        }
        l3_bird_y = al;
    }

    if (check_l3_enemy_thrown()) {
        l3_bird_x = l3_bird_saved_x;
        l3_bird_y = l3_bird_saved_y;
        return;
    }

    if (check_l3_enemy_cat()) {
        l3_start_escape();
        return;
    }

    l3_bird_next_draw_addr = (uint16_t)calc_cga_addr(l3_bird_y, l3_bird_x, NULL);
    erase_l3_bird();

    l3_bird_frame_timer--;
    if (l3_bird_frame_timer == 0) {
        l3_bird_frame_timer = 2;
        l3_bird_frame_toggle ^= 0x54;
    }

    draw_l3_bird();
}

/* ---------------------------------------------------------------------------------------------
 * T20 — puertas del nivel 3 (level_objects.asm L1376-1440). PROGRESS.md §6q.
 * Los labels `l3_door_*` de tareas.md NO corresponden a estas variables (0x396b es el toggle de
 * alas del pájaro); el ASM usa dat_37a3.. :
 *   dat_37af (byte)      puertas abiertas que quedan (init 3)
 *   dat_37b0/2/4 (words) puerta i activa (init 1); índice = bx>>1, bx = 0,2,4
 *   dat_37b6 (word)      índice de bucle bx;  dat_37b8 (word) último tick procesado
 *   dat_37a3 (3 words, ds_pool)  X de cada puerta = 0x00c0, 0x00e0, 0x0100
 *   dat_37a9 (3 words, ds_pool)  offset CGA de cada puerta = 0x03f0, 0x03f8, 0x0400
 * ------------------------------------------------------------------------------------------- */
#define L3_DOOR_X_TABLE   0x37a3
#define L3_DOOR_CGA_TABLE 0x37a9

static uint8_t  l3_doors_left;        /* dat_37af */
static uint16_t l3_door_active[3];    /* dat_37b0, dat_37b2, dat_37b4 */
static uint16_t l3_door_loop_bx;      /* dat_37b6 */
static uint16_t l3_door_last_tick;    /* dat_37b8 */

static uint16_t l3_ds_word(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

void init_level3_doors(void) {
    l3_doors_left = 0x3;
    l3_door_active[0] = 1;
    l3_door_active[1] = 1;
    l3_door_active[2] = 1;
}

/* close_level3_door: desactiva la puerta, rellena su hueco (2 words x 16 filas) con 0xAAAA tomado
 * del scratch DS 0x000e (que primero se llena con 0xAAAA: rep stosw cx=0x20 = 64 bytes), y si era
 * la última puerta y !object_hit marca cat_caught (que es el flag de fin de nivel de entry.asm). */
void close_level3_door(uint16_t bx) {
    uint8_t scratch[64];
    l3_door_active[bx >> 1] = 0;
    for (int i = 0; i < 64; i++) scratch[i] = 0xaa;
    blit_to_cga(scratch, l3_ds_word((uint16_t)(L3_DOOR_CGA_TABLE + bx)), 2, 0x10);
    if (--l3_doors_left == 0 && object_hit == 0) {   /* dec byte / jnz / cmp [0x552],0 */
        cat_caught = 0x1;
    }
}

void update_level3_doors(void) {
    uint16_t tick = read_bios_tick();
    if (tick == l3_door_last_tick) return;           /* cmp dx,[dat_37b8] / jnz */
    l3_door_last_tick = tick;

    /* bx = 4, 2, 0 (sub 2 / jnb: sale al pasar de 0) */
    for (int bx = 4; bx >= 0; bx -= 2) {
        l3_door_loop_bx = (uint16_t)bx;
        if (l3_door_active[bx >> 1] == 0) continue;
        /* A: ax=x puerta, dl=0x18 (y), si=0x10 (ancho), cl=0x10 (alto);
         * B: bx=cat_x, dh=cat_y, di=0x18, ch=0x0e (cx=0x0e10) */
        if (!check_rect_collision((int16_t)l3_ds_word((uint16_t)(L3_DOOR_X_TABLE + bx)), 0x18, 0x10, 0x10,
                                  (uint16_t)cat_x, cat_y, 0x18, 0x0e)) continue;
        start_tone(0xc00, 0x8fd);
        restore_alley_buffer();
        erase_l3_bird();
        close_level3_door(l3_door_loop_bx);
        save_alley_buffer();
        draw_l3_bird();
        return;
    }
}
