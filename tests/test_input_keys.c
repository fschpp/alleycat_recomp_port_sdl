/* T61 — src/input.c (SDL -> matriz de teclas, el papel de la ISR de INT 9). Sin libsdl2-dev: se incluye input.c con el SDL
 * falso de tests/fake_sdl. */
#include <stdio.h>
#include <string.h>
#include "../src/input.c"
#include "cat_state.h"
#include "gen/ds_pool.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint8_t fake_keys[SDL_NUM_SCANCODES];
const uint8_t *SDL_GetKeyboardState(int *n) { if (n) *n = SDL_NUM_SCANCODES; return fake_keys; }
static int pause_calls;
void __wrap_show_pause_menu(void) { pause_calls++; }
static int silence_calls;
void __wrap_silence_speaker(void) { silence_calls++; }

static void reset(void) {
    memset(fake_keys, 0, sizeof fake_keys);
    input_poll();                                  /* suelta todo en la matriz (estado interno de sync) */
    init_bios_data(); keyboard_prev = keyboard_counter = 0; pause_counter = 0; pause_calls = silence_calls = 0;
    show_attract = restart_game = quit_requested = reboot_requested = false; sound_enabled = 0xff; lives_count = 3;
}
static void key(SDL_Scancode sc, int down) { fake_keys[sc] = (uint8_t)down; }

