/* keyboard.c — read_keyboard_dirs y process_keyboard (input.asm L90-193). T61. Ver include/keyboard.h y PROGRESS.md §6bd.
 * Sin SDL: lee key_matrix; el relleno de la matriz (la ISR de INT 9) esta en input.c. */
#include <stdint.h>
#include <stdbool.h>
#include "keyboard.h"
#include "cat_state.h"
#include "hardware.h"
#include "input.h"
#include "sound.h"
#include "ui.h"

/* read_keyboard_dirs (L90-141). Cada bloque hace `al = tecla [& mod & mod]`, `xor al,0x80`, `jz`: la matriz solo
 * guarda 0x80 (suelta) o 0 (pulsada), asi que el AND vale 0x80 solo si TODAS estan sueltas. */
void read_keyboard_dirs(void) {
    uint8_t al;
    al = key_matrix[KEY_IDX_DOWN];                         /* mov al,[key_down] */
    if (rom_id == 0xfd) goto lab_12d3;                     /* cmp byte [rom_id],0xfd / jz (PCjr: sin mods) */
    al &= key_matrix[KEY_IDX_MOD2];
    al &= key_matrix[KEY_IDX_MOD3];
lab_12d3:
    al ^= 0x80;
    if (al == 0) goto lab_12d9;                            /* jz */
    al = 0x1;
lab_12d9:
    input_vertical = (int8_t)al;
    al = key_matrix[KEY_IDX_UP];
    if (rom_id == 0xfd) goto lab_12ee;
    al &= key_matrix[KEY_IDX_MOD1];
    al &= key_matrix[KEY_IDX_MOD4];
lab_12ee:
    al ^= 0x80;
    if (al == 0) goto lab_12f7;
    input_vertical = (int8_t)0xff;                         /* arriba pisa a abajo */
lab_12f7:
    al = key_matrix[KEY_IDX_RIGHT];
    if (rom_id == 0xfd) goto lab_1309;
    al &= key_matrix[KEY_IDX_MOD1];
    al &= key_matrix[KEY_IDX_MOD2];
lab_1309:
    al ^= 0x80;
    if (al == 0) goto lab_130f;
    al = 0x1;
lab_130f:
    input_horizontal = (int8_t)al;
    al = key_matrix[KEY_IDX_LEFT];
    if (rom_id == 0xfd) goto lab_1324;
    al &= key_matrix[KEY_IDX_MOD3];
    al &= key_matrix[KEY_IDX_MOD4];
lab_1324:
    al ^= 0x80;
    if (al == 0) goto lab_132d;
    input_horizontal = (int8_t)0xff;                       /* izquierda pisa a derecha */
lab_132d:
    joy_button = (uint8_t)(key_matrix[KEY_IDX_FIRE] >> 3); /* mov al,[key_fire] / mov cl,3 / shr al,cl: 0x10 suelta, 0 pulsada */
}

/* process_keyboard (L148-193). Los nombres del ASM engañan (ver hardware.h): key_ctrl = Esc, key_pause = Ctrl,
 * key_fn = Y. Un bit 7 a 0 = pulsada. */
void process_keyboard(void) {
    uint16_t ax;
    ax = keyboard_counter;                                 /* mov ax,[keyboard_counter] */
    if (ax == keyboard_prev) goto lab_1357;                /* cmp ax,[keyboard_prev] / jz: ninguna pulsacion nueva */
    keyboard_prev = ax;
    if ((key_matrix[KEY_IDX_ESC] & 0x80) != 0) goto lab_1358;   /* test [key_ctrl],0x80 / jnz: Esc suelta */
    ax = keyboard_counter;
    if (ax == pause_counter) goto lab_1357;                /* cmp ax,[pause_counter] / jz: la pulsacion que cerro la pausa */
    show_pause_menu();
lab_1357:
    return;
lab_1358:
    if ((key_matrix[KEY_IDX_PAUSE] & 0x80) == 0) goto lab_1360;   /* test [key_pause],0x80 / jz: Ctrl pulsada */
    return;
lab_1360:
    if ((key_matrix[KEY_IDX_CHEAT] & 0x80) != 0) goto lab_136d;   /* Ctrl+9 */
    lives_count = 0x9;
    return;
lab_136d:
    if ((key_matrix[KEY_IDX_QUIT] & 0x80) == 0) goto lab_13a5;    /* Ctrl+Y */
    if ((key_matrix[KEY_IDX_DEMO] & 0x80) != 0) goto lab_1381;    /* Ctrl+M */
    show_attract = true;                                   /* mov byte [show_attract],0xff */
    return;
lab_1381:
    if ((key_matrix[KEY_IDX_RESTART] & 0x80) != 0) goto lab_138e; /* Ctrl+R */
    restart_game = true;
    return;
lab_138e:
    if ((key_matrix[KEY_IDX_SOUND] & 0x80) != 0) goto lab_13a4;   /* Ctrl+S */
    sound_enabled = (uint8_t)~sound_enabled;              /* not byte [sound_enabled] */
    if (sound_enabled != 0x0) goto lab_13a3;
    silence_speaker();
lab_13a3:
    return;
lab_13a4:
    return;
lab_13a5:
    /* restore_handlers / pop ax / retf: el original vuelve al DOS. El port lo traduce a "salir" (main.c). */
    quit_requested = true;
}
