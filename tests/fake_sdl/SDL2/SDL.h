/* SDL falso minimo para compilar src/input.c en los tests sin libsdl2-dev (tests/test_input_keys.c).
 * Solo declara lo que usa input.c; si input.c usa algo mas, hay que anadirlo aqui. */
#ifndef FAKE_SDL_H
#define FAKE_SDL_H
#include <stdint.h>
#include <stddef.h>
typedef enum {
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A, SDL_SCANCODE_H, SDL_SCANCODE_K, SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_R, SDL_SCANCODE_S,
    SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_9, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_DELETE,
    SDL_SCANCODE_LALT, SDL_SCANCODE_RALT, SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL,
    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_PAGEUP, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_END, SDL_SCANCODE_HOME,
    SDL_NUM_SCANCODES = 512
} SDL_Scancode;
const uint8_t *SDL_GetKeyboardState(int *numkeys);
#endif
