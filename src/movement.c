#include "movement.h"
#include "cat_state.h"
#include "gen/cat_alley_walk_frames.h"
#include "gen/ds_pool.h"
#include <stdint.h>

static uint16_t mv_ds_word(unsigned off) { return (uint16_t)(ds_pool[off] | (ds_pool[off + 1] << 8)); }

/* update_scroll — literal port of alley.asm's update_scroll. */
bool update_scroll(void) {
    scroll_left_bound = 0x8;
    scroll_right_bound = 0x123;
    if (level_number == 7) {
        scroll_left_bound = 0x24;
        scroll_right_bound = 0x10f;
    }

    uint8_t sd = (uint8_t)scroll_direction;
    if (sd < 1) return false; /* scroll_direction == 0: no scroll, no bound hit */

    int32_t ax = (uint16_t)cat_x;
    if (sd != 1) {
        /* scroll_direction == 0xFF (left) */
        ax -= (int32_t)scroll_speed;
        if (ax < 0 || (uint16_t)ax < scroll_left_bound) {
            cat_x = (int16_t)scroll_left_bound;
            return true;
        }
        cat_x = (int16_t)ax;
        return false;
    } else {
        /* scroll_direction == 1 (right) */
        ax += (int32_t)scroll_speed;
        if ((uint16_t)ax >= scroll_right_bound) {
            cat_x = (int16_t)(scroll_right_bound - 1);
            return true;
        }
        cat_x = (int16_t)ax;
        return false;
    }
}

/* update_cat_dive — lab_0a86..lab_0ace de game_loop.asm: deriva cat_y de anim_counter e in_level_mode.
 * Exportada (T70) porque la rama de muerte del nivel 2 salta directo a lab_0a86 sin pasar por el
 * bloque de scroll anterior. Cuerpo sin cambios respecto a update_cat_movement. */
void update_cat_dive(void) {
    /* `[si + max_dive_depth]` con si = difficulty_level, SIN mascara (T71: el `& 0x5` anterior mapeaba 2->0 y 3->1) */
    uint8_t dive_cap = ds_pool[0x067c + (uint16_t)difficulty_level];
    uint8_t bl = anim_counter >> 4;
    if (bl > dive_cap) bl = dive_cap;

    uint8_t dl = cat_y;
    uint8_t al_mode = (uint8_t)in_level_mode; /* original's `cmp al,0x1` / `jc` is an
                                                * UNSIGNED byte comparison — with
                                                * in_level_mode==-1 (0xFF unsigned),
                                                * jc is NOT taken (0xFF is unsigned
                                                * "above" 1), so it falls through to
                                                * the subtract branch below. Using the
                                                * signed value directly here would be
                                                * the same class of bug caught earlier
                                                * in animation.c's jc/jnz translation —
                                                * see PROGRESS.md §5e. */
    if (al_mode < 1) goto lab_0ace;                 /* only true when in_level_mode == 0 */
    if (al_mode != 1) goto lab_0ab4;                 /* true for in_level_mode == -1 (0xFF) */
    /* in_level_mode == 1: moving further into the level (down/deeper) */
    dl = (uint8_t)(dl + bl);
    if (dl < 0xb4) goto lab_0ace;
    dl = 0xb3;
    goto lab_0ace;

lab_0ab4:
    /* in_level_mode == -1: moving back out toward the alley */
    {
        int16_t sub = (int16_t)dl - (int16_t)bl;
        if (sub < 0) goto lab_0abd;
        dl = (uint8_t)sub;
        if (dl > 0x3) goto lab_0ace;
    }
lab_0abd:
    /* footstep-sound-sync check (original compares against walk_sprite_ptrs[9]
     * to catch a specific walk-cycle frame and latch a tick for sound.asm's
     * benefit). sound.asm isn't ported yet, so this is a faithful no-op stub —
     * anim_last_tick/level2_tick are updated for future use but nothing
     * currently reads level2_tick. */
    /* `mov ax,[0x9b8] / cmp ax,[cat_sprite_data] / jnz`: 0x9b8 = walk_sprite_ptrs[9] (0x9a6 + 0x12), el puntero
     * de la 2.a pose quieta; cat_sprite_data lo deja update_cat_frame() (animation.c) en el frame anterior. */
    if (mv_ds_word(0x09b8) == cat_sprite_data) {
        level2_tick = anim_last_tick;
    }
    dl = 0x2;

lab_0ace:
    cat_y = dl;
}

