#ifndef FALL_OBJECT_H
#define FALL_OBJECT_H

#include <stdbool.h>

/* Ported from objects.asm — see src/fall_object.c and PROGRESS.md §5u.
 * Objects (bottles/pots) periodically fall from windows above certain
 * alley door positions; the cat must dodge them. */
void reset_jump(void);
void animate_falling(void);
bool check_jump_collision(void);

#endif
