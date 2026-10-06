#include "cat_state.h"
#include "cga.h"
#include "score.h"
#include "gen/digit_sprites.h"
#include "gen/ds_pool.h"
#include "game_flow.h"
#include "level7_epilogue.h"
#include "palette.h"
#include "sound.h"
#include <string.h>
#include <time.h>

/* CGA screen positions — these are immediate constants in the original
 * (mov di,label with no brackets loads the label's OWN address as a
 * literal value, same pattern already found for dat_1b02 in §5u), not
 * data reads. Verified in-range for CGA_MEM_SIZE (0x4000). */
#define LIVES_CGA_POS        0x1260
#define HIGH_SCORE_CGA_POS   0x12ca
#define CURRENT_SCORE_CGA_POS 0x143c

/* zero_score_buffer — literal port. */
static void zero_score_buffer(unsigned char *buf) {
    memset(buf, 0, 7);
}

void clear_score(void) {
    zero_score_buffer(current_score);
}

void clear_high_score(void) {
    zero_score_buffer(high_score);
}

/* render_score_digits — literal port. Draws 7 BCD digits left-to-right
 * from `buf`, each via the shared digit_sprite_data (1 word x 8 rows =
 * 16B per digit, indexed by digit*16), with an extra 2px gap inserted
 * after the 3rd digit (a thousands-grouping visual gap). */
static void render_score_digits(const unsigned char *buf, uint16_t draw_pos) {
    score_draw_pos = draw_pos;
    score_buf_ptr = buf;
    score_digit_idx = 0;

    for (;;) {
        unsigned char digit = *score_buf_ptr;
        const uint8_t *sprite = &digit_sprite_data[(size_t)digit * 16];
        blit_to_cga(sprite, score_draw_pos, 1, 8);

        score_draw_pos = (uint16_t)(score_draw_pos + 2);
        score_buf_ptr++;
        score_digit_idx++;

        if (score_digit_idx == 7) break;
        if (score_digit_idx == 3) score_draw_pos = (uint16_t)(score_draw_pos + 2);
    }
}

/* draw_current_score / draw_high_score_display — see score.h's note:
 * named by content (verified against update_high_score's clear
 * current/high buffer comment), not by the original's swapped labels. */
void draw_current_score(void) {
    render_score_digits(current_score, CURRENT_SCORE_CGA_POS);
}

void draw_high_score_display(void) {
    render_score_digits(high_score, HIGH_SCORE_CGA_POS);
}

/* draw_lives — literal port. Only redraws when lives_count changed since
 * the last call, using the same shared digit sprite sheet. */
void draw_lives(void) {
    if (lives_count == lives_display) return;
    lives_display = lives_count;
    const uint8_t *sprite = &digit_sprite_data[(size_t)lives_count * 16];
    blit_to_cga(sprite, LIVES_CGA_POS, 1, 8);
}

/* add_score — literal port of the BCD-add-with-carry-propagation loop.
 * The original indexes via a mislabeled "lives_display"-based pointer
 * that actually lands on current_score[1..6] (verified in §5v — the
 * label is an artifact, not really lives_display); this port just
 * operates on current_score directly, which is what it always meant. */
void add_score(unsigned char bcd_digit) {
    unsigned char carry = bcd_digit;
    for (int i = 5; i >= 0; i--) {
        unsigned char sum = (unsigned char)(current_score[i + 1] + carry);
        /* These buffer bytes are unpacked decimal digits (0-9 each), not
         * packed BCD nibbles — the original's AAA instruction, applied
         * to values in this restricted 0-9(+carry) domain, is exactly
         * equivalent to plain "if sum>=10, subtract 10 and carry 1."
         * (An earlier draft of this function tried to approximate AAA's
         * nibble/aux-carry check directly and got it wrong for sums like
         * 9+9+1=19, whose low nibble (3) isn't >9 despite needing a
         * carry — decimal sum>=10 is the correct, simpler equivalent.) */
        if (sum >= 10) {
            current_score[i + 1] = (unsigned char)(sum - 10);
            carry = 1;
        } else {
            current_score[i + 1] = sum;
            carry = 0;
        }
        if (carry == 0) break;
    }
    draw_current_score();
}

/* add_bcd_scores — literal port, right-to-left BCD add with carry
 * (matches current_score/high_score's 7-byte MSB-first layout). */
void add_bcd_scores(unsigned char *dst, const unsigned char *src) {
    unsigned char carry = 0;
    for (int i = 6; i >= 0; i--) {
        unsigned char sum = (unsigned char)(dst[i] + src[i] + carry);
        /* same decimal-digit equivalence as add_score, see its comment */
        if (sum >= 10) {
            dst[i] = (unsigned char)(sum - 10);
            carry = 1;
        } else {
            dst[i] = sum;
            carry = 0;
        }
    }
}

