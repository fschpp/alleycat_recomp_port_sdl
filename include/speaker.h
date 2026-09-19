#ifndef SPEAKER_H
#define SPEAKER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* PC speaker / 8253 PIT hardware model.
 *
 * sound.asm never calls a "play a note" API — it writes the speaker
 * hardware directly, in three distinct ways:
 *
 *   1. tone mode:   out 0x43,0xB6 (ch2, lobyte/hibyte, mode 3 square wave)
 *                   out 0x42,lo / out 0x42,hi (the divisor)
 *                   in 0x61 / or al,3 / out 0x61 (gate + data enable)
 *   2. direct PWM:  in 0x61 / xor-or-and with bit 1 / out 0x61, with bit 0
 *                   left clear, so the cone follows the data bit itself
 *                   (play_explosion_effect, play_hiss_sound, update_noise,
 *                   update_buzz_sound all do this)
 *   3. timing:      read_pit_timer() latches PIT channel 0 and reads its
 *                   16-bit down-counter, used as a sub-BIOS-tick clock
 *                   (play_timed_tone, play_falling_sound, play_crash_sound)
 *
 * So the port models the ports, not the notes: sound.c is then a literal
 * translation that writes the same bytes to the same port numbers, and
 * everything audible falls out of this file. See PROGRESS.md §6e.
 *
 * This file is deliberately SDL-free and pure C so it can be unit-tested
 * offline (tests/test_sound.c renders a .wav without an audio device);
 * src/audio.c is the thin SDL2 glue that pumps speaker_render().
 */

#define SPEAKER_SAMPLE_RATE 44100
#define PIT_INPUT_HZ        1193182u   /* 105/88 MHz, the real 8253 clock */

/* The original's busy-wait loops (`loop lab_x`, and the int-0x1a-paced
 * toggle loops) generated sound *by their own execution rate* on a
 * 4.77 MHz 8088. That rate is not reproducible on a modern CPU — which is
 * exactly why the game checks rom_id and halves/doubles its loop counts
 * for the PCjr. The port therefore paces those loops against an emulated
 * 8088 cycle clock (speaker_spin_cycles) instead of running them flat out,
 * which would toggle the speaker far above the audio Nyquist rate and
 * produce nothing but aliasing. */
#define EMU_CPU_HZ          4772727u
/* Cycle costs used to pace the original's loops. 8088 timings; the
 * int 0x1a figure is a BIOS-call estimate (interrupt entry + the handler's
 * own work), not a documented constant — flagged as modeled, not verified. */
#define EMU_CYC_LOOP_INSN   17u    /* `loop` taken */
#define EMU_CYC_INT1A       400u   /* int 0x1a round trip (estimate) */
#define EMU_CYC_PORT_IO     14u    /* in/out al,imm8 */

void    speaker_reset(void);

/* Port-level primitives — these are what sound.c calls. */
void    pit_out_43(uint8_t control);  /* out 0x43,al */
void    pit_ch2_out(uint8_t data);    /* out 0x42,al (lo byte then hi byte) */
uint8_t port61_in(void);              /* in  al,0x61 */
void    port61_out(uint8_t value);    /* out 0x61,al */

/* PIT channel 0's live down-counter (hardware.asm's read_pit_timer). Counts
 * down at PIT_INPUT_HZ and wraps every 65536 ticks (~54.9 ms), same as the
 * real thing, so the original's `prev - now` elapsed-time math works
 * unchanged. */
uint16_t read_pit_timer(void);

/* Paces one iteration of an original busy-wait loop. */
void    speaker_spin_cycles(uint32_t cycles);

/* Live rendering: fills `out` with `frames` mono S16 samples for the audio
 * window that is now due, replaying the port writes at their recorded
 * timestamps. Called from the SDL audio callback (src/audio.c). */
size_t  speaker_render(int16_t *out, size_t frames);

/* Offline rendering, for tests: replays the whole recorded port-write log
 * from its first event, ignoring wall-clock sync. Returns frames written. */
void    speaker_capture_begin(void);
size_t  speaker_capture_render(int16_t *out, size_t max_frames);

/* Introspection, for tests/asserts: current divisor and port-0x61 value. */
uint16_t speaker_current_divisor(void);
uint8_t  speaker_current_port61(void);

#endif
