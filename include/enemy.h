#ifndef ENEMY_H
#define ENEMY_H

#include <stdbool.h>

/* Ported from enemy.asm — see src/enemy.c and PROGRESS.md §5o. */
void update_enemies(void);
/* enemy.asm's own reset routine (misleadingly named: it also calls
 * sound.asm's init_chase_sound, which is where the name comes from).
 * entry.asm calls it on every level entry. */
void init_sound(void);
bool check_enemy_activate(void);
bool check_enemy_object_hit(void);
/* check_dog_collision (enemy.asm L69-107, T18) — aterrizaje del proyectil en el nivel 0. */
bool check_dog_collision(void);

#endif
