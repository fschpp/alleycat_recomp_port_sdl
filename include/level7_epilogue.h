#ifndef LEVEL7_EPILOGUE_H
#define LEVEL7_EPILOGUE_H

#include <stdint.h>

/* Ported from level_objects.asm's level-7 "love scene" epilogue
 * (~lines 3195-3862, see PROGRESS.md). This is a large, ~20-function
 * subsystem being ported incrementally in reviewed chunks rather than
 * all at once.
 *
 * Chunk 1 (this header): the "heart cat" sprite/slot primitives —
 * init_level7_objects, save/load_l7_slot, draw/erase/clear_l7_sprite,
 * setup_l7_sprite. Up to 7 small parading-cat sprites are tracked, each
 * with its own 12-byte state slot (position, facing, animation phase),
 * saved/restored between updates the same way the original swaps a
 * single scratch "current cat" struct in and out of 7 backing buffers.
 *
 * NOT yet ported (later chunks): the per-tick walk/animation update
 * (lab_4d14), check_l7_cat_hit, check_l7_cupid, check_l7_object_overlap,
 * check_l7_all_objects, and the victory-wave/march-music sequencing.
 *
 * Real DS offsets for this chunk's symbols were confirmed with
 * tools/resolve_data_segment.py rather than assumed from label names —
 * see PROGRESS.md for why (meaningfully-named labels here, unlike
 * `dat_XXXX` ones, don't encode their own address). */

void init_level7_objects(void);
void update_level7_objects(void);
void check_l7_all_objects(void);

/* run_victory_sequence — the level-7 completion cutscene. Blocking, by
 * design (see the comment above it in level7_epilogue.c) — call this
 * once when the epilogue finishes, not from the per-frame update loop. */
void run_victory_sequence(void);

/* play_victory_march — the level-7 completion transition's marching
 * background/music, called once right after run_victory_sequence.
 * Also blocking, same reasoning — see level7_epilogue.c. */
void play_victory_march(void);

extern uint16_t l7_completion_counter; /* [0x414] */
extern uint16_t l7_completion_tick;    /* [0x412] */

#endif
