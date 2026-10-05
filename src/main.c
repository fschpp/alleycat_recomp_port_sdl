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
    init_music();
    init_sound();

    printf("Alley Cat C/SDL port - alley loop (entry.asm L131-180, T19).\n");
    printf("Arrow keys walk the cat; S toggles sound; R restarts; ESC quits.\n");
    printf("%s\n", have_audio ? "Audio device opened OK." : "NO audio device - running silent.");

    /* entry.asm L95-100: nueva partida. */
    lives_count = 3;
    clear_score();
    clear_high_score();
    level_number = 0;

    bool running = true;
    while (running) {
        /* ---- lab_00f3: preparación del callejón (entry.asm L110-134) ---- */
        clear_screen();            /* fondo + detalles + edificios + ventanas + init_alley_objects */
        render_sprites();
        lives_display = 0xff;      /* force lives redraw */
        silence_speaker();
        level_number = 0;
        /* start_in_level == 0 → lab_0137 (la rama setup_level se usa al volver de un nivel: T41/T42) */
        cat_x = 0;                 /* start at left edge */
        setup_alley();
        /* lab_0140 */
        init_sound();
        init_player();
        reset_jump();
        init_cycle_objects();      /* init_objects */
        draw_high_score_display(); /* draw_score (nombres cruzados, ver tareas.md 0.5) */
        draw_current_score();      /* draw_high_score */
        init_music();
        restart_game = false;
        show_attract = false;

        /* ---- lab_0155: loop del callejón ---- */
        bool in_alley = true;
        while (running && in_alley) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) running = false;
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                    running = false;
            }
            if (!running) break;

            if (lives_count == 0) { running = false; break; }  /* lab_0081: game over (T41 lo reemplaza) */

            input_poll();
            input_process_keys();                /* process_keyboard */
            if (show_attract) { show_attract = false; continue; }  /* lab_00a3: attract (T54) */
            if (restart_game) break;             /* lab_00ae: reinicia el callejón */
            /* poll_joystick: opcional (T60) */
            immune_flag = 0;

            update_alley_movement();             /* update_animation (auditoría en T70-T73) */
            update_enemies();

            bool run_physics = true;
            if (enemy_active == 0) {             /* sin enemigo: física solo cada 4.º frame */
                frame_counter++;
                if (frame_counter & 0x3) run_physics = false;
            }

            if (run_physics) {
                play_sound();
                update_thrown_objects();
                update_cat_jump();
                apply_cat_gravity();
                animate_falling();
                update_cycle_objects();          /* cycle_animations */
                draw_lives();
                if (cat_died) {                  /* lab_01b7: selector de nivel / muerte → T41 */
                    if (lives_count == 0) { running = false; break; }
                    in_alley = false;            /* por ahora: reentra al callejón (TODO(T41)) */
                    cat_died = 0;
                }
            }

            video_present();
            SDL_Delay(33); /* ~30Hz */
        }
    }

    silence_speaker();
    audio_shutdown();
    video_shutdown();
    return 0;
}
