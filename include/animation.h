#ifndef ANIMATION_H
#define ANIMATION_H

#include "sprite.h"

/* Ported from game_loop.asm — see src/animation.c and PROGRESS.md §6 for
 * the full decode of which branch produces which pose. */
const cat_walk_frame_t *select_cat_sprite(void);

/* update_cat_frame (T71) — lab_0ace..lab_0bab: cat_screen_pos, pose, cat_sprite_data/dims, borrar, dibujar,
 * check_level_objects. Sin cablear hasta T74/T75. */
void update_cat_frame(void);

#endif
