/* alley_drawing.c — alley_drawing.asm A (T14): ícono de dificultad y filas de
 * objetos del callejón (PROGRESS.md §6k); B (T15): ventanas y edificios (§6l); C (T16): detalles y escena (§6m). */

#include <string.h>
#include "alley_drawing.h"
#include "cat_state.h"
#include "cga.h"
#include "throw.h"
#include "level_background.h"
#include "gen/ds_pool.h"

/* DS verificados con tools/asm_label.sh y /tmp/data_segment.bin:
 *   diff_icon_table 0x2ad1: 4 words -> 0x2890, 0x27e0, 0x2820, 0x2810 (indexado con
 *     `and bx,3; shl bl,1`, exactamente 4 entradas; la 5.ª palabra, 0, ya es otro label).
 *     Cada ícono mide 16 bytes (cx=0x801: 1 word x 8 filas); los 4 caen dentro de ds_pool.
 *   throw_chance 0x2aba: 8 bytes (T13).
 *   throw_col_data 0x1016, 15 bytes; alley_obj_state 0x1015 es el mismo bloque desplazado
 *     un byte: el bucle `bx=0xf..1` de init_alley_objects pone a 0 0x1016..0x1024.
 * draw_row_param/draw_loop_count/draw_row_offset (0x2ac9/0x2ac4/0x2aca) son scratch que
 * solo usa draw_object_row: se pasan como argumentos/locales. */
#define DS_DIFF_ICON_TABLE 0x2ad1u
#define DS_THROW_CHANCE    0x2abau
#define DIFF_ICON_CGA_POS  0x1902u   /* constante inmediata, NO un dato */

/* T15 (verificados con asm_label.sh / data_segment.bin):
 *   window_sprite_data 0x2680: cx=0x1005 -> 5 words x 16 filas = 160 bytes.
 *   building_top_sprite 0x2976 (cx=0xc05: 5w x 12 = 120 B), building_mid_sprite 0x29ee
 *   (cx=0x804: 4w x 8 = 64 B), building_bottom_sprite 0x2a2e (cx=0xb04: 4w x 11 = 88 B):
 *   las diferencias de punteros coinciden con ancho*2*alto.
 *   building_pos_table 0x2a86: words, 0 termina cada lista; building_offsets 0x2ab2:
 *   bytes {0,14,26,36,26,36,26,36}, indexado por difficulty_level (word, valores 0..7). */
#define DS_WINDOW_SPRITE   0x2680u
#define DS_BUILDING_TOP    0x2976u
#define DS_BUILDING_MID    0x29eeu
#define DS_BUILDING_BOTTOM 0x2a2eu
#define DS_BUILDING_POS    0x2a86u
#define DS_BUILDING_OFFS   0x2ab2u

/* draw_loop_count (DS 0x2ac4, inicial 0): scratch de alley_drawing.asm que SE ARRASTRA entre
 * rutinas: draw_alley_details lo lee (`cmp dl,[draw_loop_count]`) antes de escribirlo, y lo
 * encuentra con lo que dejó la última rutina (draw_object_row 0x14, strips/edificios/detalles 0).
 * Solo lo usa este archivo (grep en todo el ASM), por eso es static. T16. */
static uint8_t draw_loop_count = 0;

