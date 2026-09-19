#include "cat_state.h"
#include "alley.h"
#include "sound.h"
#include "cga.h"
#include "movement.h"
#include "alley_movement.h"
#include "gen/cat_gap1_sprites.h"
#include "level_collision.h"
#include "level_objects.h"
#include "enemy.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* --- stubs for subroutines not yet ported (documented, called in the
 * right places so control flow stays structurally faithful) --- */

/* update_footprint (level_objects.asm) — now a real port, see
 * src/level_objects.c / PROGRESS.md. */

/* spawn_window_event / restore_alley_buffer / draw_alley_foreground are
 * real now — see src/alley.c and PROGRESS.md §6f. The "window-animation
 * state machine" this TODO waited on turned out to be 36 lines of
 * alley.asm, not a separate subsystem. */

/* check_dog_collision (enemy.asm) — literal name is misleading; per
 * PROGRESS.md §5o this is actually level-0-specific gravity-fall-landing
 * detection (uses gravity_sprite_ptrs), not literal cat-touches-dog
 * collision. Still genuinely unported (needs level 0's jump/landing
 * state), so this stays a documented "no collision" stub — NOT the same
 * function as enemy.c's real dog AI (update_enemies/check_enemy_activate),
 * which IS ported and wired in below. */
static bool check_dog_collision(void) { return false; }

/* check_enemy_activate — FIX: this used to be a local stub here that
 * unconditionally returned false, silently shadowing the real, ported
 * implementation in enemy.c (declared in enemy.h, wired into main.c's
 * update_enemies() call already). That meant the dog could never trigger
 * its "activate chase" transition via this code path — only via
 * update_enemies()'s own internal call to it. Removed the shadow; this
 * file now uses enemy.h's real check_enemy_activate() directly. */

/* The g_last_frame/g_last_draw_pos "keep redrawing the last frame" demo
 * hack that used to live here is GONE (§6f): with a real background
 * save/restore pipeline and a screen that is no longer wiped every frame,
 * an idle cat simply stays on screen because nothing erases it — exactly
 * like the original. */

/* play_catch_sound comes from the real sound.asm port (src/sound.c); it
 * also clears door_contact itself, exactly as the original does. */

/* select_vertical_sprite — table lookup for game_loop.asm's
 * climb_sprite_ptrs/climb_sprite_dims, indexed exactly as lab_0f14 does.
 * IMPORTANT: the original's climb_sprite_ptrs/climb_sprite_dims tables
 * are only 1 entry "wide" by their own label, but the code indexes up to
 * entry 5 — verified (PROGRESS.md §5m) that this is a DELIBERATE memory
 * overlap: entries [1..5] are byte-identical to enter_sprite_data[0..4]/
 * enter_sprite_dims[0..4] (already extracted in §5d's gap-1 work). So
 * this table is expressed directly in terms of the sprites we already
 * have, rather than needing a new extraction. */
static const cat_walk_frame_t *select_vertical_sprite(uint8_t bx_word_index) {
    switch (bx_word_index) {
        case 0: return &climb_sprite;
        case 1: return &enter_sprite[0];
        case 2: return &enter_sprite[1];
        case 3: return &enter_sprite[2];
        case 4: return &enter_sprite[3];
        case 5: return &enter_sprite[4];
        default: return &climb_sprite; /* shouldn't happen; safe fallback */
    }
}

/* update_climb_transition — literal port of game_loop.asm's lab_0e91
 * through lab_0f33: once in_level_mode is known non-zero (i.e. the cat is
 * climbing, or transitioning back out of a climb toward the alley), this
 * picks the right transition pose and sets up the climb animation timers.
 *
 * NOT PORTED (needs level_objects.asm geometry, not available): the
 * *first* transition into climbing — game_loop.asm's lab_0e23, above
 * lab_0e78, which calls check_level_collision to detect a ladder/pole at
 * the cat's position before ever setting in_level_mode. This port still
 * derives in_level_mode straight from input_vertical (see
 * update_alley_movement) rather than gating it on real level geometry —
 * a cat can "climb" anywhere in this demo, not just at a ladder. */