/* update_high_score — literal port: lexicographically compares the two
 * 7-digit BCD buffers (matching the original's byte-by-byte `loopz`
 * comparison scan), copying current -> high if current is larger. */
void update_high_score(void) {
    int i;
    for (i = 0; i < 7; i++) {
        if (current_score[i] != high_score[i]) break;
    }
    if (i < 7 && current_score[i] > high_score[i]) {
        memcpy(high_score, current_score, 7);
    }
}

/* ===================================================================================================
 * handle_level_complete (level_objects.asm L1100-1227, T47; PROGRESS.md §6ar)
 * Pantalla de bonus de fin de nivel: calcula el bonus segun el tiempo jugado, lo suma al puntaje, anima la barra,
 * parpadea el borde con la melodia y limpia. La llaman level_transition (enemy.asm L142, via transition.c) y el
 * epilogo del nivel 7 (level_objects.asm L3618, via level7_epilogue.c).
 * =================================================================================================== */

#define DS_BONUS_PTR_TABLE   0x36cc   /* dat_36cc: 8 punteros DS (por level_state*2) a constantes BCD de 7 bytes */
#define DS_L7_REPEAT_TABLE   0x36dc   /* dat_36dc: 8 words (1,3,5,...,15): veces que se suma el bonus del nivel 7 */

uint16_t bonus_binary;      /* DS 0x3697 */
uint8_t  bonus_bcd[8];      /* DS 0x368d */
uint16_t bonus_tick_start;  /* DS 0x3695 */
uint8_t  bonus_color;       /* DS 0x3699 */
uint8_t  bonus_row;         /* DS 0x369e */
uint8_t  bonus_bar_flag;    /* DS 0x369f */
uint16_t bonus_l7_index;    /* DS 0x370c */
uint16_t bonus_duration;    /* DS 0x3722 */
uint8_t  score_save_a[64];  /* DS 0x000e */
uint8_t  score_save_b[320]; /* DS 0x004e */

/* Mismo reloj que result.c / game_flow.c: int 0x1a -> dx a 18.2 Hz, con hook de tests. */
static uint16_t bios_tick_offset;   /* T55: ajuste que aplica set_bios_tick (int 0x1a ah=1); 0 por defecto */
static uint16_t score_tick_raw(void) {
    if (game_tick_fn) return game_tick_fn();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms * 182 / 10000);
}
uint16_t score_tick(void) { return (uint16_t)(score_tick_raw() + bios_tick_offset); }

/* int 0x1a ah=1 (fijar el contador de ticks, solo la palabra baja). Modelado como un desplazamiento sobre el reloj de
 * score_tick: el resto de modulos del port conservan su propio reloj (ver PROGRESS.md §6ay). */
void set_bios_tick(uint16_t t) { bios_tick_offset = (uint16_t)(t - score_tick_raw()); }

