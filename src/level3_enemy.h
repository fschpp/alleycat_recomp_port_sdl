#ifndef LEVEL3_ENEMY_H
#define LEVEL3_ENEMY_H

#include <stdint.h>

/* Ported from level_objects.asm lines ~1518-1715 — see src/level3_enemy.c
 * and PROGRESS.md for the full derivation. Called from the level-3
 * per-frame loop (entry.asm's lab_0394/lab_03b2), alongside
 * init/update_level3_doors (a separate, not-yet-ported subsystem). */

void init_level3_enemy(void);
void update_level3_enemy(void);

/* [0x552] in the original — set once the bird's "buzz away" escape
 * sequence finishes. Shared byte-address in the original but only ever
 * written by this subsystem in the currently-ported code; exposed in
 * case a future port of enemy.asm's dog-catch sequence needs to read
 * it (see the comment above enemy_escape_active in level3_enemy.c). */
extern uint8_t l3_bird_escaped;

/* [0x553] in the original — a guard shared across several not-yet-ported
 * "enemy escaping" sequences in level_objects.asm (levels 4/5/6 each have
 * their own analogous fence-creature). Always 0 until those are ported,
 * so this subsystem's escape sequence can never be blocked by another
 * one yet — a documented simplification, see PROGRESS.md. */
extern uint8_t enemy_escape_active;

#endif
