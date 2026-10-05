#ifndef LEVEL3_ENEMY_H
#define LEVEL3_ENEMY_H

#include <stdint.h>

/* Ported from level_objects.asm lines ~1518-1715 — see src/level3_enemy.c
 * and PROGRESS.md for the full derivation. Called from the level-3
 * per-frame loop (entry.asm's lab_0394/lab_03b2), alongside
 * init/update_level3_doors (a separate, not-yet-ported subsystem). */

void init_level3_enemy(void);
void update_level3_enemy(void);

/* init_level3_doors / update_level3_doors / close_level3_door (level_objects.asm L1376-1440,
 * T20 — PROGRESS.md §6q). Llamadas desde el loop del nivel 3 (entry.asm L377/L386).
 * close_level3_door recibe bx = 2*índice de puerta (0, 2, 4), igual que el ASM. */
void init_level3_doors(void);
void update_level3_doors(void);
void close_level3_door(uint16_t bx);

/* NOTA (T20): [0x552] y [0x553] son `object_hit` y `cat_caught` de cat_state.h (entry.asm L21-22).
 * Antes este módulo tenía copias propias (`l3_bird_escaped`, `enemy_escape_active`) que nadie leía;
 * se eliminaron y ahora se usan directamente `object_hit` / `cat_caught`. */

#endif