int main(void) {
    rom_id = 0xff;
    /* tabla: orden y scancodes XT = los de DS 0x6a1, sin repetidos */
    for (int i = 0; i < KEY_MATRIX_SIZE; i++) {
        CHECK(key_bindings[i].xt == ds_pool[0x6a1 + i], "binding %d: xt %02x != ds_pool %02x", i, key_bindings[i].xt, ds_pool[0x6a1 + i]);
        CHECK(key_bindings[i].a != SDL_SCANCODE_UNKNOWN, "binding %d sin tecla", i);
        for (int j = 0; j < i; j++) CHECK(key_bindings[i].a != key_bindings[j].a && key_bindings[i].a != key_bindings[j].b, "SDL repetida %d/%d", i, j);
    }

    /* cada tecla (y su version alternativa): pulsar = matriz 0 y +1 al contador; soltar = 0x80 sin contar; sin repeticion */
    for (int i = 0; i < KEY_MATRIX_SIZE; i++)
    for (int v = 0; v < 2; v++) {
        SDL_Scancode sc = v ? key_bindings[i].b : key_bindings[i].a;
        if (sc == SDL_SCANCODE_UNKNOWN) continue;
        reset(); uint16_t c0 = keyboard_counter;
        key(sc, 1); input_poll();
        CHECK(key_matrix[i] == 0x00 && keyboard_counter == (uint16_t)(c0 + 1), "tecla %d/%d: pulsar (m=%02x, contador +%d)", i, v, key_matrix[i], keyboard_counter - c0);
        input_poll(); input_process_keys();
        CHECK(keyboard_counter == (uint16_t)(c0 + 1), "tecla %d/%d: mantener no cuenta de nuevo", i, v);
        key(sc, 0); input_poll();
        CHECK(key_matrix[i] == 0x80 && keyboard_counter == (uint16_t)(c0 + 1), "tecla %d/%d: soltar (m=%02x, contador +%d)", i, v, key_matrix[i], keyboard_counter - c0);
        for (int j = 0; j < KEY_MATRIX_SIZE; j++) if (j != i) CHECK(key_matrix[j] == 0x80, "tecla %d/%d: toco la entrada %d", i, v, j);
    }

    /* direcciones (todas, incluidas las diagonales de la tabla key_mod) */
    struct { SDL_Scancode k[2]; int h, v; const char *n; } dirs[] = {
        { { SDL_SCANCODE_LEFT,  0 }, -1,  0, "izq" }, { { SDL_SCANCODE_RIGHT, 0 }, 1, 0, "der" },
        { { SDL_SCANCODE_UP, 0 }, 0, -1, "arriba" }, { { SDL_SCANCODE_DOWN, 0 }, 0, 1, "abajo" },
        { { SDL_SCANCODE_PAGEUP, 0 }, 1, -1, "RePag (arriba-der)" }, { { SDL_SCANCODE_PAGEDOWN, 0 }, 1, 1, "AvPag (abajo-der)" },
        { { SDL_SCANCODE_END, 0 }, -1, 1, "Fin (abajo-izq)" }, { { SDL_SCANCODE_HOME, 0 }, -1, -1, "Inicio (arriba-izq)" },
        { { SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT }, -1, 0, "izq+der: gana izq" },
        { { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN }, 0, -1, "arriba+abajo: gana arriba" },
    };
    for (unsigned d = 0; d < sizeof dirs / sizeof *dirs; d++) {
        reset(); key(dirs[d].k[0], 1); if (dirs[d].k[1]) key(dirs[d].k[1], 1);
        input_poll();
        CHECK(input_horizontal == dirs[d].h && input_vertical == dirs[d].v, "%s: (h=%d,v=%d) esperado (%d,%d)", dirs[d].n, input_horizontal, input_vertical, dirs[d].h, dirs[d].v);
        memset(fake_keys, 0, sizeof fake_keys); input_poll();
        CHECK(input_horizontal == 0 && input_vertical == 0, "%s: soltar deja (0,0)", dirs[d].n);
    }

    /* fuego: Alt izquierda o derecha -> joy_button 0 / input_fire (lo que leen los niveles 4, 6 y 7) */
    reset(); input_poll();
    CHECK(joy_button == 0x10 && !input_fire, "sin Alt: joy_button 0x10 (%02x)", joy_button);
    key(SDL_SCANCODE_RALT, 1); input_poll();
    CHECK(joy_button == 0x00 && input_fire, "Alt: joy_button 0 (%02x)", joy_button);
    key(SDL_SCANCODE_RALT, 0); input_poll();
    CHECK(joy_button == 0x10 && !input_fire, "soltar Alt: joy_button vuelve a 0x10 (el port no lo refrescaba nunca)");

    /* Ctrl+Alt+Del y Ctrl+Alt+Der/Izq siguen pasando por check_special_keys en cada cambio */
    reset(); key(SDL_SCANCODE_LCTRL, 1); key(SDL_SCANCODE_LALT, 1); input_poll();
    CHECK(!reboot_requested, "Ctrl+Alt sin Del: no reinicia");
    key(SDL_SCANCODE_DELETE, 1); input_poll();
    CHECK(reboot_requested, "Ctrl+Alt+Del: reboot_requested");
    reset(); video_mode = 4; key(SDL_SCANCODE_LCTRL, 1); key(SDL_SCANCODE_LALT, 1); key(SDL_SCANCODE_RIGHT, 1); input_poll();
    CHECK(video_mode == 3, "Ctrl+Alt+Der: video_mode 4 -> 3 (%d)", video_mode);

    /* teclas de comando via input_process_keys */
    reset(); key(SDL_SCANCODE_S, 1); input_process_keys();
    CHECK(sound_enabled == 0xff, "S sola: no cambia el sonido");
    key(SDL_SCANCODE_S, 0); input_process_keys();
    key(SDL_SCANCODE_RCTRL, 1); input_process_keys();
    key(SDL_SCANCODE_S, 1); input_process_keys();
    CHECK(sound_enabled == 0x00 && silence_calls == 1, "Ctrl+S: sonido off y silencia (%02x, %d)", sound_enabled, silence_calls);
    input_process_keys(); input_process_keys();
    CHECK(sound_enabled == 0x00, "Ctrl+S mantenida: una sola vez");
    key(SDL_SCANCODE_S, 0); input_process_keys(); key(SDL_SCANCODE_S, 1); input_process_keys();
    CHECK(sound_enabled == 0xff, "Ctrl+S otra vez: sonido on");
    reset(); key(SDL_SCANCODE_LCTRL, 1); input_process_keys(); key(SDL_SCANCODE_R, 1); input_process_keys();
    CHECK(restart_game && !show_attract, "Ctrl+R");
    reset(); key(SDL_SCANCODE_LCTRL, 1); input_process_keys(); key(SDL_SCANCODE_M, 1); input_process_keys();
    CHECK(show_attract && !restart_game, "Ctrl+M");
    reset(); key(SDL_SCANCODE_LCTRL, 1); input_process_keys(); key(SDL_SCANCODE_9, 1); input_process_keys();
    CHECK(lives_count == 9, "Ctrl+9");
    reset(); key(SDL_SCANCODE_Y, 1); input_process_keys();
    CHECK(!quit_requested, "Y sola: no sale");
    key(SDL_SCANCODE_LCTRL, 1); input_process_keys(); key(SDL_SCANCODE_Y, 0); input_process_keys(); key(SDL_SCANCODE_Y, 1); input_process_keys();
    CHECK(quit_requested, "Ctrl+Y: quit_requested");
    /* Esc = pausa, una vez por pulsacion (ya no es salir, ni existe la tecla P) */
    reset(); key(SDL_SCANCODE_ESCAPE, 1); input_process_keys();
    CHECK(pause_calls == 1, "Esc: show_pause_menu (%d)", pause_calls);
    input_process_keys(); CHECK(pause_calls == 1, "Esc mantenida: no repite");

    if (!fails) printf("test_input_keys: OK\n");
    return fails;
}
