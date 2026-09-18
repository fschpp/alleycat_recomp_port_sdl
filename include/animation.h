#ifndef ANIMATION_H
#define ANIMATION_H

#include "sprite.h"

/* Ported from game_loop.asm — see src/animation.c and PROGRESS.md §6 for
 * the full decode of which branch produces which pose. */
const cat_walk_frame_t *select_cat_sprite(void);

#endif
