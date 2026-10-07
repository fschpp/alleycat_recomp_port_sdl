#ifndef ANIMATION_ENTRY_H
#define ANIMATION_ENTRY_H
/* animation_entry (T70) — game_loop.asm L158-285: la ENTRADA de update_animation
 * (update_animation .. lab_0a1a). Compuerta por tick BIOS / pcjr_delay, bloqueos de los niveles 4 y 6
 * y el bloque propio del nivel 2 (tiempo de fase, maullido, desplazamiento de color del borde, muerte).
 * Ver PROGRESS.md §6be. SIN CABLEAR hasta T74/T75: hoy los niveles llaman directo a
 * update_alley_movement (lab_0e23). */
#include <stdint.h>

typedef enum {
    UA_RET,     /* lab_08fc: `ret` — no toca nada este frame */
    UA_L0BAC,   /* `jmp near lab_0bac` — niveles != 2: sigue el resto de update_animation (T72) */
    UA_L09F6,   /* nivel 2, fase normal: borde fijado; sigue lab_09f6 = update_cat_movement() (ya hace la
                 * instantanea prev_* y el tramo lab_0a1a..) y luego select_cat_sprite() */
    UA_L0A86    /* nivel 2, fase de muerte: sigue lab_0a86 = update_cat_dive() y luego lo que viene despues */
} ua_next_t;

/* Solo tests: >= 0 sustituye a `int 0x1a` (mismo patron que l4_tick_override/l6_tick_override). */
extern int32_t ua_tick_override;
extern uint16_t ua_anim_tick_delay;   /* DS 0x057f (lo escribe lab_0926) */

/* Devuelve a que etiqueta del ASM sigue el control. */
ua_next_t update_animation_entry(void);

#endif
