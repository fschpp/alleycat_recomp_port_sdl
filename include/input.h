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

/* Flags process_keyboard() used to set directly in the original.
 *
 * sound_enabled is a BYTE holding 0xFF (on) or 0x00 (off), NOT a boolean 1 —
 * entry.asm inits it with `mov byte [sound_enabled],0xff` and input.asm
 * toggles it with `not byte [sound_enabled]`. That matters: sound.asm's
 * update_noise and play_hiss_sound do `and dl,byte [sound_enabled]` where dl
 * is the speaker DATA bit (0x02), so a boolean 1 would silently AND those
 * effects to zero forever. See PROGRESS.md §6e, finding 1. */
extern uint8_t sound_enabled;
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
