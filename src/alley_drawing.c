/* alley_drawing.c — alley_drawing.asm A (T14): ícono de dificultad y filas de
 * objetos del callejón. PROGRESS.md §6k. */

#include "alley_drawing.h"
#include "cat_state.h"
#include "cga.h"
#include "throw.h"
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
    uint8_t loop_count = 0;                             /* draw_loop_count */
    do {                                                /* lab_2acf */
        uint8_t bl = ds_pool[DS_THROW_CHANCE + difficulty_level];
        generate_throw_object(bl, bh);                  /* di = throw_obj_buf */
        blit_to_cga(throw_obj_buf, di, 2, 16);          /* cx=0x1002 */
        uint16_t bx = (uint16_t)(loop_count >> 2);      /* bl >> 2 */
        uint8_t cl = (uint8_t)((((uint8_t)~loop_count) & 0x3) << 1);
        uint8_t al = (uint8_t)(throw_bits << cl);       /* shl al,cl (8 bits) */
        throw_col_data[bx + row_offset] |= al;
        di = (uint16_t)(di + 4);
        loop_count++;
    } while (loop_count < 0x14);                        /* cmp 0x14 / jc */
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
