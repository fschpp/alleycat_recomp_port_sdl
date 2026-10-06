#ifndef LEVEL_BACKGROUND_H
#define LEVEL_BACKGROUND_H

#include <stdint.h>

/* Ported from score.asm's draw_level_background + alley_drawing.asm's
 * draw_block_list — see src/level_background.c and PROGRESS.md §5w.
 * Draws the current level's room background (border, doors, platforms,
 * ledges, decorative blocks) using the embedded verified data segment
 * (gen/ds_pool.h) as the tile source pool, exactly replicating the
 * original's absolute-DS-offset addressing scheme.
 *
 * Level 7 delegates to draw_love_scene_bg (T39); level 2's randomized
 * block-puzzle background IS ported (see level_background.c). */
void draw_level_background(void);

/* draw_block_list (alley_drawing.asm L128-158), exportada en T16 para clear_screen.
 * base_offset = ax del original; list_ofs = offset DS de {dims, {src,dst}..., 0xFFFF}. */
void draw_block_list(uint16_t base_offset, uint16_t list_ofs);

/* draw_love_scene_bg / draw_bg_tile (level_objects.asm L209-299, T39): fondo del nivel 7. Ademas de dibujar, rellena window_open_state,
 * reinicia l7_obj_* y coloca l7_completion_counter corazones (ver level_background.c). */
void draw_love_scene_bg(void);
void draw_bg_tile(uint16_t x, uint8_t y, uint16_t bx);

#endif
