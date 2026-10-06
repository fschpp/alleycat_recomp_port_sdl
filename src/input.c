#include "input.h"
#include "sound.h"
#include "hardware.h"
#include <SDL2/SDL.h>

int8_t input_horizontal = 0;
int8_t input_vertical   = 0;
bool   input_fire        = false;

uint8_t sound_enabled = 0xFF;   /* `mov byte [sound_enabled],0xff` (entry.asm) */
bool restart_game    = false;
bool show_attract    = false;
bool pause_requested = false;

/* Scancode bindings recovered from the original's key-matrix table in
 * cat.asm (the 22-byte table scanned by the custom INT 9 handler in
 * hardware.asm / init_bios_data). Original PC XT scancodes -> SDL scancodes:
 *   key_fire    = 0x38 Alt        -> SDL_SCANCODE_LALT
 *   key_up      = 0x48 Up arrow   -> SDL_SCANCODE_UP
 *   key_right   = 0x4D Right      -> SDL_SCANCODE_RIGHT
 *   key_down    = 0x50 Down       -> SDL_SCANCODE_DOWN
 *   key_left    = 0x4B Left       -> SDL_SCANCODE_LEFT
 *   key_sound   = 0x1F 'S'        -> SDL_SCANCODE_S
 *   key_restart = 0x13 'R'        -> SDL_SCANCODE_R
 *   key_demo    = 0x32 'M'        -> SDL_SCANCODE_M
 *   key_cheat   = 0x0A '9'        -> SDL_SCANCODE_9   (grants 9 lives)
 *   key_ctrl/pause combo          -> SDL_SCANCODE_ESCAPE / SDL_SCANCODE_P
 * (key_mod1..4 / the PCjr-vs-XT keyboard-matrix workaround in the original
 * don't apply to a modern keyboard driver, so they're not reproduced.) */

void input_init(void) {
    input_horizontal = 0;
    input_vertical = 0;
    input_fire = false;
}

void input_poll(void) {
    const uint8_t *ks = SDL_GetKeyboardState(NULL);

    if (ks[SDL_SCANCODE_LEFT])       input_horizontal = -1; /* 0xFF in original */
    else if (ks[SDL_SCANCODE_RIGHT]) input_horizontal = 1;
    else                              input_horizontal = 0;

    if (ks[SDL_SCANCODE_UP])         input_vertical = -1;
    else if (ks[SDL_SCANCODE_DOWN])  input_vertical = 1;
    else                              input_vertical = 0;

    input_fire = ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_RALT] || ks[SDL_SCANCODE_SPACE];
}

void input_process_keys(void) {
    const uint8_t *ks = SDL_GetKeyboardState(NULL);
    static bool prev_s = false, prev_r = false, prev_m = false;

    bool s = ks[SDL_SCANCODE_S];
    if (s && !prev_s) {
        /* input.asm: `not byte [sound_enabled]`, then silence the speaker if
         * the toggle turned it off. */
        sound_enabled = (uint8_t)~sound_enabled;
        if (sound_enabled == 0) silence_speaker();
    }
    prev_s = s;

    bool r = ks[SDL_SCANCODE_R];
    if (r && !prev_r) restart_game = true;
    prev_r = r;

    bool m = ks[SDL_SCANCODE_M];
    if (m && !prev_m) show_attract = true;
    prev_m = m;

    pause_requested = ks[SDL_SCANCODE_ESCAPE] || ks[SDL_SCANCODE_P];

    /* T51: el manejador de INT 9 del original llama a check_special_keys con cada scancode (pulsar o soltar). Aqui se
     * rellenan las 5 entradas de la matriz que lee y se llama en cada cambio (sin repeticion de teclado):
     * Ctrl+Alt+Del = reinicio (reboot_requested), Ctrl+Alt+Izq/Der = desplazar la imagen (crtc_hsync_pos). */
    {
        static uint8_t prev[5] = { 0x80, 0x80, 0x80, 0x80, 0x80 };
        uint8_t cur[5];
        cur[0] = (ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_RALT]) ? 0x00 : 0x80;     /* KEY_IDX_FIRE */
        cur[1] = ks[SDL_SCANCODE_RIGHT] ? 0x00 : 0x80;                              /* KEY_IDX_RIGHT */
        cur[2] = ks[SDL_SCANCODE_LEFT] ? 0x00 : 0x80;                               /* KEY_IDX_LEFT */
        cur[3] = (ks[SDL_SCANCODE_LCTRL] || ks[SDL_SCANCODE_RCTRL]) ? 0x00 : 0x80;  /* KEY_IDX_PAUSE */
        cur[4] = ks[SDL_SCANCODE_DELETE] ? 0x00 : 0x80;                             /* KEY_IDX_DEL */
        bool changed = false;
        for (int i = 0; i < 5; i++) changed = changed || cur[i] != prev[i];
        if (changed) {
            key_matrix[KEY_IDX_FIRE] = cur[0]; key_matrix[KEY_IDX_RIGHT] = cur[1]; key_matrix[KEY_IDX_LEFT] = cur[2];
            key_matrix[KEY_IDX_PAUSE] = cur[3]; key_matrix[KEY_IDX_DEL] = cur[4];
            for (int i = 0; i < 5; i++) prev[i] = cur[i];
            check_special_keys();
        }
    }
}
