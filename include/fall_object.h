#ifndef FALL_OBJECT_H
#define FALL_OBJECT_H

#include <stdbool.h>

/* Ported from objects.asm — see src/fall_object.c and PROGRESS.md §5u.
 * Objects (bottles/pots) periodically fall from windows above certain
 * alley door positions; the cat must dodge them. */
void reset_jump(void);
/* init_player (level_physics.asm L269-275, T18): jump_anim_counter=0, gravity_y=0,
 * idle_aggro_flag=0, deduct_life=0, jump_toss_delay=9. */
void init_player(void);
void animate_falling(void);
bool check_jump_collision(void);

#endif
