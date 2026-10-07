/* T61 — read_keyboard_dirs y process_keyboard (input.asm L90-193). Sin SDL. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cat_state.h"
#include "hardware.h"
#include "input.h"
#include "keyboard.h"
#include "sound.h"
#include "ui.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static int pause_calls, silence_calls;
void __wrap_show_pause_menu(void) { pause_calls++; }
void __wrap_silence_speaker(void) { silence_calls++; }

static void release_all(void) { memset(key_matrix, 0x80, KEY_MATRIX_SIZE); }
static void press(int idx) { key_matrix[idx] = 0x00; }

int main(void) {
    /* ---- read_keyboard_dirs: las 2^8 combinaciones de las 8 teclas de direccion, contra la especificacion */
    const int keys[8] = { KEY_IDX_UP, KEY_IDX_DOWN, KEY_IDX_LEFT, KEY_IDX_RIGHT, KEY_IDX_MOD1, KEY_IDX_MOD2, KEY_IDX_MOD3, KEY_IDX_MOD4 };
    for (int rom = 0; rom < 2; rom++)
    for (int m = 0; m < 256; m++) {
        release_all(); rom_id = rom ? 0xfd : 0xff;
        for (int b = 0; b < 8; b++) if (m & (1 << b)) press(keys[b]);
        bool up = m & 1, down = m & 2, left = m & 4, right = m & 8, pgup = m & 16, pgdn = m & 32, end = m & 64, home = m & 128;
        if (rom) { pgup = pgdn = end = home = false; }        /* PCjr: sin diagonales (y la rama salta los AND) */
        bool u = up || pgup || home, d = down || pgdn || end, l = left || end || home, r = right || pgup || pgdn;
        int8_t ev = u ? -1 : d ? 1 : 0, eh = l ? -1 : r ? 1 : 0;
        input_vertical = 0x55; input_horizontal = 0x55;
        read_keyboard_dirs();
        CHECK(input_vertical == ev && input_horizontal == eh, "rom=%02x teclas=%02x: (h=%d,v=%d) esperado (%d,%d)", rom_id, m, input_horizontal, input_vertical, eh, ev);
    }
    rom_id = 0xff;
    /* joy_button = key_fire >> 3 */
    release_all(); joy_button = 0xee; read_keyboard_dirs();
    CHECK(joy_button == 0x10, "fuego suelto: joy_button = 0x10 (%02x)", joy_button);
    press(KEY_IDX_FIRE); read_keyboard_dirs();
    CHECK(joy_button == 0x00, "fuego pulsado: joy_button = 0 (%02x)", joy_button);

    /* ---- process_keyboard */
    #define SETUP() do { release_all(); keyboard_counter = 10; keyboard_prev = 10; pause_counter = 0; pause_calls = silence_calls = 0; \
        lives_count = 3; show_attract = restart_game = quit_requested = false; sound_enabled = 0xff; } while (0)
    /* sin pulsacion nueva no hace nada, aunque Esc/Ctrl+tecla esten pulsadas */
    SETUP(); press(KEY_IDX_ESC); press(KEY_IDX_PAUSE); press(KEY_IDX_RESTART);
    process_keyboard();
    CHECK(!pause_calls && !restart_game, "sin pulsacion nueva: nada");
    /* Esc -> pausa, una vez por pulsacion; keyboard_prev se actualiza */
    SETUP(); press(KEY_IDX_ESC); keyboard_counter = 11;
    process_keyboard();
    CHECK(pause_calls == 1 && keyboard_prev == 11, "Esc: show_pause_menu (%d), prev=%u", pause_calls, keyboard_prev);
    process_keyboard();
    CHECK(pause_calls == 1, "misma pulsacion: no repite");
    /* la pulsacion que cierra la pausa (pause_counter == keyboard_counter) no vuelve a pausar */
    SETUP(); press(KEY_IDX_ESC); keyboard_counter = 12; pause_counter = 12;
    process_keyboard();
    CHECK(pause_calls == 0 && keyboard_prev == 12, "Esc con pause_counter == keyboard_counter: no pausa");
    /* Esc pulsada tiene prioridad: no mira Ctrl */
    SETUP(); press(KEY_IDX_ESC); press(KEY_IDX_PAUSE); press(KEY_IDX_RESTART); keyboard_counter = 11;
    process_keyboard();
    CHECK(pause_calls == 1 && !restart_game, "Esc + Ctrl+R: solo pausa");

    /* sin Ctrl, ninguna de S/R/M/9/Y hace nada (el port viejo reaccionaba a S, R y M solas) */
    const int cmd[5] = { KEY_IDX_CHEAT, KEY_IDX_QUIT, KEY_IDX_DEMO, KEY_IDX_RESTART, KEY_IDX_SOUND };
    for (int k = 0; k < 5; k++) {
        SETUP(); press(cmd[k]); keyboard_counter = 11;
        process_keyboard();
        CHECK(lives_count == 3 && !quit_requested && !show_attract && !restart_game && sound_enabled == 0xff && !pause_calls && !silence_calls,
              "tecla %d sin Ctrl: no hace nada", cmd[k]);
    }
    /* Ctrl sola no hace nada */
    SETUP(); press(KEY_IDX_PAUSE); keyboard_counter = 11; process_keyboard();
    CHECK(lives_count == 3 && !quit_requested && !show_attract && !restart_game && sound_enabled == 0xff, "Ctrl sola: nada");
    /* con Ctrl, cada comando */
    SETUP(); press(KEY_IDX_PAUSE); press(KEY_IDX_CHEAT); keyboard_counter = 11; process_keyboard();
    CHECK(lives_count == 9, "Ctrl+9: lives_count = 9 (%d)", lives_count);
    SETUP(); press(KEY_IDX_PAUSE); press(KEY_IDX_QUIT); keyboard_counter = 11; process_keyboard();
    CHECK(quit_requested && !show_attract && !restart_game, "Ctrl+Y: quit_requested");
    SETUP(); press(KEY_IDX_PAUSE); press(KEY_IDX_DEMO); keyboard_counter = 11; process_keyboard();
    CHECK(show_attract && !restart_game && !quit_requested, "Ctrl+M: show_attract");
    SETUP(); press(KEY_IDX_PAUSE); press(KEY_IDX_RESTART); keyboard_counter = 11; process_keyboard();
    CHECK(restart_game && !show_attract, "Ctrl+R: restart_game");
    SETUP(); press(KEY_IDX_PAUSE); press(KEY_IDX_SOUND); keyboard_counter = 11; process_keyboard();
    CHECK(sound_enabled == 0x00 && silence_calls == 1, "Ctrl+S: sonido off (%02x) y silencia (%d)", sound_enabled, silence_calls);
    keyboard_counter = 12; process_keyboard();
    CHECK(sound_enabled == 0xff && silence_calls == 1, "Ctrl+S otra vez: sonido on (0xff, no 1) y sin silenciar");
    /* prioridad con varias: 9 > Y > M > R > S */
    SETUP(); press(KEY_IDX_PAUSE); for (int k = 0; k < 5; k++) press(cmd[k]); keyboard_counter = 11; process_keyboard();
    CHECK(lives_count == 9 && !quit_requested && !show_attract && !restart_game && sound_enabled == 0xff, "9 gana a todas");
    SETUP(); press(KEY_IDX_PAUSE); for (int k = 1; k < 5; k++) press(cmd[k]); keyboard_counter = 11; process_keyboard();
    CHECK(quit_requested && !show_attract && !restart_game && sound_enabled == 0xff, "Y gana a M/R/S");
    SETUP(); press(KEY_IDX_PAUSE); for (int k = 2; k < 5; k++) press(cmd[k]); keyboard_counter = 11; process_keyboard();
    CHECK(show_attract && !restart_game && sound_enabled == 0xff, "M gana a R/S");
    SETUP(); press(KEY_IDX_PAUSE); press(KEY_IDX_RESTART); press(KEY_IDX_SOUND); keyboard_counter = 11; process_keyboard();
    CHECK(restart_game && sound_enabled == 0xff, "R gana a S");

    /* la tabla de DS 0x6a1: los indices usados coinciden con los scancodes XT esperados */
    {
        extern const uint8_t ds_pool[];
        struct { int idx; uint8_t xt; } t[] = { {KEY_IDX_FIRE,0x38},{KEY_IDX_UP,0x48},{KEY_IDX_RIGHT,0x4d},{KEY_IDX_DOWN,0x50},{KEY_IDX_LEFT,0x4b},
            {KEY_IDX_MOD1,0x49},{KEY_IDX_MOD2,0x51},{KEY_IDX_MOD3,0x4f},{KEY_IDX_MOD4,0x47},{KEY_IDX_ESC,0x01},{KEY_IDX_QUIT,0x15},
            {KEY_IDX_SOUND,0x1f},{KEY_IDX_RESTART,0x13},{KEY_IDX_PAUSE,0x1d},{KEY_IDX_DEMO,0x32},{KEY_IDX_CHEAT,0x0a} };
        for (unsigned i = 0; i < sizeof t / sizeof *t; i++)
            CHECK(ds_pool[0x6a1 + t[i].idx] == t[i].xt, "KEY_IDX %d: scancode %02x != %02x", t[i].idx, ds_pool[0x6a1 + t[i].idx], t[i].xt);
    }

    if (!fails) printf("test_keys: OK\n");
    return fails;
}
