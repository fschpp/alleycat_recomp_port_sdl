/* transition.c — enemy.asm level_transition (L110-158, T44; PROGRESS.md §6ao).
 * En un .c aparte (no en game_flow.c) para que game_level_enter()/game_level_exit() la llamen a traves de
 * una frontera de unidad y los tests puedan envolverla con --wrap. */
#include "cat_state.h"
#include "game_flow.h"
#include "palette.h"
#include "sound.h"
#include "level7_epilogue.h"

uint16_t wipe_fill_pattern = 0;     /* DS 0x1839 (word): patron de relleno del barrido; lo lee animate_screen_wipe (T45) */

/* Se llama al entrar a un nivel (level_number = el nuevo, level_state = 0) y al salir (level_number = 0,
 * level_state = el nivel que se acaba de jugar). cat_caught ([0x553]) hace aqui de "nivel completado". */
void level_transition(void) {
    uint16_t ax;
    bios_color_select(0x0, 0x0);                       /* sub bx,bx / mov ah,0xb / int 10h: fondo negro */
    if (level_state != 0x7) goto lab_1c12;             /* cmp word [0x6],7 */
    if (cat_caught == 0x0) goto lab_1c12;              /* cmp byte [0x553],0 */
    run_victory_sequence();
    cat_x = 0x98;
    cat_y = 0x5f;
lab_1c12:
    wipe_fill_pattern = 0x0;
    animate_screen_wipe();
    silence_speaker();
    set_palette();
    if (level_number != 0x0) goto lab_1c49;            /* cmp word [0x4],0 */
    if (cat_caught == 0x0) goto lab_1c46;
    if (level_state != 0x7) goto lab_1c41;
    play_victory_march();
    goto lab_1c49;
lab_1c41:
    handle_level_complete();
    goto lab_1c49;
lab_1c46:
    show_level_result();
lab_1c49:
    if (level_number == 0x7) goto lab_1c5a;
    ax = 0xaaaa;
    if (level_number != 0x2) goto lab_1c5d;
lab_1c5a:
    ax = 0x5555;
lab_1c5d:
    wipe_fill_pattern = ax;
    animate_screen_wipe();
    silence_speaker();
}
