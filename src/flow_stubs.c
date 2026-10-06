/* Stubs de funciones del flujo de entry.asm que pertenecen a tareas aun no hechas.
 * Estan en un .c aparte (y no en game_flow.c) para que los tests puedan
 * envolverlos con -Wl,--wrap, y para que cada tarea borre solo su stub. */
#include "game_flow.h"
#include "input.h"
#include "score.h"

void show_title_screen(void) { /* TODO(T52): ui.asm show_title_screen */ }
void love_scene_outro(void)     { /* TODO(T58): ui.asm L489-570 */ }
/* Helpers de la barra de bonus (level_objects.asm L1244-1375). handle_level_complete (T47) ya los llama. */
void mask_score_tiles(uint16_t dx)  { (void)dx; /* TODO(T48): L1303-1316 */ }
void print_bonus_score(void)        { /* TODO(T48): L1262-1281 */ }
void print_level7_bonus(void)       { /* TODO(T48): L1282-1302 */ }
void animate_score_bar(uint16_t ax) { (void)ax; /* TODO(T49): L1317-1347 */ }
void binary_to_bcd(uint16_t ax)     { (void)ax; /* TODO(T49): L1348-1367 */ }
/* flash_score_color: el original devuelve dx = tick leido al entrar (int 0x1a); el stub solo lo devuelve para que el
 * bucle del final de nivel termine. TODO(T48): L1244-1261 (borde alternando 0 / bonus_color segun el bit 2 del tick). */
uint16_t flash_score_color(void)    { return score_tick(); }

void reset_cupid(void)       { /* TODO(T56): ui.asm reset_cupid */ }
void update_cupid(void)      { /* TODO(T56): ui.asm update_cupid */ }
void show_attract_mode(void) { /* TODO(T54): ui.asm show_attract_mode */ }

/* Respaldo debil de input_process_keys (input.c usa SDL y no esta en TEST_SRC): con input.c
 * enlazado gana su definicion fuerte; los tests sin SDL usan este no-op o definen la suya. */
__attribute__((weak)) void input_process_keys(void) { }
