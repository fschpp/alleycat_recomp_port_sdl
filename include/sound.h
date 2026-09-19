#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

/* Literal port of src/sound.asm. Every function here drives the emulated
 * PC speaker through include/speaker.h's port primitives, exactly as the
 * original drives the real hardware. See PROGRESS.md §6e.
 *
 * NOTE on naming: the original's `init_sound` is NOT in sound.asm at all —
 * it lives in enemy.asm and is an enemy-state reset that happens to end
 * with a call to init_chase_sound (see src/enemy.c). sound.asm's own
 * "reset the music engine" entry point is init_music. */

/* --- per-frame drivers (called from the main loop) --- */
void play_sound(void);          /* entry.asm calls this once per frame */
void play_music_note(void);     /* title-screen music, ui.asm */
void init_music(void);
void init_chase_sound(void);

/* --- one-shot effects --- */
void play_catch_sound(void);
void play_hit_sound(void);
void play_death_melody(void);
void play_meow_sound(void);
void play_hiss_sound(void);
void play_crash_sound(void);
void play_explosion_effect(void);
void play_swoop_sound(void);
void play_random_chirp(void);
void play_random_noise(void);
void play_falling_sound(void);

/* start_tone takes TWO frequencies, and that is not a mistake: the
 * original programs the PIT with AX immediately, but latches BX into
 * [tone_freq], which is what play_sound reprograms on every later tick of
 * the same tone. Call sites that want a plain tone pass the same value
 * twice; play_random_chirp/play_meow_sound deliberately pass different
 * ones. */
void start_tone(uint16_t ax_freq, uint16_t bx_freq);
void play_tone(uint16_t freq, uint16_t loop_count);
void silence_speaker(void);
void set_speaker_freq(uint16_t divisor);
void play_timed_tone(void);

/* --- continuous / stateful effects --- */
void init_buzz_sound(void);
void update_buzz_sound(void);
void reset_noise(void);
void update_noise(void);

/* --- melodies --- */
void init_result_melody(void);
void play_result_note(void);
void init_level_melody(void);
void play_level_note(void);
void play_melody_step(void);
void wipe_sound_start(void);
void play_wipe_note(void);
void init_victory_melody(void);
void play_victory_note(void);
void play_full_victory(void);

/* --- the extra-life award cutscene (sound.asm, but half graphics) --- */
void show_extra_life(void);

/* Sound state another translation unit resets, mirroring the original:
 * alley.asm zeroes fall_sound_x/fall_sound_y when a fall starts.
 * (walk_note_index, zeroed by enemy.asm on spawn, lives in cat_state.h.) */
extern uint16_t fall_sound_x, fall_sound_y;

#endif
