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

/* T15 (alley_drawing.asm L160-182, L241-282) — ver PROGRESS.md §6l.
 * draw_window_strip: 4 ventanas (5 words x 16 filas) a paso de 0x14 bytes desde di.
 * draw_all_windows: 3 franjas en 0x3c5, 0x8c5, 0xdc5.
 * draw_building: techo en di, 3 (2 si di >= 0x1720, sin signo) cuerpos de 0x140 bytes, base.
 * draw_all_buildings: recorre building_pos_table desde building_offsets[difficulty_level]
 * hasta la entrada 0. */
void draw_window_strip(uint16_t di);
void draw_all_windows(void);
void draw_building(uint16_t di);
void draw_all_buildings(void);

/* T16 (alley_drawing.asm L6-50, L184-239) — ver PROGRESS.md §6m.
 * draw_alley_details: detalles aleatorios, acera 0x5655, parches y
 *   extras de suelo; orden fijo de random(): 40 detalles (con reintento si dl == draw_loop_count),
 *   36 parches, 5 extras.
 * clear_screen: fondo 0xAA + detalles + base + ícono + edificios + ventanas + init_alley_objects.
 * draw_alley_scene: igual pero sin ventanas ni objetos y con edificios de lista difficulty=1. */
void draw_alley_details(void);
void clear_screen(void);
void draw_alley_scene(void);

/* render_sprites (sound.asm L40-68, T17) — sprites decorativos del callejón: recorre la lista de
 * posiciones CGA de sprite_list_ptrs[difficulty&7] hasta 0xffff; por cada una un random() (&0xe)
 * elige una de 8 variantes de sprite_variant_table (8 por dificultad) y la blitea. */
void render_sprites(void);

#endif