static uint16_t score_ds_word(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

/* save_score_regions (L1228-1243): guarda en DS:0xe y DS:0x4e las dos regiones de CGA que la pantalla de bonus del
 * nivel 7 tapa con texto. (Es de T48 en tareas.md, pero el final del nivel 7 de esta funcion las restaura.) */
void save_score_regions(void) {
    save_from_cga(score_save_a, 0x8e4, 0x4, 0x8);      /* cx=0x804 */
    save_from_cga(score_save_b, 0xc94, 0x14, 0x8);     /* cx=0x814 */
}

/* Traduccion literal, etiqueta por etiqueta. Notas de lectura del ASM:
 *  - `sub ax,dx` + `jnb`: CF = ax < dx (sin signo); con CF el bonus se satura a 0.
 *  - `db 0xd1,0xe8` = shr ax,1; `db 0xd1,0xe0` = shl ax,1 (el desensamblador los muestra con ",0x0").
 *  - `db 0xd0,0xe3` = shl bl,1 (solo el byte bajo; level_state/difficulty_level <= 7, no hay acarreo a bh).
 *  - [0x412] y [0x410] son DOS variables distintas: el nivel 7 mide desde [0x412] (start_tick == l7_completion_tick,
 *    el mismo word que escribe run_victory_sequence); los demas desde [0x410] (game_tick, lo escribe el manejador de
 *    muerte). */
void handle_level_complete(void) {
    uint16_t ax, bx, cx, dx;
    uint8_t al;

    if (level_state != 0x7) goto lab_38ba;
    goto lab_38d3;
lab_38ba:
    l7_completion_counter++;                               /* inc word [0x414] */
    force_level7 = 0x1;                                    /* mov byte [0x418],1 */
    mask_score_tiles(0xaaaa);
    ax = 0;                                                /* sub ax,ax */
    bonus_bar_flag = 0x0;                                  /* mov byte [dat_369f],0 */
    animate_score_bar(ax);
lab_38d3:
    dx = score_tick();                                     /* sub ah,ah / int 0x1a */
    if (level_state != 0x7) goto lab_38ef;
    dx = (uint16_t)(dx - l7_completion_tick);              /* sub dx,[0x412] */
    ax = 0x2a30;
    ax = (ax < dx) ? 0 : (uint16_t)(ax - dx);              /* sub ax,dx / jnb / sub ax,ax */
    ax >>= 1;                                              /* lab_38eb: shr ax,1 */
    goto lab_390e;
lab_38ef:
    dx = (uint16_t)(dx - game_tick);                       /* sub dx,[0x410] */
    ax = 0x546;
    if (level_state != 0x6) goto lab_38ff;
    ax = (uint16_t)(ax << 1);                              /* shl ax,1 */
lab_38ff:
    ax = (ax < dx) ? 0 : (uint16_t)(ax - dx);              /* sub ax,dx / jnb / sub ax,ax */
    if (level_state == 0x6) goto lab_390e;                 /* lab_3905 */
    ax = (uint16_t)(ax << 1);                              /* shl ax,1 */
lab_390e:
    bonus_binary = ax;
    binary_to_bcd(ax);                                     /* recibe el valor en ax (no lee dat_3697) */
    bx = level_state;
    bx = (uint16_t)((bx & 0xff00) | (uint8_t)((bx & 0xff) << 1));   /* shl bl,1 */
    add_bcd_scores(bonus_bcd, &ds_pool[score_ds_word((uint16_t)(DS_BONUS_PTR_TABLE + bx))]);   /* si=[bx+dat_36cc] di=dat_368d */
    if (level_state != 0x7) goto lab_396e;
    bx = difficulty_level;                                 /* mov bx,[0x8] */
    bx = (uint16_t)((bx & 0xff00) | (uint8_t)((bx & 0xff) << 1));   /* shl bl,1 */
    ax = bx;
    cx = score_ds_word((uint16_t)(DS_L7_REPEAT_TABLE + bx));
    if (l7_obj_spawn_slot >= 0x8) goto lab_3943;           /* jnb (sin signo); 0xffff tras draw_love_scene_bg */
    cx = (uint16_t)(cx << 1);
    ax = (uint16_t)(ax + 0x10);
lab_3943:
    bonus_l7_index = ax;
lab_3946:
    add_bcd_scores(current_score, bonus_bcd);              /* di=dat_1f82 (current_score), si=dat_368d */
    cx = (uint16_t)(cx - 1);                               /* loop: cx-- ; jnz */
    if (cx != 0) goto lab_3946;
    save_score_regions();
    bonus_row = 0x38;
    bonus_color = 0x1;
    bonus_duration = 0x44;
    print_bonus_score();
    print_level7_bonus();
    goto lab_39a7;
lab_396e:
    add_bcd_scores(current_score, bonus_bcd);
    bonus_color = 0x2;
    bonus_duration = 0x1e;
    mask_score_tiles(0xffff);
    ax = (uint16_t)(0xa8c - bonus_binary);
    ax >>= 4;                                              /* mov cl,4 / shr ax,cl */
    al = (uint8_t)(ax & 0xf0);                             /* and al,0xf0 (ah no se usa: se pisa abajo) */
    bonus_row = al;
    ax = (uint16_t)(al * 0x28);                            /* mov ah,0x28 / mul ah */
    bonus_bar_flag = 0x1;
    animate_score_bar(ax);
    print_bonus_score();
lab_39a7:
    bonus_tick_start = score_tick();                       /* sub ah,ah / int 0x1a / mov [dat_3695],dx */
lab_39af:
    if (level_state != 0x7) goto lab_39bb;
    play_victory_note();
    goto lab_39be;
lab_39bb:
    play_level_note();
lab_39be:
    dx = flash_score_color();                              /* devuelve dx = tick (push dx ... pop dx) */
    dx = (uint16_t)(dx - bonus_tick_start);
    if (dx < bonus_duration) goto lab_39af;                /* jb (sin signo) */
    bios_color_select(0x0, 0x0);                           /* sub bx,bx / mov ah,0xb / int 0x10 */
    if (level_state == 0x7) goto lab_39dc;
    silence_speaker();
    return;
lab_39dc:
    blit_to_cga(score_save_a, 0x8e4, 0x4, 0x8);            /* si=0xe  di=0x8e4 cx=0x804 */
    blit_to_cga(score_save_b, 0xc94, 0x14, 0x8);           /* si=0x4e di=0xc94 cx=0x814 */
}
