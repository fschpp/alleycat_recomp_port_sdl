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

/* spawn_thrown_object (level_objects.asm L41-138, T37): hace caer un corazon desde la ventana mas cercana a cat_y, en la columna
 * de cat_x. Usa l7_obj_spawn_slot (0..7) como slot a ocupar. */
void spawn_thrown_object(void);

/* tick_level_thrown_objects (level_objects.asm L139-208, T38): recoge los corazones que toca el gato (una vez por tick BIOS). */
void tick_level_thrown_objects(void);
extern uint16_t (*l7_tick_fn)(void);  /* solo tests: reloj falso; NULL = reloj real */
extern uint16_t l7_obj_last_tick;     /* DS 0x2e8f */

extern int16_t  l7_obj_x[8];          /* DS 0x2b5a */
extern uint8_t  l7_obj_y[8];          /* DS 0x2b6a */
extern uint8_t  l7_obj_active[8];     /* DS 0x2b72 */
extern uint16_t l7_obj_spawn_slot;    /* DS 0x2e8d */
extern uint8_t  l7_obj_closest_dist;  /* DS 0x2e91 */
extern uint16_t l7_obj_closest_row;   /* DS 0x2e92 */
extern uint16_t l7_obj_last_picked;   /* DS 0x2e94 */
extern uint16_t l7_obj_cur_x;         /* DS 0x2e96 */
extern uint8_t  l7_obj_cur_y;         /* DS 0x2e98 */

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
