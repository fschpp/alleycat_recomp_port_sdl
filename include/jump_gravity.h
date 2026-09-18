#ifndef JUMP_GRAVITY_H
#define JUMP_GRAVITY_H

/* Ported from level_physics.asm's apply_cat_gravity/update_cat_jump and
 * enemy.asm's check_fish_collision/check_trashcan_near/decode_enemy_params
 * — a fish/creature enemy that leaps from a ledge, pauses, then throws a
 * falling projectile at the cat. Separate from the dog (enemy.c) and the
 * 3 ground-patrol objects (cycle_objects.c) — see PROGRESS.md §5t. */

/* update_cat_jump — the main per-tick dispatcher. Call once per frame. */
void update_cat_jump(void);

/* apply_cat_gravity — animates the thrown projectile's fall arc once
 * gravity_y != 0 (armed by update_cat_jump's toss). Call once per frame,
 * after update_cat_jump. */
void apply_cat_gravity(void);

#endif
