#ifndef ENEMY_H
#define ENEMY_H

#include <stdbool.h>

/* Ported from enemy.asm — see src/enemy.c and PROGRESS.md §5o. */
void update_enemies(void);
bool check_enemy_activate(void);
bool check_enemy_object_hit(void);

#endif
