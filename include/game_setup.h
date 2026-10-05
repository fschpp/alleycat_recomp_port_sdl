#ifndef GAME_SETUP_H
#define GAME_SETUP_H

/* Ported from game_loop.asm — see src/game_setup.c and PROGRESS.md §5f. */
void setup_alley(void);
void setup_level(void);
/* start_auto_walk (game_loop.asm L108-152, T18) — ver PROGRESS.md §6o. */
void start_auto_walk(void);

#endif
