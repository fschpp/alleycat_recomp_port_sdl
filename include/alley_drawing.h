#ifndef ALLEY_DRAWING_H
#define ALLEY_DRAWING_H

#include <stdint.h>

/* alley_drawing.asm A (T14) — ver PROGRESS.md §6k. El resto de alley_drawing.asm
 * (draw_alley_scene, edificios, ventanas, detalles) llega en T15/T16. */

/* draw_difficulty_icon (L53-61): blit 1 word x 8 filas del ícono
 * diff_icon_table[diff_icon_idx & 3] a CGA offset 0x1902 (constante inmediata,
 * `mov di,diff_icon_cga_pos` sin corchetes). */
void draw_difficulty_icon(void);

/* init_alley_objects (L64-85): pone a 0 throw_col_data (alley_obj_state[1..15] es
 * el mismo bloque: alley_obj_state=0x1015, throw_col_data=0x1016), dibuja las 3
 * filas de objetos del callejón y deja window_column=0x10, current_floor=0,
 * throw_timer=1. Usa random() vía generate_throw_object. */
void init_alley_objects(void);

/* draw_object_row (L88-125): 20 objetos a paso de 4 bytes CGA desde di; cada uno
 * se genera con generate_throw_object(throw_chance[dif], bh), se dibuja (2 words x
 * 16 filas) y su throw_bits se OR-ea en throw_col_data[(i>>2) + row_offset]
 * desplazado ((~i)&3)*2. */
void draw_object_row(uint16_t di, uint8_t bh, uint16_t row_offset);

#endif
