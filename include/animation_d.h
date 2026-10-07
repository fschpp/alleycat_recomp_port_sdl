#ifndef ANIMATION_D_H
#define ANIMATION_D_H
/* animation_d (T73) — game_loop.asm L685-817 (lab_0e23 .. lab_0f86): el final de update_animation para niveles != 2.
 * lab_0e23: entrada por geometria (check_level_collision; nivel 0: check_jump_collision / salto forzado),
 * lab_0e78: lectura de input_horizontal/vertical, lab_0e91..lab_0f33: poses de subida/transicion (sin dibujar),
 * lab_0f34..lab_0f86: huella, update_scroll, ventana idle o paso de caminata + dibujo.
 * Termina siempre en `ret`. PROGRESS.md §6bi. SIN CABLEAR hasta T74/T75 (los niveles aun llaman a
 * update_alley_movement, que difiere en los puntos listados en §6bi). */
#include "animation_c.h"

/* `from` es lo que devolvio update_animation_c1: AC_L0E23 o AC_L0E78. Cualquier otro valor no hace nada. */
void update_animation_d(ac_next_t from);

#endif
