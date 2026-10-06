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

/* T45: el barrido de level_transition bloquea (como el original); presentar cada paso para que se vea. */
static void wipe_present_step(void) {
    SDL_PumpEvents();
    video_present();
    SDL_Delay(25);
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
        if (ev.type == SDL_KEYDOWN && !ev.key.repeat) keyboard_counter++;
    }
    video_present();
    SDL_Delay(10);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    cga_init();
    input_init();
    if (!video_init(3)) {
        fprintf(stderr, "video_init failed\n");
        return 1;
    }

    /* entry.asm opens with sound on and the music engine reset; the audio
     * device is this port's stand-in for "the speaker exists". If no device
     * can be opened the game just runs silent. */
    bool have_audio = audio_init();
    wipe_step_hook = wipe_present_step;
    ui_wait_hook = ui_wait_pump;

    printf("Alley Cat C/SDL port - entry.asm flow (T40/T41).\n");
    printf("Arrow keys walk the cat; S toggles sound; R restarts; ESC quits.\n");
    printf("%s\n", have_audio ? "Audio device opened OK." : "NO audio device - running silent.");

    /* entry.asm L27-143: init, title (T52 stub), attract (T54 stub), new game, alley setup.
     * Returns where lab_0155 (the alley loop) begins. */
    game_start();

    bool running = true;
    bool in_level = false;                         /* true while a level loop (T42) runs instead of the alley */
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                running = false;
        }
        if (!running) break;

        input_poll();                              /* read_keyboard_dirs (port: SDL) */
        gf_next_t next;
        if (in_level) {
            /* one pass of the level loop (entry.asm lab_027e/lab_02c5/lab_0319, T42) */
            next = (game_level_frame() == GL_EXIT) ? game_level_exit() : GF_STAY;
        } else {
            next = game_alley_frame();             /* one pass of lab_0155 (entry.asm L144-212) */
        }

        video_present();
        SDL_Delay(33); /* ~30Hz: the original has a closed loop with no delay */

        switch (next) {
        case GF_STAY:   break;
        case GF_TO_0081: in_level = false; game_flow_run(GF_LAB_0081); break;   /* game over -> title */
        case GF_TO_00A3: in_level = false; game_flow_run(GF_LAB_00A3); break;   /* attract timeout */
        case GF_TO_00AE: in_level = false; game_flow_run(GF_LAB_00AE); break;   /* restart */
        case GF_TO_00F3: in_level = false; game_flow_run(GF_LAB_00F3); break;   /* level exit -> alley (lab_0427) */
        case GF_TO_0238:
            /* The cat died: game_death_handler() already picked level_number; lab_0238 dispatches to
             * the init of that level (T42/T43: every level is ported) and its loop runs from now on. */
            in_level = game_level_enter();
            break;
        }
    }

    silence_speaker();
    audio_shutdown();
    video_shutdown();
    return 0;
}
