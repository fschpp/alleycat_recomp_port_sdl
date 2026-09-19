#ifndef CAT_STATE_H
#define CAT_STATE_H

#include <stdint.h>
#include "sprite.h"

/* Ported 1:1 from cat.asm's .data section (types/sizes verified against
 * the original db declarations — see PROGRESS.md §6 for the byte-exact
 * source lines). */

extern int8_t  scroll_direction;   /* 0 = not scrolling, 1 = right, -1(0xFF) = left */
extern int8_t  prev_scroll_dir;
extern int8_t  prev_vert_dir;      /* original: prev_vert_dir, compared against in_level_mode */
extern int8_t  in_level_mode;      /* NOTE: despite the name/earlier assumption, this is
                                     * NOT a plain 0/1 boolean — game_loop.asm's movement
                                     * block assigns it directly from input_vertical
                                     * (-1/0/1), so it's really a tri-state "vertical
                                     * direction" indicator. Existing code that only checks
                                     * ==1 or !=1 (select_cat_sprite, etc.) still works
                                     * correctly either way. Found while porting
                                     * update_cat_movement — see PROGRESS.md §5e. */
extern uint16_t scroll_speed;
extern uint16_t speed_ramp;
extern uint8_t  anim_counter;
extern uint8_t  anim_accumulator;
extern uint8_t  anim_step;
extern int16_t  cat_x;
extern uint8_t  cat_y;
extern uint16_t walk_frame;        /* walk-cycle phase accumulator (RAM counter,
                                     * NOT the same thing as the walk_frame_table
                                     * sprite pointer table — unfortunate original
                                     * naming collision, kept as-is for fidelity) */
extern uint8_t  immune_flag;

/* --- newly ported for real movement physics (game_loop.asm ~250-330,
 * alley.asm update_scroll) --- */
extern int16_t  level_number;
extern uint16_t scroll_left_bound;
extern uint16_t scroll_right_bound;
/* rom_id — hardware.asm's read_rom_id reads BIOS F000:FFFE (0xFD = PCjr,
 * 0xFE = XT, 0xFF = PC/XT). This port targets a plain PC/XT, so 0xFF. It is
 * not decoration: sound.asm, game_loop.asm and enemy.asm all branch on it to
 * halve/double loop counts and animation rates for the slower PCjr. */
extern uint8_t rom_id;

extern uint16_t difficulty_level;          /* word, used as a byte-pair index (*2) into the tables below */
extern const uint16_t max_swim_speed[6];   /* verified DS 0x066c, values (4,6,8,10,12,12) */
extern const uint8_t  max_dive_depth[6];   /* verified DS 0x067c, values (3,3,4,4,4,4) */
extern uint16_t anim_last_tick;
extern uint16_t level2_tick;
extern uint16_t cat_sprite_data;

/* --- newly ported for setup_alley/setup_level (game_loop.asm ~lines 1-107) --- */
extern uint8_t  entry_steps;
extern uint8_t  entry_delay;
extern uint8_t  cat_y_bottom;
extern uint16_t cat_draw_pos;
extern uint8_t  transition_timer;
extern uint8_t  game_mode;
extern uint8_t  at_platform;
extern uint8_t  transitioning;
extern uint8_t  sprite_hidden;
extern uint8_t  cat_died;
extern uint8_t  auto_walk;
extern uint8_t  object_hit;
extern uint16_t level_complete;
extern uint8_t  cat_caught;
extern uint8_t  door_contact;
extern int16_t  saved_cat_x;
extern uint8_t  saved_cat_y;
extern const cat_walk_frame_t *vert_sprite; /* set from &enter_sprite[3] on level entry —
                                                      * see src/game_setup.c setup_level(). Using a
                                                      * real typed pointer here instead of trying to
                                                      * replicate the original's raw 16-bit DS offset
                                                      * (vert_sprite_data/vert_sprite_dims words) —
                                                      * that representation doesn't map onto anything
                                                      * meaningful in this port's architecture, where
                                                      * sprites are proper C arrays, not one flat DS
                                                      * byte blob indexed by saved offsets. */
extern const int16_t level_start_x_table[8]; /* verified DS 0x05d9: 160,144,150,88,256,240,64,152 */
extern const uint8_t level_start_y_table[8]; /* verified DS 0x05e9: 20,96,4,96,96,96,56,175 */
extern uint8_t level2_rise;
extern uint8_t meow_timer;
extern uint16_t ambient_freq;

/* --- newly ported for the real general alley-movement path (§5g/§5h) --- */
extern uint16_t cat_screen_pos;
extern uint16_t buffer_size;              /* CORRECTED (§6f): a packed CX dims pair —
                                            * high byte = rows, low byte = width in WORDS —
                                            * NOT a byte count, as this comment used to say.
                                            * It is handed straight to save_from_cga /
                                            * blit_to_cga as the original's CX. */
