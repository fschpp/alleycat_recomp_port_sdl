#include "input.h"
#include "sound.h"
#include "hardware.h"
#include "keyboard.h"
#include "ui.h"
#include <SDL2/SDL.h>

int8_t input_horizontal = 0;
int8_t input_vertical   = 0;
bool   input_fire        = false;

uint8_t sound_enabled = 0xFF;   /* `mov byte [sound_enabled],0xff` (entry.asm) */
bool restart_game    = false;
bool show_attract    = false;
bool pause_requested = false;   /* en desuso desde T61: la pausa es process_keyboard -> show_pause_menu (Esc) */

/* T61 — teclado. El original no sondea el teclado: su ISR de INT 9 (hardware.asm L184-216) busca cada scancode en la
 * tabla de 22 bytes de DS 0x6a1, pone la entrada correspondiente de la matriz (DS 0x6b7+i) a 0x80/0x00 (suelta/
 * pulsada), cuenta las pulsaciones en keyboard_counter y llama a check_special_keys. read_keyboard_dirs y
 * process_keyboard (keyboard.c, literales) leen esa matriz. Aqui se hace el papel de la ISR: cada llamada compara el
 * estado de SDL con el de la llamada anterior y, por cada cambio, hace lo mismo que la ISR (sin repeticion de teclado:
 * el original recibe tambien los codigos repetidos, que solo sumaban al contador).
 *
 * Tabla (orden = DS 0x6a1; la columna XT se comprueba contra ds_pool en tests/test_input_keys.c). Teclas con dos
 * versiones (izquierda/derecha) responden a las dos. */
typedef struct { uint8_t xt; SDL_Scancode a, b; } key_binding;
static const key_binding key_bindings[KEY_MATRIX_SIZE] = {
    { 0x38, SDL_SCANCODE_LALT,   SDL_SCANCODE_RALT },    /*  0 key_fire: Alt            */
    { 0x48, SDL_SCANCODE_UP,     SDL_SCANCODE_UNKNOWN }, /*  1 key_up                   */
    { 0x4d, SDL_SCANCODE_RIGHT,  SDL_SCANCODE_UNKNOWN }, /*  2 key_right                */
    { 0x50, SDL_SCANCODE_DOWN,   SDL_SCANCODE_UNKNOWN }, /*  3 key_down                 */
    { 0x4b, SDL_SCANCODE_LEFT,   SDL_SCANCODE_UNKNOWN }, /*  4 key_left                 */
    { 0x49, SDL_SCANCODE_PAGEUP, SDL_SCANCODE_UNKNOWN }, /*  5 key_mod1: arriba+derecha */
    { 0x51, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_UNKNOWN }, /*  6 key_mod2: abajo+derecha  */
    { 0x4f, SDL_SCANCODE_END,    SDL_SCANCODE_UNKNOWN }, /*  7 key_mod3: abajo+izquierda */
    { 0x47, SDL_SCANCODE_HOME,   SDL_SCANCODE_UNKNOWN }, /*  8 key_mod4: arriba+izquierda */
    { 0x01, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_UNKNOWN }, /*  9 (label key_ctrl): Esc = pausa */
    { 0x15, SDL_SCANCODE_Y,      SDL_SCANCODE_UNKNOWN }, /* 10 (label key_fn): Y        */
    { 0x31, SDL_SCANCODE_N,      SDL_SCANCODE_UNKNOWN }, /* 11 N                        */
    { 0x25, SDL_SCANCODE_K,      SDL_SCANCODE_UNKNOWN }, /* 12 K                        */
    { 0x23, SDL_SCANCODE_H,      SDL_SCANCODE_UNKNOWN }, /* 13 H                        */
    { 0x14, SDL_SCANCODE_T,      SDL_SCANCODE_UNKNOWN }, /* 14 T                        */
    { 0x1e, SDL_SCANCODE_A,      SDL_SCANCODE_UNKNOWN }, /* 15 A                        */
    { 0x1f, SDL_SCANCODE_S,      SDL_SCANCODE_UNKNOWN }, /* 16 key_sound                */
    { 0x13, SDL_SCANCODE_R,      SDL_SCANCODE_UNKNOWN }, /* 17 key_restart              */
    { 0x1d, SDL_SCANCODE_LCTRL,  SDL_SCANCODE_RCTRL },   /* 18 (label key_pause): Ctrl  */
    { 0x53, SDL_SCANCODE_DELETE, SDL_SCANCODE_UNKNOWN }, /* 19 Del                      */
    { 0x32, SDL_SCANCODE_M,      SDL_SCANCODE_UNKNOWN }, /* 20 key_demo                 */
    { 0x0a, SDL_SCANCODE_9,      SDL_SCANCODE_UNKNOWN }, /* 21 key_cheat: 9             */
};

void input_init(void) {
    input_horizontal = 0;
    input_vertical = 0;
    input_fire = false;
}

/* El papel de la ISR de INT 9: un cambio de tecla = int9_set_scancode + (si es pulsacion, bit 7 = 0) keyboard_counter++
 * + check_special_keys. Idempotente: llamarla varias veces por fotograma no repite nada. */
static void sync_key_matrix(void) {
    static bool down[KEY_MATRIX_SIZE];
    const uint8_t *ks = SDL_GetKeyboardState(NULL);
    for (int i = 0; i < KEY_MATRIX_SIZE; i++) {
        bool now = ks[key_bindings[i].a] || (key_bindings[i].b != SDL_SCANCODE_UNKNOWN && ks[key_bindings[i].b]);
        if (now == down[i]) continue;
        down[i] = now;
        int9_set_scancode(key_bindings[i].xt, now);
        if (now) keyboard_counter++;
        check_special_keys();
    }
}

/* read_keyboard_dirs del original (port: la matriz la rellena sync_key_matrix). */
void input_poll(void) {
    sync_key_matrix();
    read_keyboard_dirs();
    input_fire = (key_matrix[KEY_IDX_FIRE] == 0x0);       /* joy_button == 0 */
}

/* process_keyboard del original. */
void input_process_keys(void) {
    sync_key_matrix();
    process_keyboard();
}