static void update_climb_transition(void) {
    uint8_t al, ah;

    if (in_level_mode == 1) {
        if ((uint8_t)cat_y < 0xb4) {
            /* lab_0eb1: still descending into the level */
            ah = 1; al = 0x20;
            transition_timer = 8;
            if (game_mode == 1) game_mode = 0;
            goto lab_0ef1;
        }
        /* reached the bottom (cat_y >= 0xb4): snap back out to the alley */
        in_level_mode = 0;
        auto_walk = 0;
        input_vertical = 0;
        return; /* caller (update_alley_movement) handles the plain-alley draw */
    }

    /* in_level_mode == -1: climbing back up/out toward the alley */
    {
        transition_timer = 0;
        uint16_t speed = scroll_speed;
        uint8_t bl = (uint8_t)speed;
        uint8_t adjusted = (bl <= 2) ? bl : (uint8_t)(bl - 2);
        scroll_speed = (uint16_t)((speed & 0xFF00) | adjusted);

        ah = 8;
        al = bl; /* original AL, before the -2 adjustment */
        al ^= 0x0f;
        al = (uint8_t)(al << 4);
        if (game_mode == 1) game_mode = 2;
    }

lab_0ef1:
    anim_step = al;
    anim_counter = ah;
    anim_accumulator = 1;
    at_platform = 0;

    uint8_t bl = (uint8_t)(scroll_direction + 1);
    bl = (uint8_t)(bl << 1);
    if (in_level_mode != -1) bl = (uint8_t)(bl + 6);

    const cat_walk_frame_t *frame = select_vertical_sprite((uint8_t)(bl >> 1));
    vert_sprite = frame;

    l3_platform_id = 0;
    if (door_contact != 0) play_catch_sound();

    /* draw the selected transition pose immediately (the original defers
     * to a separate later draw_alley_foreground dispatch; this port
     * draws right away since that separate call chain isn't wired up
     * yet, prioritizing keeping the cat visible for this demo). */
    cat_draw_pos = (uint16_t)calc_cga_addr(cat_y, (uint16_t)cat_x, NULL);
    cat_sprite_ptr  = frame->data;
    cat_sprite_dims = (uint16_t)((frame->height << 8) | frame->width_words);
    draw_alley_foreground();
}

/* update_alley_movement — literal port of game_loop.asm's lab_0e23
 * through lab_0f86. This is the REAL general per-frame cat movement/
 * render dispatch — see PROGRESS.md §5g/§5h for how this was found,
 * correcting an earlier misattribution.
 *
 * NOT YET PORTED (faithfully skipped, not silently wrong): the level-0
 * and level-7 specific entry checks (`check_jump_collision`/level-0's
 * `check_door_position` semantics not yet disambiguated, and level-7's
 * `check_stairs_collision`, blocked on the unported window-state
 * machine — see PROGRESS.md §5n). For levels 1,2,3,4,5,6 the real
 * geometry-gated trigger IS wired in below via check_level_collision() —
 * see include/level_collision.h and PROGRESS.md §5n for an important
 * polarity correction this required: check_level_platform() returning
 * true means "solid platform found here" (stay on normal ground), and
 * it's the ABSENCE of a platform (a gap/hole in the geometry) that
 * triggers automatic climb-mode entry — backwards from the first-guess
 * intuition of "found a ladder -> climb." */
void update_alley_movement(void) {
    if (level_number != 0 && level_number != 7 && (uint8_t)cat_y < 0xb4) {
        if (!check_level_collision()) {
            /* no solid platform under the cat: fall/climb through the gap */
            scroll_direction = 0;
            in_level_mode = 1;
            update_climb_transition();
            return;
        }
    }

    /* lab_0e78 */
    prev_scroll_dir = scroll_direction;
    scroll_direction = input_horizontal;
    in_level_mode = input_vertical;

    if (in_level_mode != 0) {
        update_climb_transition();
        if (in_level_mode == 0) {
            /* update_climb_transition snapped us back to the alley
             * (cat_y reached the exit threshold) — fall through to the
             * plain-alley path below so we still draw this frame. */
        } else {
            return;
        }
    }

    /* lab_0f34 */
    if (level_number != 0 && level_number != 7) {
        update_footprint();
    }

    update_scroll();

    cat_screen_pos = (uint16_t)calc_cga_addr(cat_y, (uint16_t)cat_x, NULL);

    if (scroll_direction == 0 && in_level_mode == 0) {
        /* fully idle: no walk animation, just maybe pop a window event. */
        spawn_window_event();
        return;
    }

    /* lab_0f63 */
    const cat_walk_frame_t *frame = update_walk_frame();

    restore_alley_buffer();

    if (check_dog_collision()) return;
    if (check_enemy_activate()) return;

    cat_draw_pos    = cat_screen_pos;
    cat_sprite_ptr  = frame->data;
    cat_sprite_dims = (uint16_t)((frame->height << 8) | frame->width_words);
    draw_alley_foreground();
}
