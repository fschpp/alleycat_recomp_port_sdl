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

/* --- T22: helpers B (level_objects.asm L2150-2177) y cola de init_level4_bg (L1877-1896) --- */

/* check_l4_obj_cat: objeto idx (x = l5_obj_dims[idx], y = l5_obj_y_pos[idx], 0x10 x 0x0c) vs. gato
 * (0x18 x 0x0e). Devuelve CF. En el ASM bx = idx y si = 2*idx; se preservan ambos (push/pop). */
bool check_l4_obj_cat(uint16_t idx);
/* check_l4_obj_thrown: igual pero contra el objeto lanzado (thrown_obj_x/y, 0x10 x 0x1e). */
bool check_l4_obj_thrown(uint16_t idx);

/* dat_3ce3/dat_3ce4 (DS 0x3ce3, 16 bytes entrelazados: par = dat_3ce3[si], impar = dat_3ce4[si]) y
 * dat_3cf3/dat_3cf4 (DS 0x3cf3, 16 bytes igual). Los siembra la cola de init_level4_bg; los consume
 * update_level4_state (L1762, T24). Semántica aún sin nombrar: se dejan con el nombre del ASM. */
extern uint8_t l4_dat_3ce3[16];
extern uint8_t l4_dat_3cf3[16];

/* Cola de init_level4_bg (L1877-1896): bx = (difficulty_level & 3) << 3 (8 bits, bh intacto) y
 * 8 bytes de dat_3cc3[bx..bx+7]; cada byte -> nibble alto en dat_3ce3[si], nibble bajo en
 * dat_3ce4[si] (= dat_3ce3[si+1]); dat_3cf3/dat_3cf4 = 0. */
void init_level4_bg_tail(void);

/* --- T23: update_level4_anim (level_objects.asm L1933-2093) --- */

/* Hook de pruebas del tick BIOS (`int 0x1a`): -1 = reloj real (~18.2 Hz); >= 0 = ese valor. */
extern int32_t l4_tick_override;
extern uint16_t l4_dat_3de4;   /* DS 0x3de4: dirección CGA del último dibujo (expuesta para tests) */

/* update_level4_anim: un objeto por tick BIOS (l5_obj_index 1,2,3,0,1,...): colisión con el gato
 * (lo "atrapa" y dibuja el icono de bonus), con el objeto lanzado (se queda como está), y si no,
 * la animación de aparición/desaparición/movimiento del objeto (`l5_obj_anim` 0x14..0 con sprites
 * 0x3de0[]/0x3d80/0x3db0, o ninguno si anim >= 0x14). */
void update_level4_anim(void);
#endif
