#ifndef LEVEL_BACKGROUND_H
#define LEVEL_BACKGROUND_H

/* Ported from score.asm's draw_level_background + alley_drawing.asm's
 * draw_block_list — see src/level_background.c and PROGRESS.md §5w.
 * Draws the current level's room background (border, doors, platforms,
 * ledges, decorative blocks) using the embedded verified data segment
 * (gen/ds_pool.h) as the tile source pool, exactly replicating the
 * original's absolute-DS-offset addressing scheme.
 *
 * NOT ported: level 7 delegates to draw_love_scene_bg (the victory
 * epilogue, roadmap item (e), separately scoped); level 2's randomized
 * block-puzzle background IS ported (see level_background.c). */
void draw_level_background(void);

#endif