/* update_cat_movement — literal goto-based port of game_loop.asm's
 * ~lines 265-330 momentum/scroll/dive block. See include/movement.h and
 * PROGRESS.md §5e for the full writeup, including the in_level_mode
 * tri-state discovery this port surfaced. */
void update_cat_movement(void) {
    /* snapshot BEFORE this frame's new direction/mode are computed —
     * select_cat_sprite() compares against these afterward */
    prev_scroll_dir = scroll_direction;
    prev_vert_dir = in_level_mode;

    int8_t al = input_horizontal;
    if (al != 0) goto lab_0a1a;

    /* no horizontal input: decay speed_ramp toward its floor */
    /* ASM L282: `cmp word [speed_ramp],0x10 / jc lab_0a2e` — por debajo del piso se REINICIA la
     * direccion (scroll_direction = al = 0, speed_ramp = 0x20); no se salta a lab_0a37 (bug T70). */
    if (speed_ramp < 0x10) goto lab_0a2e;
    speed_ramp--;
    goto lab_0a37;

lab_0a1a:
    if (al != scroll_direction) goto lab_0a2e;
    /* same direction held: ramp speed_ramp up toward its ceiling */
    if (speed_ramp >= 0x30) goto lab_0a37;
    speed_ramp += 3;
    goto lab_0a37;

lab_0a2e:
    /* direction just changed: reset to the baseline ramp value */
    scroll_direction = al;
    speed_ramp = 0x20;

lab_0a37: {
    uint16_t speed = speed_ramp >> 3;
    /* `shl bl,1 / [bx + max_swim_speed]`, sin mascara (T71) */
    uint16_t cap = mv_ds_word(0x066c + (((uint16_t)difficulty_level << 1) & 0xff));
    if (speed > cap) speed = cap;
    scroll_speed = speed;
    update_scroll();
}

    al = input_vertical;
    if (al != 0) goto lab_0a6a;

    /* ASM L300-307: `not al` (al = 0 -> 0xFF) / `cmp [anim_counter],0x10 / jc lab_0a7e`. Por debajo del piso se
     * fija in_level_mode = 0xFF (-1) y anim_counter = 0x20 (el C anterior saltaba a lab_0a86; bug T71). */
    al = (int8_t)~al;
    if (anim_counter < 0x10) goto lab_0a7e;
    anim_counter--;
    goto lab_0a86;

lab_0a6a:
    if (al != in_level_mode) goto lab_0a7e;
    /* same vertical direction held: ramp anim_counter up toward ceiling */
    if (anim_counter >= 0x40) goto lab_0a86;
    anim_counter += 4;
    goto lab_0a86;

lab_0a7e:
    /* vertical direction just changed (tri-state: -1/0/1 straight from input) */
    in_level_mode = al;
    anim_counter = 0x20;

lab_0a86:
    update_cat_dive();
}

/* update_walk_frame — literal port of alley.asm's update_walk_frame. */
const cat_walk_frame_t *update_walk_frame(void) {
    if (scroll_direction != prev_scroll_dir) {
        scroll_speed = 0x2;
    }

    if (scroll_speed < 8) {
        anim_accumulator--;
        if ((anim_accumulator & 0x3) == 0) {
            scroll_speed++;
        }
    }

    uint8_t bl = walk_anim_frame;
    bl++;
    if (bl >= 6) bl = 0;
    walk_anim_frame = bl;

    if (scroll_direction == -1) bl += 6;

    return &alley_walk_frames[bl];
}
