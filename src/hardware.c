/* hardware.c — partes portables de hardware.asm / ui.asm (T51). Ver include/hardware.h. */
#include <stdint.h>
#include <string.h>
#include "hardware.h"
#include "cat_state.h"
#include "ui.h"
#include "gen/ds_pool.h"

uint8_t  key_matrix[KEY_MATRIX_SIZE];
uint16_t keyboard_prev;
uint8_t  crtc_hsync_pos;
bool     reboot_requested;
uint16_t bios_equipment = 0x0020;
bool     cga_ram_ok = true;
uint8_t  bios_equipment_after;

#define DS_MSG_COLOR_DISPLAY 0x60f0   /* "Please turn on the color display." */
#define DS_MSG_NEED_CGA      0x6112   /* "This program requires a color/graphics adapter." */

/* init_bios_data (L49-73), parte DS: rep stosb 0x16 bytes de 0x80 en 0x6b7; keyboard_prev = keyboard_counter - 0x70.
 * El resto (leer 0040:0012 a [cs:0x13e7]) es del manejador de INT 9 del PCjr y no existe en el port. */
void init_bios_data(void) {
    memset(key_matrix, 0x80, sizeof key_matrix);
    keyboard_prev = (uint16_t)(keyboard_counter - 0x70);
}

/* check_special_keys (L227-261), llamada al final de cada interrupcion de teclado. Solo actua con Ctrl (0x6c9) y
 * Alt (0x6b7) pulsadas a la vez (`or al,..; cmp al,0; jnz ret`: cualquier tecla suelta = 0x80 lo anula):
 *   - Del pulsada (bit 7 de 0x6ca a 0): Ctrl+Alt+Del = reinicio en caliente (-> reboot_requested).
 *   - si no, Derecha pulsada: video_mode-- hasta 0; si no, Izquierda pulsada: video_mode++ hasta 7.
 *   - tras cambiar video_mode escribe CRTC[2] = video_mode + 0x27 (mueve la imagen horizontalmente; con el valor
 *     inicial 6 queda el 0x2d estandar de CGA, con 4 el 0x2b del PCjr). video_mode (DS 0x690) no es un modo de
 *     video sino ese desplazamiento. */
void check_special_keys(void) {
    uint8_t al = (uint8_t)(key_matrix[KEY_IDX_PAUSE] | key_matrix[KEY_IDX_FIRE]);   /* mov al,[0x6c9] / or al,[0x6b7] */
    if (al != 0x0) goto lab_15c9;
    if (key_matrix[KEY_IDX_DEL] & 0x80) goto lab_158d;     /* test [0x6ca],0x80 / jnz */
    reboot_requested = true;                               /* out 0x20,0x20 / jmp lab_1557 (reinicio) */
    return;
lab_158d:
    if (key_matrix[KEY_IDX_RIGHT] & 0x80) goto lab_15a4;   /* test [0x6b9],0x80 / jnz */
    if (video_mode < 0x1) goto lab_15c9;                   /* cmp byte [0x690],1 / jb */
    video_mode--;
    goto lab_15b9;
lab_15a4:
    if (key_matrix[KEY_IDX_LEFT] & 0x80) goto lab_15c9;    /* test [0x6bb],0x80 / jnz */
    if (video_mode >= 0x7) goto lab_15c9;                  /* cmp byte [0x690],7 / jnb */
    video_mode++;
lab_15b9:
    crtc_hsync_pos = (uint8_t)(video_mode + 0x27);         /* out 0x3d4,2 / out 0x3d5,[0x690]+0x27 */
lab_15c9:
    return;
}

/* print_startup_msg (ui.asm L41-46): fija DS al segmento de datos y llama a print_string. */
void print_startup_msg(const uint8_t *si) {
    print_string(si);
}

/* detect_video (ui.asm L7-35). INT 11h: con los bits 4-5 distintos de 0x30 (monocromo) vuelve sin hacer nada
 * (lab_5c95). Con 0x30 prueba la RAM CGA (escribe 0x55aa y lo relee): si falla imprime el error y se cuelga
 * (aqui devuelve 1); si pasa imprime "Please turn on the color display.", deja los bits 4-5 de 0040:0010 en 01
 * (color 40x25) y pone el modo CGA 4 (que en el port ya lo crea game_hw_init/SDL). */
int detect_video(void) {
    if ((bios_equipment & 0x30) != 0x30) return 0;         /* and al,0x30 / cmp al,0x30 / jnz lab_5c95 */
    if (!cga_ram_ok) {                                     /* cmp ax,0x55aa / jnz lab_5c96 */
        print_startup_msg(&ds_pool[DS_MSG_NEED_CGA]);
        return 1;                                          /* lab_5c9c: jmp short lab_5c9c (bucle infinito) */
    }
    print_startup_msg(&ds_pool[DS_MSG_COLOR_DISPLAY]);
    bios_equipment_after = (uint8_t)(((bios_equipment & 0xff) & 0xcf) | 0x10);   /* and al,0xcf / or al,0x10 */
    return 0;
}

/* Parte de la ISR de INT 9 (hardware.asm, `in al,0x60 / mov ah,al / and al,0x7f / ... repne scasb / sub di,0x6a2 /
 * and ah,0x80 / mov [di+0x6b7],ah`): el scancode sin el bit 7 se busca en los 22 bytes de DS 0x6a1; si esta, la entrada
 * de la matriz toma el bit 7 (1 = soltada = 0x80, 0 = pulsada = 0x00). El contador keyboard_counter lo incrementa
 * quien llame (main.c) al pulsar. */
void int9_set_scancode(uint8_t scancode, bool pressed) {
    for (int i = 0; i < KEY_MATRIX_SIZE; i++) {
        if (ds_pool[0x6a1 + i] == (uint8_t)(scancode & 0x7f)) {
            key_matrix[i] = pressed ? 0x00 : 0x80;
            return;
        }
    }
}
