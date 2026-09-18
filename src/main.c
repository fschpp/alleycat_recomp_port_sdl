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

/* Background fill used to visualize the cat's silhouette clearly (see
 * PROGRESS.md §5b: draw_alley_foreground's blit_masked is AND-only, so
 * the cat only ever appears as a black silhouette against whatever's
 * behind it — a pure-black background would make it invisible). */
#define DEMO_BG_BYTE 0x55 /* color index 1 (cyan), repeated 4x per byte */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    cga_init();
    input_init();
    if (!video_init(3)) {
        fprintf(stderr, "video_init failed\n");
        return 1;
    }

    printf("Alley Cat C/SDL port - real general alley-movement pipeline.\n");
    printf("Arrow keys move the cat left/right along the alley. On levels\n");
    printf("1-6, climbing is now triggered by REAL level geometry (see\n");
    printf("PROGRESS.md Sec 5n) -- walk the cat over a gap between\n");
    printf("platforms and it'll fall/climb through automatically, just\n");
    printf("like the original. This demo starts on level_number=0 (the\n");
    printf("alley), where the climb trigger isn't ported yet; edit main.c's\n");
    printf("level_number to try 1-6 and see the geometry-gated behavior.\n");
    printf("A dog enemy (real AI + real sprite, see PROGRESS.md Sec 5o/5p)\n");
    printf("will randomly appear near ground level and chase the cat.\n");
    printf("3 patrol rats/mice (real AI, see PROGRESS.md Sec 5s) roam the\n");
    printf("alley floor and knock the cat back on contact.\n");
    printf("Score/lives HUD is drawn top area of screen (real BCD scoring,\n");
    printf("see PROGRESS.md Sec 5v) -- score stays 0 in this demo since no\n");
    printf("gameplay event currently calls add_score() yet.\n");

    lives_count = 3;
    clear_score();
    clear_high_score();

    cat_x = 100; /* pre-set so setup_alley()'s left/right-edge choice is deterministic for the demo */
    difficulty_level = 5; /* higher than default so the dog's spawn chance is visible within a short demo */
    level_number = 3;
    setup_alley();
    /* entry.asm's lab_0140 calls init_objects() right after setup_alley/
     * setup_level, alongside init_player/reset_jump/init_sound — see
     * PROGRESS.md §5s. init_player/reset_jump aren't ported yet (they
     * belong to the still-unported fish-jump/falling-object systems). */
    init_cycle_objects();
    /* entry.asm's per-level setup also calls init_thrown_objects()
     * unconditionally (levels 0-6 all use the general thrown-object
     * rain, gated internally by its own per-level rate table) — see
     * src/level_objects.c. */
    init_thrown_objects();
    /* entry.asm's level-3 loop (lab_0394) also calls init_level3_enemy()
     * once on entry — see src/level3_enemy.c and PROGRESS.md. There's no
     * real level-cycling state machine here yet (this demo just sets
     * level_number once at startup), so this only matters if that's 3. */
    if (level_number == 3) init_level3_enemy();
    /* entry.asm's level-7 loop also calls init_level7_objects() once on
     * entry — see src/level7_epilogue.c. Partial port (chunks 1-2 of the
     * epilogue subsystem, check_l7_cupid/check_l7_object_overlap and the
     * victory-wave/music still to come); see PROGRESS.md. */
    if (level_number == 7) init_level7_objects();

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                running = false;
        }

        input_poll();
        input_process_keys();
        immune_flag = 0;

        if (restart_game) {
            cat_x = 100;
            setup_alley();
            init_cycle_objects();
            init_thrown_objects();
            if (level_number == 3) init_level3_enemy();
            if (level_number == 7) init_level7_objects();
            restart_game = false;
        }

        memset(cga_mem, DEMO_BG_BYTE, sizeof(cga_mem));

        /* The real ported per-frame dispatch: direction assignment, scroll,
         * walk-cycle frame selection, and the draw call, all in one —
         * see src/alley_movement.c for the full breakdown. */
        update_alley_movement();

        /* Dog enemy AI (spawn/approach/chase/exit state machine) plus
         * real sprite rendering — see src/enemy.c and PROGRESS.md §5o/§5p. */
        update_enemies();

        /* 3 patrol objects (rats/mice) — real AI, real sprites, real
         * cat-collision knockback. See src/cycle_objects.c and
         * PROGRESS.md §5s. */
        update_cycle_objects();

        /* General thrown-object rain (levels 0-6) — see
         * src/level_objects.c and PROGRESS.md. */
        tick_thrown_objects();

        /* Level-3 bird enemy (fence dive-bomber) — real AI, real sprite,
         * real hitboxes against both the cat and thrown objects. See
         * src/level3_enemy.c and PROGRESS.md. Only active when
         * level_number is 3 (set this demo's level_number to 3 to see
         * it); its door/fence subsystem (init/update_level3_doors) is a
         * separate, still-unported piece of level_objects.asm. */
        if (level_number == 3) update_level3_enemy();

        /* Level-7 victory epilogue "heart cats" — partial port (chunks
         * 1-2), see src/level7_epilogue.c and PROGRESS.md. */
        if (level_number == 7) update_level7_objects();

        /* Fish-jump/gravity-toss enemy — a creature that leaps from a
         * ledge, pauses near a patrol object, then throws a falling
         * projectile at the cat. See src/jump_gravity.c and PROGRESS.md
         * §5t. The fish's own jump-arc sprite isn't extracted yet (only
         * the thrown projectile's gravity_sprite frames are), so only
         * the projectile is currently visible once tossed. */
        update_cat_jump();
        apply_cat_gravity();

        /* Falling-object dodge mechanic (bottles/pots dropped from
         * windows above certain door positions) — see src/fall_object.c
         * and PROGRESS.md §5u. */
        animate_falling();

        /* Score/lives HUD — see src/score.c and PROGRESS.md §5v.
         * draw_lives() only redraws when lives_count changed since its
         * last call, which — combined with this demo clearing the whole
         * screen every frame (a simplification noted since §5h/§5m, no
         * real background persistence yet) — would make the lives digit
         * disappear after one frame. Force it by resetting the cache
         * each frame; not needed once real background persistence
         * exists. */
        lives_display = 0xff;
        draw_lives();
        draw_current_score();
        draw_high_score_display();

        video_present();

        SDL_Delay(33); /* ~30Hz: fast enough to see the walk cycle, slow enough to watch it */
    }

    video_shutdown();
    return 0;
}
