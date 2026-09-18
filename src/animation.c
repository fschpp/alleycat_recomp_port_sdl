#include "cat_state.h"
#include "sprite.h"
#include "gen/cat_walk_frames.h"
#include <stdint.h>

/* select_cat_sprite — literal port of game_loop.asm ~lines 349-411 (the
 * unnamed block that picks which of Pool A's 13 walk_sprite_ptrs/
 * walk_sprite_dims entries to display). Translated 1:1, label-for-label,
 * rather than "cleaned up," to avoid introducing a logic bug while porting
 * a branch tree we don't have a running original to test against — see
 * PROGRESS.md for the semantic decode this preserves:
 *
 *   bx=0x10 (idx 8)      : immune-flag flash (reuses the alley-idle pose)
 *   bx=0x18 (idx 12)     : direction/mode just changed -> "turning" pose
 *   bx=(0,2,4,6)         : walking RIGHT, 4-frame cycle  (idx 0-3)
 *   bx=(8,10,12,14)      : walking LEFT,  4-frame cycle  (idx 4-7)
 *   bx=(16,18)           : idle in the alley, 2-frame bob (idx 8-9)
 *   bx=(20,22)           : climbing in a level, 2-frame  (idx 10-11)
 *
 * Call this once per frame, after scroll_direction/in_level_mode/cat_y
 * have been updated for the current frame but BEFORE prev_scroll_dir/
 * prev_vert_dir get overwritten with this frame's values (the original
 * updates those elsewhere, right after this selection runs). */
const cat_walk_frame_t *select_cat_sprite(void) {
    uint16_t bx;

    if (immune_flag != 0) {
        bx = 0x10;
        goto lab_0b64;
    }

    if (scroll_direction != prev_scroll_dir) goto lab_0afb;
    if (in_level_mode != prev_vert_dir) goto lab_0afb;
    goto lab_0b00;

lab_0afb:
    bx = 0x18;
    goto lab_0b64;

lab_0b00:
    walk_frame++;
    bx = walk_frame;
    if (input_horizontal != 0 || input_vertical != 0) goto lab_0b13;
    bx = (uint16_t)((uint8_t)bx >> 1); /* original only shifts BL, the low byte */

lab_0b13:
    if ((uint8_t)cat_y < 0xb3) goto lab_0b21;   /* jc: unsigned cat_y < 0xb3 */
    if (in_level_mode == 1) goto lab_0b3c;
    goto lab_0b21_body;

lab_0b21:
lab_0b21_body:
    if ((uint8_t)cat_y > 0x4) goto lab_0b2f;    /* ja: unsigned cat_y > 0x4 */
    if (input_vertical != 0) goto lab_0b53;
    goto lab_0b2f_body;

lab_0b2f:
lab_0b2f_body:
    /* jnc lab_0b53 means: jump if (anim_counter>>1) >= speed_ramp (no borrow) */
    if ((uint16_t)(anim_counter >> 1) >= speed_ramp) goto lab_0b53;
    goto lab_0b3c;

lab_0b3c:
    if (scroll_direction == 0) goto lab_0b53;
    bx &= 0x6;
    if (scroll_direction == 1) goto lab_0b64;
    bx |= 0x8;
    goto lab_0b64;

lab_0b53:
    bx &= 0x2;
    bx |= 0x10;
    if (in_level_mode != 1) goto lab_0b64;
    bx += 0x4;

lab_0b64:
    return &cat_walk_frames[bx / 2];
}
