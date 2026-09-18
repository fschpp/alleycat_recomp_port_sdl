#ifndef ALLEY_MOVEMENT_H
#define ALLEY_MOVEMENT_H

/* update_alley_movement — the REAL general per-frame cat movement/render
 * dispatch for normal alley walking, ported from game_loop.asm's
 * lab_0e78/lab_0f34/lab_0f63. See src/alley_movement.c and PROGRESS.md
 * §5g/§5h. This supersedes select_cat_sprite()+update_cat_movement() as
 * "the" movement path for a plain alley-walking demo — those are real,
 * verified, but level-2-specific (see §5g). */
void update_alley_movement(void);

#endif
