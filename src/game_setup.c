#include "cat_state.h"
#include "alley.h"
#include "cga.h"
#include "game_setup.h"
#include "gen/cat_gap1_sprites.h"
#include "level_background.h"
#include <stdint.h>

/* save_alley_buffer / reset_window_state — the original calls these to
 * snapshot the background under the cat and reset window-animation state.
 * Neither is ported yet (alley.asm's background-drawing pipeline and the
 * window state machine haven't been tackled), so these are faithful no-op
 * stubs for now — called in the right places so the control flow this
 * file ports stays structurally identical to the original, ready to be
 * filled in once that pipeline exists. */
/* save_alley_buffer is real now — src/alley.c, PROGRESS.md §6f. */
static void reset_window_state(void) { /* TODO: not yet ported */ }

/* setup_alley — literal port of game_loop.asm's setup_alley (lines 1-40).
 * Positions the cat at the left or right edge based on its current cat_x,
 * and resets essentially all per-scene game state flags. */
void setup_alley(void) {
    int16_t cx;
    int8_t  ah;

    if ((uint16_t)cat_x < 0xa0) {
        cx = 0x0;
        ah = 0x1;
    } else {
        cx = 0x128;
        ah = -1; /* 0xff */
    }

    scroll_direction = ah;
    entry_steps = 0x3;
    entry_delay = 0xc;
    uint8_t dl = 0xb4;
    cat_x = cx;
    cat_y = dl;
    cat_y_bottom = 0xe6;

    cat_draw_pos = (uint16_t)calc_cga_addr(cat_y, (uint16_t)cat_x, NULL);
    /* original also writes the same computed value to a second location
     * (word [0x561]) via a hardcoded literal address — this is
     * cat_screen_pos or an equivalent alias not yet given its own name in
     * our port; skipped since nothing currently reads it under that name. */

    save_alley_buffer();

    in_level_mode = 0;
    scroll_speed = 0x2;
    anim_counter = 0x1;
    transition_timer = 0;
    game_mode = 0;
    at_platform = 0;
    transitioning = 0;
    sprite_hidden = 0;
    input_horizontal = 0;
    input_vertical = 0;
    cat_died = 0;
    auto_walk = 0;
    object_hit = 0;
    level_complete = 0;
    cat_caught = 0;
    door_contact = 0;

    reset_window_state();
}

/* setup_level — literal port of game_loop.asm's setup_level (lines 47-107).
 * Positions the cat for the given level_number (from saved_cat_x/y if
 * level 0, else from level_start_x_table/level_start_y_table), sets up
 * the vertical-entry sprite (reusing gap1's enter_sprite[3] — see
 * PROGRESS.md §5d/§5e for how that pointer was resolved), and resets the
 * same per-scene flags as setup_alley, plus a few level-2-specific
 * extras. */
void setup_level(void) {
    int16_t cx;
    uint8_t dl;

    /* draw_level_background draws the room's border/platforms/doors —
     * see level_background.c and PROGRESS.md §5w/§5x. CONFIRMED against
     * entry.asm (the real call site, now available — score.asm's
     * dispatcher itself has no callers): every one of entry.asm's 7
     * per-level branches (levels 1-7) calls draw_level_background right
     * after level_transition, before its own setup_level/setup_level-
     * equivalent call — so calling it here, at the top of setup_level,
     * matches the original exactly. It's still never called for
     * level_number==0 (the alley uses setup_alley, a separate path) —
     * draw_level_background's own level_number<=0 guard covers that if
     * setup_level is ever accidentally invoked for the alley. */
    draw_level_background();

    if (level_number == 0) {
        cx = saved_cat_x;
        dl = saved_cat_y;
    } else {
        /* original: bx=level_number (word index), dl=[bx+level_start_y_table]
         * (byte table), then bx<<=1 for the word table lookup — our C
         * arrays are typed so plain indexing handles both correctly. */
        int idx = (int)level_number;
        dl = level_start_y_table[idx];
        cx = level_start_x_table[idx];
    }

    cat_x = cx;
    cat_y = dl;
    cat_y_bottom = (uint8_t)(dl + 0x32);

    cat_draw_pos = (uint16_t)calc_cga_addr(cat_y, (uint16_t)cat_x, NULL);

    /* the "entering the level" sprite pose — verified in §5d to be
     * enter_sprite[3] (DS 0xa7c, dims 3w x 13h) */
    vert_sprite = &enter_sprite[3];

    save_alley_buffer();

    in_level_mode = 1;
    scroll_direction = 0;
    anim_counter = 0x1;
    anim_step = 0x40;

    uint8_t transition_val = 0xa;
    if (level_number == 7) transition_val = 0;
    transition_timer = transition_val;

    game_mode = 0;
    at_platform = 0;
    transitioning = 0;
    sprite_hidden = 0;
    input_horizontal = 0;
    input_vertical = 0;
    cat_died = 0;
    auto_walk = 0;
    object_hit = 0;
    level_complete = 0;
    cat_caught = 0;
    door_contact = 0;

    reset_window_state();

    if (level_number == 2) {
        anim_counter = 0x10;
        speed_ramp = 0x10;
        /* original reads the BIOS tick count (int 0x1a) here to seed
         * level2_tick with real elapsed-time entropy; we substitute 0
         * since there's no BIOS timer to read — level2_tick isn't
         * consumed anywhere yet (see movement.c's footstep-sync stub). */
        level2_tick = 0;
        immune_flag = 0;
        level2_rise = 0x5;
        meow_timer = 0x1;
    }
}
