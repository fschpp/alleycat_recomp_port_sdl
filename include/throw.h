#ifndef THROW_H
#define THROW_H

#include <stdint.h>
#include <stdbool.h>

/* throw.asm: helpers (T12, PROGRESS.md §6i) + update_thrown_objects y
 * reset_window_state (T13, PROGRESS.md §6j). */

extern uint8_t throw_obj_buf[64]; /* DS 0x04d7..0x0516: sprite de 16 filas x 4 bytes */
extern uint8_t throw_bits;        /* DS 0x0540 */
extern uint8_t in_throw_range;    /* DS 0x04d6 */
extern uint8_t throw_timer;       /* DS 0x0531: cuenta de llamadas hasta el próximo paso (inicial 0) */
extern uint16_t throw_last_tick;  /* DS 0x0544: último tick BIOS procesado (inicial 0) */

/* rotate_throw_bits (L170-190). El original guarda CF con lahf al entrar y lo
 * propaga por 5 bytes de throw_col_data: rcr hacia arriba (pisos 0 y 2,
 * throw_rotate_dir = 0 / 10) o rcl hacia abajo (piso 1, dir = 9).
 * Devuelve el CF de salida. */
bool rotate_throw_bits(bool carry_in);

/* check_throw_range (L193-207), bx = piso. Devuelve ZF (el original sale por
 * flags): false (ZF=0) solo si cat_y_bottom cae dentro del piso Y at_platform==0;
 * en cualquier otro caso true. Efecto: in_throw_range (1 solo si dentro del
 * piso Y at_platform>=1). */
bool check_throw_range(uint16_t floor);

/* generate_throw_pattern (L257-274): un random; &6; si ==6 devuelve 0 sin
 * escribir; si no copia 8 words de throw_pattern_ptrs[dx] a throw_obj_buf con
 * paso de 4 bytes empezando en *di_off y devuelve 1. *di_off es el offset
 * dentro de throw_obj_buf (en el original, di; solo al final cambia en el
 * caso copiado: +32). */
uint8_t generate_throw_pattern(uint16_t *di_off);

/* generate_throw_object (L211-254), di = throw_obj_buf. bl = throw_chance[dif],
 * bh = throw_y_param[piso]. Orden fijo de random(): 1 (umbral) [+1 (tipo)]
 * [+2 patrones, cada uno 1 random]. */
void generate_throw_object(uint8_t bl, uint8_t bh);

/* update_thrown_objects (L5-167): paso por frame de los objetos lanzados desde
 * ventanas. Sale pronto salvo cuando throw_timer llega a 0, hay tick BIOS nuevo,
 * !transitioning y gravity_y==0. Puede mover cat_x/cat_draw_pos, armar la
 * transición de climb, scrollear la franja del piso en cga_mem y dibujar la
 * columna del objeto. check_vsync se trata como "listo" (ZF=1). */
void update_thrown_objects(void);

/* reset_window_state (L277-280): anim_last_tick = 0; pcjr_delay = 0. */
void reset_window_state(void);

#endif