static uint16_t ds_word_ad(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

void draw_difficulty_icon(void) {
    uint16_t bx = (uint16_t)(diff_icon_idx & 0x3);      /* and bx,3 */
    bx = (uint16_t)((bx << 1) & 0xff);                  /* shl bl,1 */
    uint16_t si = ds_word_ad((uint16_t)(DS_DIFF_ICON_TABLE + bx));
    blit_to_cga(&ds_pool[si], DIFF_ICON_CGA_POS, 1, 8); /* cx=0x801 */
}

void draw_object_row(uint16_t di, uint8_t bh, uint16_t row_offset) {
    draw_loop_count = 0;
    do {                                                /* lab_2acf */
        uint8_t bl = ds_pool[DS_THROW_CHANCE + difficulty_level];
        generate_throw_object(bl, bh);                  /* di = throw_obj_buf */
        blit_to_cga(throw_obj_buf, di, 2, 16);          /* cx=0x1002 */
        uint16_t bx = (uint16_t)(draw_loop_count >> 2); /* bl >> 2 */
        uint8_t cl = (uint8_t)((((uint8_t)~draw_loop_count) & 0x3) << 1);
        uint8_t al = (uint8_t)(throw_bits << cl);       /* shl al,cl (8 bits) */
        throw_col_data[bx + row_offset] |= al;
        di = (uint16_t)(di + 4);
        draw_loop_count++;
    } while (draw_loop_count < 0x14);                       /* cmp 0x14 / jc */
}

void init_alley_objects(void) {
    /* lab_2a83: bx=0xf..1, alley_obj_state[bx]=0  ==  throw_col_data[0..14] */
    for (int bx = 0xf; bx != 0; bx--) throw_col_data[bx - 1] = 0;
    draw_object_row(0x140, 0x80, 0x0);
    draw_object_row(0x640, 0x30, 0x5);
    draw_object_row(0xb40, 0x00, 0xa);
    window_column = 0x10;
    current_floor = 0x0;
    throw_timer = 0x1;
}

/* ---- T15: alley_drawing.asm L160-182 y L241-282 ---- */

void draw_window_strip(uint16_t di) {
    draw_loop_count = 0x4;
    do {                                                /* lab_2b76 */
        blit_to_cga(&ds_pool[DS_WINDOW_SPRITE], di, 5, 16);   /* cx=0x1005; push/pop di */
        di = (uint16_t)(di + 0x14);
    } while (--draw_loop_count != 0);
}

void draw_all_windows(void) {
    draw_window_strip(0x3c5);
    draw_window_strip(0x8c5);
    draw_window_strip(0xdc5);
}

void draw_building(uint16_t di) {
    uint16_t pos_tmp = di;                              /* draw_pos_tmp */
    draw_loop_count = 0x3;
    if (di >= 0x1720) draw_loop_count--;                    /* cmp di,0x1720 / jc: sin signo */
    pos_tmp = (uint16_t)(pos_tmp + 0x1e0);
    blit_to_cga(&ds_pool[DS_BUILDING_TOP], di, 5, 12);  /* cx=0xc05, di original */
    do {                                                /* lab_2c5d */
        blit_to_cga(&ds_pool[DS_BUILDING_MID], pos_tmp, 4, 8);   /* cx=0x804 */
        pos_tmp = (uint16_t)(pos_tmp + 0x140);
    } while (--draw_loop_count != 0);
    blit_to_cga(&ds_pool[DS_BUILDING_BOTTOM], pos_tmp, 4, 11);   /* cx=0xb04 */
}

void draw_all_buildings(void) {
    uint16_t bx = ds_pool[DS_BUILDING_OFFS + difficulty_level];  /* bx=word[dif]; bl=byte[bx+offs] */
    for (;;) {                                          /* lab_2c8c */
        uint16_t di = ds_word_ad((uint16_t)(DS_BUILDING_POS + bx));
        if (di == 0) return;                            /* cmp di,0 / jnz */
        draw_building(di);
        bx = (uint16_t)(bx + 2);
    }
}

/* ---- T16: alley_drawing.asm L6-50 y L184-239 ---- */

#define DS_ALLEY_BASE_BLOCK_LIST 0x28a0u
#define DS_GROUND_EXTRA_SPRITE   0x296cu
#define DS_DETAIL_SPRITES        0x2904u   /* 4 sprites de 16 B: +0,+0x10,+0x20,+0x30 */
#define SIDEWALK_FILL_POS        0x1180u   /* constantes inmediatas (sin corchetes) */
#define DAT_3180                 0x3180u

void draw_alley_details(void) {
    uint16_t pos_tmp = 0x103e;                          /* draw_pos_tmp */
    for (;;) {                                          /* lab_2ba4 */
        pos_tmp = (uint16_t)(pos_tmp + 2);
        if (pos_tmp >= 0x1090) break;                   /* cmp/jnc, sin signo */
        uint16_t dx;
        do {                                            /* lab_2bb3 */
            dx = (uint16_t)(cga_random() & 0x30);
        } while ((uint8_t)dx == draw_loop_count);       /* cmp dl,[..] / jz */
        draw_loop_count = (uint8_t)dx;
        blit_to_cga(&ds_pool[DS_DETAIL_SPRITES + dx], pos_tmp, 1, 8);   /* cx=0x801 */
    }
    /* lab_2bd2: acera, rep stosw de 0x5655 (bytes 55 56 55 56...) en dos bancos */
    for (uint16_t i = 0; i < 0x500; i++) {
        cga_mem[SIDEWALK_FILL_POS + 2u * i]     = 0x55;
        cga_mem[SIDEWALK_FILL_POS + 2u * i + 1] = 0x56;
        cga_mem[DAT_3180 + 2u * i]              = 0x55;
        cga_mem[DAT_3180 + 2u * i + 1]          = 0x56;
    }
    pos_tmp = 0x2944;
    do {                                                /* lab_2bec */
        draw_loop_count = 0x9;
        do {                                            /* lab_2bf1 */
            uint16_t di = (uint16_t)((cga_random() & 0x776) + 0x12c0);
            blit_to_cga(&ds_pool[pos_tmp], di, 1, 5);   /* cx=0x501, si=draw_pos_tmp */
        } while (--draw_loop_count != 0);
        pos_tmp = (uint16_t)(pos_tmp + 0xa);
    } while (pos_tmp < 0x296c);                         /* cmp/jc */
    draw_loop_count = 0x5;
    do {                                                /* lab_2c20 */
        uint16_t di = (uint16_t)((cga_random() & 0x3e) + 0x3a98);
        blit_to_cga(&ds_pool[DS_GROUND_EXTRA_SPRITE], di, 1, 5);
    } while (--draw_loop_count != 0);
}

/* Relleno 0xAA de ambos bancos: rep stosw cx=0xfa0 desde 0 y desde cga_bank1_base. */
static void alley_fill_aa(void) {
    memset(&cga_mem[0], 0xaa, 0xfa0u * 2);
    memset(&cga_mem[CGA_BANK_SIZE], 0xaa, 0xfa0u * 2);
}

void clear_screen(void) {
    alley_fill_aa();
    draw_alley_details();
    draw_block_list(0, DS_ALLEY_BASE_BLOCK_LIST);       /* sub ax,ax; bx=lista */
    draw_difficulty_icon();
    draw_all_buildings();
    draw_all_windows();
    init_alley_objects();
}

void draw_alley_scene(void) {
    alley_fill_aa();
    draw_alley_details();
    draw_block_list(0, DS_ALLEY_BASE_BLOCK_LIST);
    draw_difficulty_icon();
    uint16_t saved = difficulty_level;                  /* push ax */
    difficulty_level = 0x1;
    draw_all_buildings();
    difficulty_level = saved;                           /* pop ax */
}
