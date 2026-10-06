#ifndef UI_H
#define UI_H
#include <stdint.h>

/* Helpers de texto / entrada bloqueante de ui.asm (T50): print_string L199-214, set_cursor L233-242,
 * wait_for_input L379-397, display_text_line L399-413, clear_cga L415-429. PROGRESS.md §6au.
 * El texto sale por el modelo de INT 10h de include/bios_text.h. */

/* DS 0x0693 (word): lo incrementa el manejador de INT 9 del original en cada pulsacion; wait_for_input lo vigila.
 * En el port lo incrementa main.c (SDL_KEYDOWN) a traves de ui_wait_hook. */
extern uint16_t keyboard_counter;
/* DS 0x6d8f (word): indice en bytes (de 2 en 2) a las tablas de cursor/cadenas de display_text_line. */
extern uint16_t title_joy_offset;

/* Llamado en cada vuelta de las esperas bloqueantes (NULL = nada): main.c bombea eventos SDL, cuenta teclas y
 * presenta. Los tests lo usan para simular pulsaciones. */
extern void (*ui_wait_hook)(void);
/* in al,0x201 (puerto del joystick): NULL = sin joystick conectado (devuelve 0xff, ningun boton). El bit 4 a 0 es
 * boton pulsado (se lee negado en wait_for_input). */
extern uint8_t (*joy_port_fn)(void);

/* print_string: imprime la cadena terminada en 0 con teletype, color 2; devuelve el puntero tras el terminador
 * (SI al salir). */
const uint8_t *print_string(const uint8_t *si);
/* set_cursor: cursor a (fila dh, columna 0). */
void set_cursor(uint8_t dh);
/* wait_for_input: con use_joystick espera el boton (bit 4 del puerto a 0); si no, a que cambie keyboard_counter. */
void wait_for_input(void);
/* display_text_line: fila de attract_icon_sprite_b[title_joy_offset] (byte alto del word), cadena de
 * attract_icon_sprite_a[title_joy_offset]; title_joy_offset += 2. */
void display_text_line(void);
/* clear_cga: pone a 0 los dos bancos de la CGA (0xfa0 words cada uno, desde 0 y desde 0x2000). */
void clear_cga(void);


/* --- T52/T53: pantalla de titulo (ui.asm L55-163 y L164-236). PROGRESS.md §6aw. --- */
/* show_title_screen: dibuja la pantalla, y bloquea (como el original) hasta una tecla/boton o hasta el timeout hacia
 * el modo demo. Gira llamando a ui_wait_hook (main.c presenta y bombea SDL) en cada vuelta. */
void show_title_screen(void);
/* move_title_cat: rebota el gato entre x=0x20 y 0x120 con cambios de direccion aleatorios y avanza su animacion. */
void move_title_cat(void);
/* animate_title_icon: alterna los 2 frames del icono del titulo (attract_icon_ptrs). */
void animate_title_icon(void);

/* --- T54: show_attract_mode (ui.asm L300-382) y detect_joystick / test_joystick_axis (L431-469). --- */
extern uint16_t title_input_tick;   /* DS 0x6dfa: tick de arranque de test_joystick_axis */
/* test_joystick_axis / detect_joystick devuelven el flag CF del original: 0 = joystick presente, 1 = no. */
int test_joystick_axis(void);
int detect_joystick(void);
/* show_attract_mode: pantalla Y/N de joystick, dificultad (K/H/T/A -> diff_icon_idx 0..3) e instrucciones; fija
 * use_joystick y diff_icon_idx. Bloquea como el original; las teclas se leen de key_matrix (hardware.h). */
void show_attract_mode(void);
#endif