extern uint16_t alley_save_buf[128];      /* background-save area. WORDS, because blit_masked
                                            * saves whole destination words into it (the
                                            * original's `bp`), while save_from_cga/blit_to_cga
                                            * read it back bytewise. The original's lives at
                                            * DS 0x05fa with 114 bytes to the next label; the
                                            * largest real use is the cat's own 11 rows x 3
                                            * words = 66 bytes, and spawn_window_event's two
                                            * halves are 24 + 24 = 48. 128 words is generous. */
extern uint8_t  walk_anim_frame;          /* 0-5 walk-cycle phase used by update_walk_frame (Pool B) */

/* --- newly ported for climbing/window-transition (§5m) --- */
extern uint8_t l3_platform_id;
extern uint8_t jump_hit;

/* --- newly ported for the dog enemy system (§5o) --- */
extern uint8_t  enemy_active;
extern int8_t   enemy_dir;
extern uint16_t enemy_x;
extern uint8_t  enemy_y_pos;
extern uint8_t  enemy_chasing;
extern uint16_t enemy_chase_delay;
extern uint8_t  enemy_approach_timer;
extern uint8_t  enemy_exit_timer;
extern uint16_t enemy_last_tick;
extern uint16_t enemy_tick_counter;
extern uint8_t  enemy_anim_frame;
extern uint16_t enemy_sprite_ptr;   /* raw DS-offset-equivalent index into enemy_sprite_table's
                                      * pointed-to bitmap; kept as an index into our C sprite
                                      * array rather than a byte offset — see level_collision.c-
                                      * style pattern established for vert_sprite in §5m */
extern uint16_t enemy_sprite_dims;
extern uint16_t enemy_draw_addr;
extern uint16_t enemy_next_addr;
extern uint16_t enemy_erase_dims;
extern uint8_t  fall_hit;

/* --- newly ported for the falling-object dodge mechanic
 * (objects.asm's animate_falling/check_jump_collision/reset_jump — see
 * src/fall_object.c and PROGRESS.md §5u) --- */
extern uint8_t  fall_counter;
extern uint16_t fall_last_tick;
extern uint16_t fall_target_x;
extern uint8_t  fall_target_y;
extern uint8_t  fall_cur_y;
extern uint16_t fall_cga_addr;
extern uint16_t fall_draw_pos;
extern uint16_t fall_sprite_dims; /* (width_words, height) packed, height varies 1..13..0 */
extern uint16_t fall_save_dims;
extern uint16_t fall_save_buf[26]; /* max 2 words/row x 13 rows = 26 words */
extern uint8_t  last_target_door; /* DS 0x1028, verified in §5n as a 1-byte state var */

/* --- newly ported for score.asm's core score/lives HUD (see
 * src/score.c and PROGRESS.md §5v). NOTE: the original's draw_score/
 * draw_high_score function NAMES are swapped relative to what they
 * actually draw (content is self-consistent with update_high_score's
 * clear comment; only the labels are swapped) — this port's function
 * names follow the CONTENT, not the original (possibly swapped) labels;
 * see §5v for the full derivation. */
extern uint8_t current_score[7]; /* BCD digits, MSB-first (verified via
                                   * add_bcd_scores' right-to-left carry
                                   * propagation starting at index 6) */
extern uint8_t high_score[7];
extern uint8_t lives_display; /* cached lives_count, for redraw-on-change only */
extern uint16_t score_draw_pos; /* real CGA offset, consistent with cat_draw_pos etc. */
extern const uint8_t *score_buf_ptr; /* real pointer into current_score/high_score,
                                       * not a raw address placeholder — see §5f's
                                       * vert_sprite correction for why */
extern uint8_t  score_digit_idx;
extern uint16_t walk_note_index;
extern int16_t  thrown_obj_x;
extern uint8_t  thrown_obj_y;

extern const uint8_t enemy_spawn_chance[8]; /* DS 0x1cd1, verified: 2,4,8,12,16,24,32,64 */
extern const uint8_t enemy_chase_chance[8]; /* DS 0x1cd9, verified: 16,32,48,64,80,96,112,128 */
extern uint8_t lives_count;
extern uint16_t enemy_save_buf[60]; /* scratch background-save area for the enemy sprite
                                       * (4w x 15h max = 60 words) — see draw_enemy/erase_enemy */

/* --- newly ported for level geometry collision detection (§5n) --- */
extern uint16_t entrance_x;
extern uint8_t  entrance_y;
extern uint8_t  platform_cur_type;
extern uint16_t platform_cur_width;
extern uint8_t  door_hit_flag; /* original's [0x551] */

/* Verified tables — see PROGRESS.md §5n for cross-checks */
extern const uint8_t door_position_table[22];  /* DS 0x0ff0 */
extern const uint8_t floor_first_door[8];      /* DS 0x1006 */
extern const uint8_t floor_door_count[8];      /* DS 0x100e */
extern const uint16_t level_platform_index[8]; /* DS 0x1269 */
extern const uint8_t platform_y_table[96];     /* DS 0x1029 */
extern const uint8_t platform_type_table[96];  /* DS 0x1089 */
extern const uint16_t platform_width_table[96];/* DS 0x11a9 */
extern const uint16_t platform_x_left[96];     /* DS 0x10e9 */

