#ifndef MOVEMENT_H
#define MOVEMENT_H

#include <stdbool.h>
#include "sprite.h"

/* update_scroll — ported from alley.asm. Moves cat_x by scroll_speed in
 * scroll_direction, clamped to [scroll_left_bound, scroll_right_bound]
 * (which this function also sets, based on level_number). Returns true
 * if a bound was hit (matches the original's carry-flag return). */
bool update_scroll(void);

/* update_cat_movement — ported from game_loop.asm (~lines 250-330,
 * immediately preceding the select_cat_sprite block this connects to).
 * This is the REAL per-frame movement update: ramps speed_ramp/anim_counter
 * as momentum accumulators from input_horizontal/input_vertical, derives
 * scroll_speed (capped by max_swim_speed[difficulty]) and calls
 * update_scroll(), then derives the new cat_y from anim_counter (capped by
 * max_dive_depth[difficulty]) and in_level_mode. Snapshots prev_scroll_dir/
 * prev_vert_dir at the TOP of this call (before computing this frame's new
 * values) — call this once per frame, then call select_cat_sprite()
 * afterward (do NOT update prev_* separately; this function already did). */
void update_cat_movement(void);

/* update_walk_frame — ported from alley.asm. Advances the walk-cycle
 * phase (walk_anim_frame, 0-5) using its OWN independent speed-ramp
 * (scroll_speed/anim_accumulator — separate from update_cat_movement's
 * speed_ramp/anim_counter system; different callers set scroll_speed
 * differently before invoking whichever movement path they're on), then
 * returns the corresponding Pool B frame (mirrored +6 for leftward
 * movement). This is the REAL general-alley-walking frame selector — see
 * PROGRESS.md §5g/§5h for how this was found and why it differs from
 * select_cat_sprite() (which is level-2-specific). */
const cat_walk_frame_t *update_walk_frame(void);

#endif
