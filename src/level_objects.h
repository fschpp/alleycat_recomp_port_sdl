#ifndef LEVEL_OBJECTS_H
#define LEVEL_OBJECTS_H

/* Ported from level_objects.asm's tick_thrown_objects/draw_thrown_sprite/
 * erase_thrown_sprite/check_thrown_cat_hit/init_thrown_objects/
 * update_footprint/draw_footprint_tile (lines ~41-687). See
 * src/level_objects.c and PROGRESS.md for the full derivation.
 *
 * This is the general "objects thrown from windows" rain used by levels
 * 0-6 (dat_32f2's rate table has 7 entries) — distinct from level 7's own
 * spawn_thrown_object/l7_obj_* system, which belongs to the separate,
 * still-unported victory-epilogue minigame. */

void init_thrown_objects(void);
void tick_thrown_objects(void);

/* update_footprint — draws the cat's footprint trail on the ground.
 * Shares l1_bg_sprite/draw_footprint_tile with tick_thrown_objects (a
 * thrown object landing on a fresh column marks it the same way a
 * footstep does), so it lives here rather than in alley_movement.c even
 * though it's called from the general per-frame walk update. */
void update_footprint(void);

#endif
