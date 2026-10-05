#ifndef LEVEL4_H
#define LEVEL4_H
#include <stdint.h>
#include <stdbool.h>

/* Nivel 4, helpers A (level_objects.asm L1897-1932 y L2094-2149) — T21, PROGRESS.md §6r.
 * Los helpers que en el ASM reciben `bx` (y `si = 2*bx`) reciben aquí el índice de objeto 0..3. */

/* check_l4_thrown_collision: objeto lanzado (0x10 x 0x1e) vs. gato (0x18 x 0x0e). Devuelve CF. */
bool check_l4_thrown_collision(void);
/* init_level4_objects: para i = 3..0 (cx=4..1): active=1, hit=0, randomize_l4_pos(i) y
 * anim = (random() & 0xf) + 0x14; después l5_obj_index=0, l5_obj_count=4. */
void init_level4_objects(void);
/* erase_level4_sprite: restaura el fondo (2 words x 12 filas) del objeto l5_obj_index, pero SOLO si
 * l5_obj_active[idx] == 0 (jnz sale con activo != 0: así lo hace el original). */
void erase_level4_sprite(void);
/* randomize_l4_pos: elige un frame (random() & 0xf) distinto de los de los otros 3 objetos, calcula
 * la posición y reintenta (hasta 0x20 veces) mientras esté a < 0x32 del gato (check_l4_proximity). */
void randomize_l4_pos(uint16_t bx);
/* calc_l4_obj_pos: y_pos[bx] = l4_platform_offset[frame] - (frame >= 3 ? 0xa : 0) + 3;
 * dims[bx] = l4_obj_x_table[frame] + 8. */
void calc_l4_obj_pos(uint16_t bx);
/* check_l4_proximity (L2171-2185, T22 en tareas.md pero randomize_l4_pos lo necesita): distancia
 * |dx| + |dy| a medias (usa `not` en vez de neg, como el original) comparada con bp. Devuelve CF
 * (true si la distancia < bp). */
bool check_l4_proximity(uint16_t bx, uint16_t bp);
#endif
