#ifndef SPRITE_H
#define SPRITE_H

#include <stdint.h>

/* width_words: WORD count (verified against real sprite pointer-table
 * deltas — see include/cga.h). Real bytes/row = width_words*2. Pass
 * width_words directly to cga.c's blit_*() functions. */
typedef struct {
    const uint8_t *data;
    uint8_t width_words;
    uint8_t height;
} cat_walk_frame_t;

#endif
