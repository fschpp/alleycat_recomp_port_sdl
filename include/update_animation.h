#ifndef UPDATE_ANIMATION_H
#define UPDATE_ANIMATION_H
/* update_animation (T74) — game_loop.asm L158-817 completo: el despachador que une las piezas auditadas en T70-T73.
 *   entrada (animation_entry.c, T70)
 *     UA_RET    -> nada
 *     UA_L09F6  -> update_cat_movement() + update_cat_frame()          (nivel 2, fase normal)
 *     UA_L0A86  -> update_cat_dive()     + update_cat_frame()          (nivel 2, fase de muerte)
 *     UA_L0BAC  -> update_animation_c1() (animation_c.c, T72)
 *                    AC_RET              -> nada
 *                    AC_L0CE7 / AC_L0D29 -> update_animation_c2()
 *                    AC_L0E23 / AC_L0E78 -> update_animation_d() (animation_d.c, T73)
 * Sustituye a update_alley_movement (camino de demo, difiere en 5 puntos, PROGRESS.md §6bi) en todos los sitios que
 * el ASM llama a `update_animation`. Ver PROGRESS.md §6bj. */
void update_animation(void);

#endif
