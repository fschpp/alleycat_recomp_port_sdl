/* Stubs de funciones del flujo de entry.asm que pertenecen a tareas aun no hechas.
 * Estan en un .c aparte (y no en game_flow.c) para que los tests puedan
 * envolverlos con -Wl,--wrap, y para que cada tarea borre solo su stub. */
#include "game_flow.h"
#include "input.h"
#include "score.h"

void love_scene_outro(void)     { /* TODO(T58): ui.asm L489-570 */ }

/* T57 (ui.asm L676-791): draw_cupid, erase_cupid, cupid_toggle_window, check_cupid_collision (devuelve CF). */
#include "cupid.h"
void draw_cupid(void)          { /* TODO(T57) */ }
void erase_cupid(void)         { /* TODO(T57) */ }
void cupid_toggle_window(void) { /* TODO(T57) */ }
int  check_cupid_collision(void) { /* TODO(T57) */ return 0; }

/* Respaldo debil de input_process_keys (input.c usa SDL y no esta en TEST_SRC): con input.c
 * enlazado gana su definicion fuerte; los tests sin SDL usan este no-op o definen la suya. */
__attribute__((weak)) void input_process_keys(void) { }
