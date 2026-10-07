#ifndef ANIMATION_C_H
#define ANIMATION_C_H
/* animation_c (T72) — game_loop.asm L441-684: el resto de update_animation para niveles != 2, que empieza en
 * lab_0bac (destino de UA_L0BAC en animation_entry.h). C1 = L441-560 (lab_0bac .. lab_0ce7), C2 = L561-684.
 * PROGRESS.md §6bg. SIN CABLEAR hasta T74/T75. */

typedef enum {
    AC_RET,     /* `ret` (lab_0bb1 / lab_0bd2 / lab_0c1b / lab_0c66) */
    AC_L0CE7,   /* se llega al final del rango C1: sigue en lab_0ce7 (C2) */
    AC_L0D29,   /* check_level_collision con CF=1: sigue en lab_0d29 con al = cat_y_bottom (C2) */
    AC_L0E23,   /* `jmp near lab_0e23` (== update_alley_movement) */
    AC_L0E78    /* `jmp near lab_0e78` (== update_alley_movement desde la lectura de input_horizontal) */
} ac_next_t;

/* lab_0bac .. lab_0ce7: choque con el perro, auto-entrada (entry_steps/entry_delay), plataforma (at_platform),
 * rebote de update_scroll, contadores de anim_step/anim_counter y temporizador de transicion. */
ac_next_t update_animation_c1(void);

/* lab_0ce7 .. lab_0e1f (T72 C2, game_loop.asm L561-684): movimiento vertical de cat_y_bottom (+-anim_counter), limites
 * 0xe6/0xf8, fin de transicion (lab_0d29), cat_y/cat_screen_pos, borrado, choque con perro/enemigo, eleccion del sprite
 * (vert_sprite o ciclo de retroceso recoil si auto_walk), recorte por arriba/abajo y dibujo. Termina siempre en `ret`.
 * `from` es lo que devolvio update_animation_c1: AC_L0CE7 (entra en lab_0ce7) o AC_L0D29 (entra en lab_0d29 con
 * al = cat_y_bottom). Cualquier otro valor no hace nada. */
void update_animation_c2(ac_next_t from);

#endif
