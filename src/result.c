/* result.c — enemy.asm show_level_result / draw_result_frame (L275-353, T46; PROGRESS.md §6aq).
 * En un .c aparte de transition.c para que level_transition la llame a traves de una frontera de unidad
 * (los tests la envuelven con --wrap) y para poder envolver sus dependencias de sonido. */
#include <stdint.h>
#include <time.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "sound.h"
#include "gen/ds_pool.h"

#define DS_RESULT_MASK_TABLE   0x1c1e   /* 4 words (hasta result_sprite_table 0x1c26) */
#define DS_RESULT_SPRITE_TABLE 0x1c26   /* 4 words -> 0x191b, 0x19db, 0x1a9b, 0x1b5b */
#define DS_RESULT_SPRITE_DEF   0x185b   /* sprite por defecto (mov ax,0x185b) */

uint16_t result_dissolve_mask;   /* DS 0x1c1b */
uint8_t  result_frame_counter;   /* DS 0x1c1d */
uint16_t result_sprite_ptr;      /* DS 0x1c2e: offset DS del sprite (96 words = 8 palabras x 12 filas) */
uint16_t result_last_tick;       /* DS 0x1830 */

static uint16_t result_ds_word(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

/* sub ah,ah / int 0x1a -> dx: mismo hook y base de tiempo que game_flow.c (18.2 Hz) */
static uint16_t result_tick(void) {
    if (game_tick_fn) return game_tick_fn();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms * 182 / 10000);
}

/* Copia el sprite al scratch DS:0x000e (96 words) aplicando result_dissolve_mask (and ax,mask) y lo vuelca
 * a CGA 0xed0 (8 palabras x 12 filas). El scratch es local: nadie mas lee DS:0xe despues. */
void draw_result_frame(void) {
    uint8_t scratch[0x60 * 2];
    for (int i = 0; i < 0x60; i++) {
        uint16_t w = (uint16_t)(result_ds_word((uint16_t)(result_sprite_ptr + 2 * i)) & result_dissolve_mask);
        scratch[2 * i]     = (uint8_t)(w & 0xff);
        scratch[2 * i + 1] = (uint8_t)(w >> 8);
    }
    blit_to_cga(scratch, 0xed0, 0x8, 0xc);
}

void show_level_result(void) {
    uint16_t ax, bx;
    uint8_t al;

    if (level_state != 0x7) goto lab_1d81;               /* cmp word [0x6],7 */
    love_scene_outro();                                  /* T58: ui.c */
    return;
lab_1d81:
    init_result_melody();
    ax = DS_RESULT_SPRITE_DEF;
    if (object_hit == 0x0) goto lab_1dc6;                /* cmp byte [0x552],0 */
    bx = game_timer;
    game_timer = (uint16_t)(game_timer + 0x2);
    bx &= 0x6;
    ax = result_ds_word((uint16_t)(DS_RESULT_SPRITE_TABLE + bx));
    if (lives_count == 0x0) goto lab_1daa;
    lives_count--;
lab_1daa:
    if (object_hit != 0xdd) goto lab_1dc6;
    if (difficulty_level == 0x0) goto lab_1dc6;          /* cmp word [0x8],0 */
    if (lives_count < 0x1) goto lab_1dc6;                /* jb (sin signo) */
    show_extra_life();
    silence_speaker();
    return;
lab_1dc6:
    result_sprite_ptr = ax;
    result_dissolve_mask = 0x8080;
    result_frame_counter = 0x1c;
lab_1dd4:
    draw_result_frame();
    result_last_tick = result_tick();
lab_1ddf:
    play_result_note();
    if (result_tick() == result_last_tick) goto lab_1ddf;
    if (result_frame_counter > 0x14) goto lab_1e02;      /* ja (sin signo) */
    bx = (uint16_t)(result_frame_counter & 0x6);
    ax = result_ds_word((uint16_t)(DS_RESULT_MASK_TABLE + bx));
    goto lab_1e0a;
lab_1e02:
    /* mov ax,mask ; stc ; rcr al,1 (el `rcr al,0x0` del ASM) ; mov ah,al: desplaza un 1 por la izquierda */
    al = (uint8_t)(((result_dissolve_mask & 0xff) >> 1) | 0x80);
    ax = (uint16_t)((al << 8) | al);
lab_1e0a:
    result_dissolve_mask = ax;
    result_frame_counter--;
    if (result_frame_counter != 0) goto lab_1dd4;
    silence_speaker();
}
