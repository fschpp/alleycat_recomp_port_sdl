#ifndef CYCLE_OBJECTS_H
#define CYCLE_OBJECTS_H

/* Ported from objects.asm's init_objects/cycle_animations — the alley's
 * 3 horizontally-patrolling ground objects (rats/mice, per the cycle_walk
 * sprite's ASCII-art comments already confirmed in PROGRESS.md §5l). See
 * src/cycle_objects.c and PROGRESS.md §5s. NOT the dog enemy system
 * (enemy.c/enemy.h) — a separate, simpler patrol/chase AI that shares no
 * code with it in the original. */

/* init_objects — literal port. Called once per alley entry (setup_alley),
 * same call site as reset_jump/init_player in the original's lab_0140. */
void init_cycle_objects(void);

/* cycle_animations — literal, rate-limited per-tick port. Advances one of
 * the 3 object slots per call (round-robin), matching the original. */
void update_cycle_objects(void);

#endif
