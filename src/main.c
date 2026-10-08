#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cga.h"
#include "video.h"
#include "input.h"
#include "cat_state.h"
#include "game_setup.h"
#include "alley_movement.h"
#include "enemy.h"
#include "cycle_objects.h"
#include "jump_gravity.h"
#include "fall_object.h"
#include "score.h"
#include "level3_enemy.h"
#include "level_objects.h"
#include "level7_epilogue.h"
#include "audio.h"
#include "sound.h"
#include "alley_drawing.h"
#include "throw.h"
#include "game_flow.h"
#include "ui.h"
#include "hardware.h"

/* T45: el barrido de level_transition bloquea (como el original); presentar cada paso para que se vea. */
static void wipe_present_step(void) {
    SDL_PumpEvents();
    video_present();
    SDL_Delay(25);
}

/* T84: la cinematica del nivel 7 (corazones, marcha) bloquea y dibuja en cga_mem: hay que presentar durante sus esperas. */
static void l7_present_step(void) {
    SDL_PumpEvents();
    video_present();
}

/* T50: las esperas bloqueantes de ui.asm (wait_for_input) necesitan bombear SDL, contar pulsaciones (lo que hacia la
 * ISR de INT 9 con keyboard_counter) y presentar. */
static void ui_wait_pump(void) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            silence_speaker();
            audio_shutdown();
            video_shutdown();
            exit(0);
        }
        if ((ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) && !ev.key.repeat) {
            /* T54: la ISR de INT 9 fija ademas la matriz de teclas (0x00 pulsada / 0x80 suelta) para los scancodes de la
             * tabla de DS 0x6a1; show_attract_mode lee Y/N (joystick) y K/H/T/A (dificultad). Scancodes XT de esas teclas. */
            uint8_t xt = 0;
            switch (ev.key.keysym.scancode) {
                case SDL_SCANCODE_Y: xt = 0x15; break;
                case SDL_SCANCODE_N: xt = 0x31; break;
                case SDL_SCANCODE_K: xt = 0x25; break;
                case SDL_SCANCODE_H: xt = 0x23; break;
                case SDL_SCANCODE_T: xt = 0x14; break;
                case SDL_SCANCODE_A: xt = 0x1e; break;
                default: break;
            }
            if (xt) int9_set_scancode(xt, ev.type == SDL_KEYDOWN);
            if (ev.type == SDL_KEYDOWN) keyboard_counter++;   /* la ISR solo cuenta pulsaciones (bit 7 = 0) */
        }
    }
    video_present();
    SDL_Delay(10);
}

/* T74: eventos SDL + lectura de teclado una vez por frame, antes de la logica. false = salir. */
static bool frame_poll(void) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) return false;
    }
    if (!use_joystick) input_poll();           /* read_keyboard_dirs (port: SDL); con joystick lo pisaria poll_joystick (T60) */
    return true;
}

/* T74: presentar y retardar tras la logica. El original es un loop cerrado sin retardo; ~30 Hz aqui. */
static void frame_present(void) {
    video_present();
    SDL_Delay(33);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    cga_init();
    input_init();
    if (!video_init(3)) {
        fprintf(stderr, "video_init failed\n");
        return 1;
    }

    /* entry.asm opens with sound on and the music engine reset; the audio device is this port's stand-in for
     * "the speaker exists". If no device can be opened the game just runs silent. */
    if (!audio_init()) fprintf(stderr, "NO audio device - running silent.\n");
    wipe_step_hook = wipe_present_step;
    l7_step_hook = l7_present_step;
    ui_wait_hook = ui_wait_pump;
    game_poll_hook = frame_poll;
    game_present_hook = frame_present;

    game_run();

    silence_speaker();
    audio_shutdown();
    video_shutdown();
    return 0;
}
