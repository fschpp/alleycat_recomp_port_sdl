#ifndef LEVEL_COLLISION_H
#define LEVEL_COLLISION_H

#include <stdint.h>
#include <stdbool.h>

/* Ported from level_physics.asm / level_objects.asm — see
 * src/level_collision.c and PROGRESS.md §5n. */

/* check_rect_collision — AABB overlap test. Exposed publicly since
 * enemy.c's check_enemy_object_hit reuses it directly, same as the
 * original. */
bool check_rect_collision(int16_t a_x, uint8_t a_y, uint16_t a_w, uint8_t a_h,
                           uint16_t b_x, uint8_t b_y, uint16_t b_w, uint8_t b_h);

bool check_door_position(void);
bool check_fence_collision(void);
bool check_level_platform(void);

/* check_level_collision — the real dispatcher game_loop.asm calls above
 * lab_0e78 to decide whether the cat is at valid ladder/platform/door
 * geometry before allowing a climb transition. Level 0 (window landing)
 * and level 7 (stairs) are not fully ported — see level_collision.c. */
bool check_level_collision(void);

#endif
