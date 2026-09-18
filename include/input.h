#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Mirrors of the original's globals (src/input.asm, src/hardware.asm).
 * input_horizontal / input_vertical: 0xFF = negative dir, 0x01 = positive
 * dir, 0x00 = neutral - same encoding the original game logic expects,
 * so downstream ports of game_loop.asm / alley.asm can consume these
 * exactly as-is. */
extern int8_t input_horizontal;
extern int8_t input_vertical;
extern bool   input_fire;

/* Flags process_keyboard() used to set directly in the original */
extern bool sound_enabled;
extern bool restart_game;
extern bool show_attract;
extern bool pause_requested;

void input_init(void);

/* Replaces read_keyboard_dirs(): samples the live SDL keyboard state into
 * input_horizontal/input_vertical/input_fire. The original had separate
 * "PCjr" vs "XT" key-matrix logic (key_mod1..4) to work around PCjr's
 * different keyboard controller; on modern hardware via SDL that
 * distinction doesn't exist, so this is the unified path. */
void input_poll(void);

/* Replaces process_keyboard(): edge-triggered handling of the
 * non-movement keys (pause, sound toggle, restart, attract/demo, cheat). */
void input_process_keys(void);

#endif
