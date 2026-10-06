#ifndef HARDWARE_H
#define HARDWARE_H
#include <stdint.h>
#include <stdbool.h>

/* T51: hardware.asm (init_bios_data L49-73, check_special_keys L227-261) y ui.asm (detect_video L7-35,
 * print_startup_msg L41-46). PROGRESS.md §6av. */

/* Matriz de teclas del manejador de INT 9 (DS 0x6b7..0x6cc, 22 bytes, uno por entrada de la tabla de scancodes de
 * DS 0x6a1): 0x80 = suelta, 0x00 = pulsada. init_bios_data la deja toda en 0x80. */
#define KEY_MATRIX_SIZE 0x16
#define KEY_IDX_FIRE    0   /* DS 0x6b7, scancode 0x38 (Alt) */
#define KEY_IDX_RIGHT   2   /* DS 0x6b9, 0x4d */
#define KEY_IDX_LEFT    4   /* DS 0x6bb, 0x4b */
#define KEY_IDX_PAUSE   18  /* DS 0x6c9, 0x1d (Ctrl; el label key_pause es engañoso) */
#define KEY_IDX_DEL     19  /* DS 0x6ca, 0x53 (Del) */
/* T54 (show_attract_mode): teclas que lee la pantalla de seleccion. OJO: el comentario del ASM dice "keys 1-4", pero la
 * tabla de DS 0x6a1 da letras: 0x6c1 = scancode 0x15 (Y), 0x6c2 = 0x31 (N), 0x6c3 = 0x25 (K), 0x6c4 = 0x23 (H),
 * 0x6c5 = 0x14 (T), 0x6c6 = 0x1e (A). Y/N = joystick si/no; K,H,T,A = dificultad 0..3. */
#define KEY_IDX_JOY_YES 10  /* DS 0x6c1, 0x15 (Y) */
#define KEY_IDX_JOY_NO  11  /* DS 0x6c2, 0x31 (N) */
#define KEY_IDX_DIFF0   12  /* DS 0x6c3, 0x25 (K) */
#define KEY_IDX_DIFF1   13  /* DS 0x6c4, 0x23 (H) */
#define KEY_IDX_DIFF2   14  /* DS 0x6c5, 0x14 (T) */
#define KEY_IDX_DIFF3   15  /* DS 0x6c6, 0x1e (A) */
extern uint8_t key_matrix[KEY_MATRIX_SIZE];
extern uint16_t keyboard_prev;     /* DS 0x691 */

/* CRTC registro 2 (posicion del sync horizontal: desplaza la imagen), lo que check_special_keys escribe con
 * out 0x3d4,2 / out 0x3d5,video_mode+0x27. El port no lo dibuja (video.c no centra la imagen). */
extern uint8_t crtc_hsync_pos;
/* Ctrl+Alt+Del: el original hace un reinicio en caliente (0x1234 en 0040:0072 y jmp F000:E05B). El port lo traduce
 * a "salir" (main.c lee este flag). */
extern bool reboot_requested;

void int9_set_scancode(uint8_t scancode, bool pressed);   /* parte de la ISR de INT 9: busca el scancode en DS 0x6a1 y fija la matriz */
void init_bios_data(void);         /* parte del ASM que toca el DS: matriz a 0x80 y keyboard_prev */
void check_special_keys(void);

/* detect_video: modelo de INT 11h y del test de la RAM CGA. Valores por defecto: equipo con adaptador en color
 * (bits 4-5 = 0x20) y RAM correcta; con eso la rutina no hace nada, como en un PC real con CGA. */
extern uint16_t bios_equipment;
extern bool cga_ram_ok;
extern uint8_t bios_equipment_after;   /* byte bajo de 0040:0010 tras la rutina (solo se escribe en la rama 0x30) */
/* devuelve 0 normal, 1 si fallo el test de RAM (el original imprime el error y se cuelga en un bucle infinito) */
int detect_video(void);
void print_startup_msg(const uint8_t *si);

#endif
