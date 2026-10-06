/* Stubs de funciones del flujo de entry.asm que pertenecen a tareas aun no hechas.
 * Estan en un .c aparte (y no en game_flow.c) para que los tests puedan
 * envolverlos con -Wl,--wrap, y para que cada tarea borre solo su stub. */
#include "game_flow.h"
#include "input.h"
#include "score.h"

void love_scene_outro(void)     { /* TODO(T58): ui.asm L489-570 */ }

void reset_cupid(void)       { /* TODO(T56): ui.asm reset_cupid */ }
void update_cupid(void)      { /* TODO(T56): ui.asm update_cupid */ }
void show_attract_mode(void) { /* TODO(T54): ui.asm show_attract_mode */ }

/* Respaldo debil de input_process_keys (input.c usa SDL y no esta en TEST_SRC): con input.c
 * enlazado gana su definicion fuerte; los tests sin SDL usan este no-op o definen la suya. */
__attribute__((weak)) void input_process_keys(void) { }
