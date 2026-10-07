#ifndef KEYBOARD_H
#define KEYBOARD_H
/* read_keyboard_dirs y process_keyboard (input.asm L90-193). T61, PROGRESS.md §6bd. Trabajan sobre key_matrix
 * (hardware.h: 0x80 = suelta, 0 = pulsada), como el original; input.c (SDL) solo rellena la matriz. */

/* read_keyboard_dirs: input_vertical / input_horizontal (-1/0/1) desde la matriz y joy_button = key_fire >> 3
 * (0x10 suelta, 0 pulsada). En PC/XT (rom_id != 0xfd) abajo/arriba/derecha/izquierda tambien responden a las
 * diagonales (key_mod1..4); arriba gana a abajo e izquierda a derecha. */
void read_keyboard_dirs(void);

/* process_keyboard: actua una vez por cada cambio de keyboard_counter (una pulsacion). Esc pulsada -> show_pause_menu
 * (si no es la misma pulsacion que cerro la pausa anterior). Con Ctrl: 9 = lives_count 9; Y = salir
 * (quit_requested); M = show_attract; R = restart_game; S = alterna sound_enabled (y silencia al apagar). */
void process_keyboard(void);
#endif