/* --- newly ported for the alley's 3 cycling patrol objects (rats/mice) —
 * objects.asm init_objects/cycle_animations, see PROGRESS.md §5s. Real,
 * verified per-slot RAM state; NOT the same system as the dog enemy
 * (enemy_* above) or the level_number-specific level2/level5/level6/level7
 * "obj" variants in level_objects.asm, which are separate, unported. */
extern uint16_t obj_x[3];          /* DS 0x1f30, initial {0,0,0} */
extern uint8_t  obj_y[3];          /* DS 0x1f36, verified {0x00,0x20,0x40} */
extern const uint8_t obj_score[3]; /* DS 0x1f39, verified {9,7,5} */
extern int8_t   obj_dir[3];
extern int8_t   obj_prev_dir[3];
extern uint16_t obj_draw_pos[3];
extern uint8_t  obj_hidden[3];
extern uint16_t obj_cga_tmp;
extern uint8_t  obj_anim_toggle[3];
extern uint8_t  obj_hit[3];
extern uint16_t obj_hit_tick[3];
extern uint16_t obj_last_tick;
extern uint16_t obj_collision_pos;
extern uint16_t obj_speed;
extern uint16_t obj_slot;
extern uint8_t  cycle_active;
extern const uint8_t obj_chase_table[8]; /* DS 0x1f6e, verified:
                                           * 8,32,64,128,160,192,208,240 */
extern uint16_t current_floor; /* which floor row the cat is on; set by the
                                 * unported window state machine — stays 0
                                 * here, matching this port's existing
                                 * simplification for other window-state
                                 * fields (e.g. window_row_offset users) */
extern uint8_t  gravity_y;     /* nonzero while a thrown projectile (fish/
                                 * object at the cat) is in flight — now
                                 * REAL, see update_cat_jump/apply_cat_gravity
                                 * in src/jump_gravity.c, PROGRESS.md §5t
                                 * (supersedes the earlier "stays 0" note) */
extern uint16_t gravity_x;     /* current projectile x, same update */
extern uint16_t obj_save_buf[3][16]; /* per-slot background-save scratch
                                       * (2w x 8h = 16 words max, matching
                                       * cycle_idle/cycle_walk sprite size);
                                       * the original's obj_save_buf held 3
                                       * literal DOS scratch-RAM addresses
                                       * instead — not meaningful here, so
                                       * this port uses real backing arrays,
                                       * same convention as enemy_save_buf */

/* --- newly ported for the fish-jump/gravity-toss enemy system
 * (level_physics.asm's apply_cat_gravity/update_cat_jump +
 * enemy.asm's check_fish_collision/check_trashcan_near/decode_enemy_params
 * — see src/jump_gravity.c and PROGRESS.md §5t). This is a THIRD, separate
 * enemy-like system from the dog (enemy.c) and the 3 ground-patrol
 * objects (cycle_objects.c) — a fish/creature that leaps from a ledge,
 * pauses, then throws a falling projectile at the cat. */
extern uint16_t jump_x;
extern uint16_t gravity_last_tick;
extern uint8_t  jump_y;
extern uint8_t  jump_draw_y;
extern uint8_t  jump_anim_counter;
extern uint8_t  jump_tick_delay;
extern uint8_t  jump_spawn_param;
extern uint16_t jump_toss_delay;
extern uint16_t jump_toss_tick;
extern uint8_t  jump_toss_remaining;
extern uint8_t  gravity_drift_dir;
extern uint8_t  gravity_frame;
extern uint16_t gravity_h_speed;
extern uint8_t  gravity_target_height;
extern uint16_t gravity_cur_dims;
extern uint16_t gravity_cga_addr;
extern uint16_t gravity_prev_cga;
extern uint16_t gravity_save_dims;
extern uint16_t gravity_restore_dims;
extern uint16_t gravity_save_buf[24]; /* max gravity frame: 2w x 12h = 24 words */
extern uint8_t  dog_catch_flag;
extern uint8_t  idle_aggro_flag;
extern uint8_t  deduct_life;
extern const uint16_t jump_spawn_x_table[4]; /* DS 0x1658, verified: 24,104,184,264 */
extern const uint8_t  jump_spawn_y_table[4]; /* DS 0x1660, verified: 24,56,88,24 */
extern const uint16_t jump_pause_by_diff[8]; /* DS 0x181e, verified:
                                               * 45,36,27,18,9,18,1,18 */
extern const uint8_t gravity_height_table[4]; /* DS 0x17d9, verified: 97,100,94,94 */
extern const cat_walk_frame_t *gravity_cur_sprite; /* real typed pointer, same
                                                      * pattern as vert_sprite (§5f) */

#include "input.h" /* reuses input_horizontal/input_vertical from there */

#endif
