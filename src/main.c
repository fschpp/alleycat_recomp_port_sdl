#include <SDL2/SDL.h>
#include <stdio.h>
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

    printf("Alley Cat C/SDL port - entry.asm flow (T40/T41).\n");
    printf("Arrow keys walk the cat; S toggles sound; R restarts; ESC quits.\n");
    printf("%s\n", have_audio ? "Audio device opened OK." : "NO audio device - running silent.");

    /* entry.asm L27-143: init, title (T52 stub), attract (T54 stub), new game, alley setup.
     * Returns where lab_0155 (the alley loop) begins. */
    game_start();

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                running = false;
        }
        if (!running) break;

        input_poll();                              /* read_keyboard_dirs (port: SDL) */
        gf_next_t next = game_alley_frame();       /* one pass of lab_0155 (entry.asm L144-212) */

        video_present();
        SDL_Delay(33); /* ~30Hz: the original has a closed loop with no delay */

        switch (next) {
        case GF_STAY:   break;
        case GF_TO_0081: game_flow_run(GF_LAB_0081); break;   /* game over -> title */
        case GF_TO_00A3: game_flow_run(GF_LAB_00A3); break;   /* attract timeout */
        case GF_TO_00AE: game_flow_run(GF_LAB_00AE); break;   /* restart */
        case GF_TO_0238:
            /* The cat died: game_death_handler() already saved the position and picked
             * level_number. TODO(T42/T43/T75): lab_0238 dispatches the level loop here.
             * Until then behave as if the level ended at once (entry.asm L435:
             * start_in_level = 0 -> respawn in the alley). */
            start_in_level = 0;
            game_flow_run(GF_LAB_00F3);
            break;
        }
    }

    silence_speaker();
    audio_shutdown();
    video_shutdown();
    return 0;
}
