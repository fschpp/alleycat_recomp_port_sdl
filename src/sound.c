/* sound.c — literal, label-for-label port of src/sound.asm.
 *
 * All hardware access goes through include/speaker.h's port primitives, so
 * the `out 0x43 / out 0x42 / in-or-out 0x61` sequences below are written in
 * the same order and with the same byte values as the original. Data tables
 * are read straight out of ds_pool at their resolved DS offsets (see the
 * offset block and PROGRESS.md §6e) rather than re-extracted — the sound
 * parameter tables are a dense run of split labels where a 2-byte label is
 * routinely indexed 4 entries deep, so per-label extraction would be
 * exactly the §3/§7 mistake this project keeps catching.
 */

#include "sound.h"
#include "speaker.h"
#include "cat_state.h"
#include "cga.h"
#include "input.h"
#include "gen/ds_pool.h"

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* BIOS `int 0x1a` tick substitute — same convention used throughout this
 * port (~18.2 Hz). */
static uint16_t read_bios_tick(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms / 55);
}

static uint16_t ds_word(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

/* --- ds_pool offsets, all from tools/resolve_data_segment.py's linear walk
 * of cat.asm's .data section. Extents verified against the next label's
 * offset (zero gap) where the table size is knowable — see PROGRESS.md §6e
 * for the per-table evidence. --- */
#define DS_TITLE_MUSIC_FREQS     0x5324  /* 52 words, chromatic divisor table */
#define DS_TITLE_MUSIC_SEQ       0x538c  /* byte offsets into the above; 0x66 = end */
#define DS_AMBIENT_RHYTHM_PATTERN 0x59c2 /* 48 bytes — exactly covers both
                                          * reachable rhythm_base values (see below) */
#define DS_AMBIENT_TEMPO_TABLE   0x59f2  /* 4 words (label span 8) */
#define DS_AMBIENT_NOTE_MASK     0x59fa  /* 4 bytes: label covers 2, code indexes 4 */
#define DS_AMBIENT_OCTAVE_MASK   0x59fc  /* 4 bytes, same split-label pattern */
#define DS_AMBIENT_RHYTHM_BASE   0x59fe  /* 4 words: {0, 0x10, 0x200, 0x600} */
#define DS_AMBIENT_BASE_DUR      0x5a02  /* 4 words */
#define DS_AMBIENT_ACCENT_DUR    0x5a06  /* 4 words */
#define DS_AMBIENT_PITCH_STEP    0x5a0a  /* 4 bytes */
#define DS_AMBIENT_RAND_THRESH   0x5a0c  /* 4 bytes */
#define DS_FALL_SND_WOBBLE       0x5a1a  /* 17 words (indexed &0x1e → 0..15) */
#define DS_WALK_NOTE_TABLE       0x5a44  /* 8 words (label span 16) */
#define DS_WIPE_NOTE_TABLE       0x5a5a  /* 4 words (label span 8) */
#define DS_LEVEL_MELODY_NOTES    0x5a64  /* 14 words (label span 28) */
#define DS_RESULT_MELODY_NORMAL  0x5a87  /* 14 words (label span 28) */
#define DS_RESULT_MELODY_BONUS   0x5aa3  /* 20 words (label span 40), 0-terminated */
#define DS_BUZZ_PATTERN          0x5ad2  /* 16 bytes (label span 16) */
#define DS_EXPLODE_PATTERN       0x5ae6  /* 32 bytes (label span 32) */
#define DS_VICTORY_MELODY        0x5934  /* 67 words; code wraps at byte 0x86 */
#define DS_MEOW_SOUND_DATA       0x1312  /* used as a FREQUENCY, not a pointer */
#define DS_EXTRALIFE_SPRITES     0x5f62  /* 2 words → 0x5b20, 0x5d40 */
#define DS_EXTRALIFE_ICON_DATA   0x5f68  /* 128 bytes = 4 words x 16 rows */
#define DS_EXTRALIFE_TEXT_DATA   0x5fe8  /* 252 bytes = 6 words x 21 rows */
#define DS_EXTRALIFE_TEXT_POS    0x60e4  /* 3 used words */

/* --- sound-engine state (DS variables in the original) --- */
static uint16_t title_music_pos, title_music_tick;
static uint8_t  chase_intro_ticks, chase_tick_div;
static uint16_t chase_toggle, chase_sweep_freq, chase_freq_mask, chase_last_tick;
static uint8_t  tone_duration;
static uint16_t tone_freq, tone_last_tick;
static uint16_t ambient_last_tick, ambient_duration;
static uint8_t  ambient_rhythm, ambient_note_pos, ambient_pitch_acc;
static uint8_t  ambient_direction, ambient_ornament;
static uint16_t walk_seq_index;
static uint8_t  post_explode_ticks;
static uint16_t explode_start_tick, explode_counter;
static uint8_t  explode_speed_shift;
static uint16_t buzz_phase;
static uint16_t swoop_freq;
static uint8_t  noise_counter;
static uint16_t noise_period;
static uint16_t result_melody_pos, result_melody_tick;
static uint16_t level_melody_pos, level_note_tick;
static uint8_t  level_note_toggle;
static uint16_t wipe_note_index;
static uint16_t meow_pitch_offset;
static uint16_t hiss_start_tick, hiss_phase;
static uint16_t fall_snd_pit_prev, fall_snd_wobble_idx;
static uint16_t rand_noise_tick;
static uint16_t crash_base_freq, crash_start_tick, crash_pit_prev;
static uint16_t victory_melody_pos, victory_last_tick, victory_prev_freq;
static uint16_t extralife_tick, extralife_anim_step;

/* Reset by alley.asm's window state machine (not ported), so not static.
 * walk_note_index is the same deal but already lives in cat_state.c — it is
 * zeroed by enemy.asm on enemy spawn — so it is used from there. */
uint16_t fall_sound_x, fall_sound_y;

/* --- silence_speaker --- */
void silence_speaker(void) {
    port61_out((uint8_t)(port61_in() & 0xfc));
}

/* --- set_speaker_freq --- */
void set_speaker_freq(uint16_t ax) {
    pit_ch2_out((uint8_t)(ax & 0xff));
    pit_ch2_out((uint8_t)(ax >> 8));
    port61_out((uint8_t)(port61_in() | 0x03));
}

/* --- play_music_note --- title-screen music, driven off the BIOS tick. */
void play_music_note(void) {
    if (!sound_enabled) return;
    uint16_t dx = read_bios_tick();
    if (dx == title_music_tick) return;
    title_music_tick = dx;

    uint8_t note = ds_pool[DS_TITLE_MUSIC_SEQ + title_music_pos];
    if (note == 0x66) goto lab_53dd;   /* end marker: stop, don't advance */
    title_music_pos++;
    if (note != 0) goto lab_53e1;      /* 0 = rest */
lab_53dd:
    silence_speaker();
    return;
lab_53e1:
    pit_out_43(0xb6);
    /* `note` is a BYTE offset into a word table, not an index. */
    set_speaker_freq(ds_word(DS_TITLE_MUSIC_FREQS + note));
}

/* --- init_chase_sound --- */
void init_chase_sound(void) {
    chase_intro_ticks = 0x0c;
    chase_toggle      = 0x1;
    chase_sweep_freq  = 0x1ff;
    chase_freq_mask   = 0xf;
    chase_tick_div    = 0x1;
}

/* --- play_timed_tone ---
 * Plays [ambient_freq] and BLOCKS for [ambient_duration] PIT ticks. The
 * durations in use are 0x1000-0x2000 ticks ≈ 3.4-6.9 ms, i.e. well under
 * one frame, so blocking here is faithful and harmless — unlike the long
 * cutscene waits elsewhere in the port. */
void play_timed_tone(void) {
    pit_out_43(0xb6);
    uint16_t ax = ambient_freq;
    pit_ch2_out((uint8_t)(ax & 0xff));
    pit_ch2_out((uint8_t)(ax >> 8));
    port61_out((uint8_t)(port61_in() | 0x03));

    uint16_t cx = read_pit_timer();
    for (;;) {
        uint16_t now = read_pit_timer();
        uint16_t dx = (uint16_t)(cx - now);   /* ch0 counts DOWN */
        if (dx >= ambient_duration) break;
    }
    port61_out((uint8_t)(port61_in() & 0xfc));
}

/* --- play_sound --- the per-frame sound tick: chase siren, one-shot tone
 * tail, and the ambient/walking music generator. */
void play_sound(void) {
    if (!sound_enabled) return;
    if (enemy_active == 0) goto lab_54f8;

    {
        uint16_t dx = read_bios_tick();
        if (chase_intro_ticks != 0) goto lab_54a6;
        if (dx == chase_last_tick) return;
        chase_last_tick = dx;
        pit_out_43(0xb6);
        {
            uint16_t ax = (uint16_t)(chase_sweep_freq & 0x1ff);
            ax = (uint16_t)(ax + 0xc8);
            set_speaker_freq(ax);
        }
        chase_sweep_freq = (uint16_t)(chase_sweep_freq - 0x4b);
        return;

    lab_54a6:
        if (dx != chase_last_tick) {
            chase_last_tick = dx;
            chase_intro_ticks--;
        }
        /* lab_54b4 */
        if (--chase_tick_div != 0) return;
        {
            uint8_t al = 0x1;
            if (rom_id != 0xfd) al = (uint8_t)(al << 1);
            chase_tick_div = al;
        }
        if ((uint8_t)(cga_random() & 0xff) <= 0x4) chase_toggle++;
        if (chase_toggle & 0x1) chase_freq_mask = (uint16_t)(chase_freq_mask + 0x7);
        pit_out_43(0xb6);
        {
            uint16_t ax = (uint16_t)(cga_random() & chase_freq_mask);
            ax &= 0x1ff;
            ax = (uint16_t)(ax + 0x190);
            set_speaker_freq(ax);
        }
        return;
    }

lab_54f8: {
    uint16_t dx = read_bios_tick();
    if (tone_duration != 0) {
        if (dx == tone_last_tick) return;
        tone_last_tick = dx;
        if (--tone_duration == 0) {
            silence_speaker();
            return;
        }
        pit_out_43(0xb6);
        set_speaker_freq(tone_freq);
        return;
    }

    /* lab_5522 — ambient music generator. */
    if (dx == ambient_last_tick) return;

    uint16_t si = 0x3;
    if ((enemy_chasing | post_explode_ticks) == 0) {
        si = 0x1;
        if (level_number == 0) {
            si = 0x0;
            if (gravity_y != 0) si = 0x2;
        }
    }
    /* lab_5549 */
    uint16_t di = (uint16_t)(si << 1);
    if ((auto_walk | post_explode_ticks) == 0) {
        uint16_t ax = (uint16_t)(dx - ambient_last_tick);
        if (ax < ds_word((uint16_t)(DS_AMBIENT_TEMPO_TABLE + di))) return; /* lab_5562 */
    }

    /* lab_5563 */
    ambient_last_tick = dx;
    if (enemy_chasing == 0) {
        if (post_explode_ticks == 0) goto lab_559e;
        post_explode_ticks--;
    }

    /* lab_5579 — chase/post-explosion footstep notes. */
    {
        ambient_duration = 0x1200;
        uint16_t bx = walk_note_index;
        if (bx >= 0x6) {
            bx = 0;
            walk_note_index = 0;
        }
        walk_note_index = (uint16_t)(walk_note_index + 2);
        ambient_freq = ds_word((uint16_t)(DS_WALK_NOTE_TABLE + bx));
        play_timed_tone();
        return;
    }

lab_559e:
    if (si == 0x2) {
        /* Level-0 falling-projectile whistle: pitch tracks gravity_y. */
        uint16_t ax = (uint16_t)((uint16_t)gravity_y << 4);
        ax = (uint16_t)(ax + 0x200);
        ambient_freq = ax;
        ambient_duration = 0x1800;
        goto lab_568d;
    }

    /* lab_55bb */
    ambient_duration = ds_word((uint16_t)(DS_AMBIENT_BASE_DUR + di));
    {
        uint8_t carry = (uint8_t)(ambient_rhythm & 1);
        ambient_rhythm = (uint8_t)(ambient_rhythm >> 1);
        if (!carry) goto lab_5623;
    }
    ambient_duration = 0x1000;
    ambient_rhythm   = 0x80;
    ambient_note_pos++;
    {
        uint8_t al = (uint8_t)(ambient_note_pos & ds_pool[DS_AMBIENT_NOTE_MASK + si]);
        if (al != 0) goto lab_5616_with_al;

        ambient_pitch_acc = (uint8_t)(ambient_pitch_acc + ds_pool[DS_AMBIENT_PITCH_STEP + si]);
        {
            uint8_t dl = (uint8_t)(cga_random() & 0xff);
            if (dl <= ds_pool[DS_AMBIENT_RAND_THRESH + si]) {
                ambient_ornament = (uint8_t)(dl & 0x7);
            }
        }
        /* lab_55f8 — pick a fresh pitch and a sweep direction. */
        {
            uint16_t dx2 = (uint16_t)(cga_random() & 0xff);
            dx2 = (uint16_t)(dx2 << 1);
            uint8_t cl = 0x1;
            if ((uint8_t)dx2 & 0x2) {
                cl = 0xff;
                dx2 = (uint16_t)(dx2 + 0x300);
            }
            ambient_freq      = dx2;
            ambient_direction = cl;
        }
    lab_5616_with_al:
        {
            uint8_t ah = (uint8_t)(ambient_pitch_acc & ds_pool[DS_AMBIENT_OCTAVE_MASK + si]);
            ambient_note_pos = (uint8_t)(al | ah);
        }
    }

lab_5623:
    if (ambient_direction != 0xff) {
        walk_seq_index = (uint16_t)(walk_seq_index + 2);
        uint16_t bx = (uint16_t)(walk_seq_index & 0xe);
        ambient_freq = ds_word((uint16_t)(DS_WALK_NOTE_TABLE + bx));
    } else {
        /* lab_5640 — descending sweep, rearmed when it bottoms out. */
        if (ambient_freq <= 0xc8) ambient_freq = 0x500;
        ambient_freq = (uint16_t)(ambient_freq - 0x19);
    }

    /* lab_5653. The original's `jnz lab_568d` here tests flags left by the
     * `cmp byte [auto_walk],0` three instructions earlier (the two `mov`s
     * in between don't touch flags), so on the auto_walk!=0 path it is an
     * unconditional jump. Translated as such. */
    if (auto_walk != 0) {
        ambient_duration  = 0x2000;
        ambient_direction = 0xff;
        goto lab_568d;
    }

    /* lab_5667 — rhythm gate. ambient_rhythm_base is {0,0x10,...}; only
     * si=0 (mask 0x3|0xc → pos ≤ 0x0f, base 0) and si=1 (mask 0x7|0x18 →
     * pos ≤ 0x1f, base 0x10) are reachable here, so the deepest read is
     * 0x10+0x1f = 0x2f — the last byte of the 48-byte pattern table.
     * Zero slack, which independently confirms the table's extent. */
    {
        uint16_t bx = (uint16_t)ambient_note_pos;
        bx = (uint16_t)(bx + ds_word((uint16_t)(DS_AMBIENT_RHYTHM_BASE + di)));
        uint8_t al = (uint8_t)(ds_pool[DS_AMBIENT_RHYTHM_PATTERN + bx] & ambient_rhythm);
        if (al != 0) goto lab_568d;
        if (ambient_ornament == 0) return;   /* lab_5690 */
        ambient_ornament--;
        ambient_duration = ds_word((uint16_t)(DS_AMBIENT_ACCENT_DUR + di));
    }

lab_568d:
    play_timed_tone();
}
}

/* --- play_explosion_effect ---
 * Direct-PWM noise burst for ~2 BIOS ticks (~110 ms), with the CGA border
 * flashed red for the duration. A genuine one-shot, so blocking matches the
 * original (same reasoning as PROGRESS.md §6c/§6d's cutscenes).
 *
 * The loop's iteration rate WAS the timbre on a 4.77 MHz 8088; here it's
 * paced against the emulated cycle clock instead. Documented, not guessed —
 * see PROGRESS.md §6e. */
void play_explosion_effect(void) {
    silence_speaker();
    /* `mov ah,0xB / mov bx,4 / int 0x10` — set CGA border/background to red.
     * cga.c models the framebuffer only (no border register), so this has no
     * counterpart yet. Left explicit rather than silently dropped. */
    /* TODO: border colour — needs a border/palette register in cga.c. */

    explode_start_tick = read_bios_tick();
    explode_counter    = 0;
    {
        uint8_t al = 0x2;
        if (rom_id == 0xfd) al = (uint8_t)(al >> 1);
        explode_speed_shift = al;
    }

    /* Per-iteration emulated cost: one int 0x1a, two port accesses, and
     * roughly a dozen ALU/memory instructions. */
    const uint32_t iter_cycles = EMU_CYC_INT1A + 2u * EMU_CYC_PORT_IO + 60u;

    for (;;) {
        if (sound_enabled) {
            explode_counter++;
            uint16_t bx = (uint16_t)(explode_counter >> explode_speed_shift);
            bx &= 0x1f;
            port61_out((uint8_t)(port61_in() ^ ds_pool[DS_EXPLODE_PATTERN + bx]));
        }
        /* lab_56d8 */
        speaker_spin_cycles(iter_cycles);
        if ((uint16_t)(read_bios_tick() - explode_start_tick) >= 2) break;
    }

    /* `mov ah,0xB / sub bx,bx / int 0x10` — border back to black. */
    post_explode_ticks = 0x0c;
    silence_speaker();
}

/* --- init_buzz_sound / update_buzz_sound --- level-3 bird buzz. */
void init_buzz_sound(void) {
    uint16_t ax = 0x200;
    if (rom_id == 0xfd) ax = (uint16_t)(ax << 1);
    buzz_phase = ax;
}

void update_buzz_sound(void) {
    buzz_phase++;
    uint16_t bx = buzz_phase;
    uint16_t dx = (uint16_t)(bx >> 9);
    uint8_t  cl = (uint8_t)(dx & 0xf);
    bx = (uint16_t)(bx >> cl);
    bx &= 0xf;
    uint8_t dl = (uint8_t)(ds_pool[DS_BUZZ_PATTERN + bx] & (uint8_t)sound_enabled);
    port61_out((uint8_t)((port61_in() & 0xfc) | dl));
}

/* --- play_delayed_tone / play_swoop_sound --- */
static void play_delayed_tone(void) {
    uint16_t cx = 0x1000;
    if (rom_id == 0xfd) cx = (uint16_t)(cx >> 1);
    speaker_spin_cycles((uint32_t)cx * EMU_CYC_LOOP_INSN);
    if (!sound_enabled) return;
    pit_out_43(0xb6);
    {
        uint16_t ax = swoop_freq;
        pit_ch2_out((uint8_t)(ax & 0xff));
        pit_ch2_out((uint8_t)(ax >> 8));
    }
    port61_out((uint8_t)(port61_in() | 0x03));
}

void play_swoop_sound(void) {
    swoop_freq = 0x1f4;
    do {
        play_delayed_tone();
        swoop_freq = (uint16_t)(swoop_freq - 0x1e);
    } while (swoop_freq > 0xc8);
    swoop_freq = 0x1f4;
    do {
        play_delayed_tone();
        swoop_freq = (uint16_t)(swoop_freq - 0x14);
    } while (swoop_freq > 0x12c);
    do {
        play_delayed_tone();
        swoop_freq = (uint16_t)(swoop_freq + 0x1e);
    } while (swoop_freq < 0x320);
    silence_speaker();
}

/* --- reset_noise / update_noise --- direct-PWM hiss with a slowly
 * lengthening period; called once per frame by level_objects.asm. */
void reset_noise(void) {
    silence_speaker();
    noise_counter = 0;
    noise_period  = 0x8;
}

void update_noise(void) {
    noise_counter++;
    uint8_t dl = 0;
    uint8_t al = (uint8_t)(noise_counter & 0x3f);
    if (al == 0) noise_period++;
    /* lab_57b7 */
    uint16_t bx = (uint16_t)(noise_period >> 2);
    uint8_t bl = (uint8_t)(bx & 0x1f);
    if (al >= bl) dl = 0x2;
    /* lab_57c8. sound_enabled is 0xFF/0x00, NOT 1 — this AND is exactly why
     * (see PROGRESS.md §6e, finding 1). */
    dl = (uint8_t)(dl & (uint8_t)sound_enabled);
    port61_out((uint8_t)((port61_in() & 0xfd) | dl));
}

/* --- result melody (level-complete / extra-life jingle) --- */
void init_result_melody(void) {
    result_melody_pos  = 0;
    result_melody_tick = read_bios_tick();
}

void play_result_note(void) {
    if (!sound_enabled) return;
    uint16_t dx = read_bios_tick();
    if ((uint16_t)(dx - result_melody_tick) < 2) return;
    result_melody_tick = dx;

    uint16_t bx = result_melody_pos;
    result_melody_pos = (uint16_t)(result_melody_pos + 2);

    uint16_t ax;
    if (object_hit != 0) {
        ax = ds_word((uint16_t)(DS_RESULT_MELODY_BONUS + bx));
        if (ax == 0) {                 /* 0 terminates the bonus variant */
            silence_speaker();
            return;
        }
    } else {
        ax = ds_word((uint16_t)(DS_RESULT_MELODY_NORMAL + bx));
    }
    pit_out_43(0xb6);
    set_speaker_freq(ax);
}

/* --- level melody (the descending "level start" run) --- */
void init_level_melody(void) {
    level_melody_pos  = 0;
    level_note_toggle = 0;
}

void play_level_note(void) {
    if (!sound_enabled) return;
    uint16_t dx = read_bios_tick();
    if (dx == level_note_tick) return;

    level_note_tick = dx;
    level_note_toggle++;
    pit_out_43(0xb6);
    uint16_t bx = level_melody_pos;
    if ((level_note_toggle & 0x1) == 0) bx = (uint16_t)(bx + 2);
    set_speaker_freq(ds_word((uint16_t)(DS_LEVEL_MELODY_NOTES + bx)));
}

void play_melody_step(void) {
    if (!sound_enabled) return;
    pit_out_43(0xb6);
    uint16_t bx = level_melody_pos;
    level_melody_pos = (uint16_t)(level_melody_pos + 2);
    set_speaker_freq(ds_word((uint16_t)(DS_LEVEL_MELODY_NOTES + bx)));
}

/* --- screen-wipe arpeggio --- */
void wipe_sound_start(void) {
    /* The original is a bare `ret` — a hook that was never filled in. */
}

void play_wipe_note(void) {
    if (!sound_enabled) return;
    pit_out_43(0xb6);
    uint16_t bx = (uint16_t)(wipe_note_index & 0x6);
    wipe_note_index = (uint16_t)(wipe_note_index + 2);
    set_speaker_freq(ds_word((uint16_t)(DS_WIPE_NOTE_TABLE + bx)));
}

/* --- init_music --- resets the ambient generator. */
void init_music(void) {
    ambient_rhythm     = 0x80;
    ambient_note_pos   = 0x0;
    ambient_pitch_acc  = 0x0;
    ambient_freq       = 0x500;
    ambient_direction  = 0xff;
    ambient_ornament   = 0x0;
    tone_duration      = 0x0;
    post_explode_ticks = 0x0;
    meow_pitch_offset  = 0x0;
    chase_toggle       = 0x1;
    chase_tick_div     = 0x1;
}

/* --- play_tone --- blocking beep; cx is a raw delay-loop count. */
void play_tone(uint16_t bx, uint16_t cx) {
    if (!sound_enabled) return;
    pit_out_43(0xb6);
    pit_ch2_out((uint8_t)(bx & 0xff));
    pit_ch2_out((uint8_t)(bx >> 8));
    port61_out((uint8_t)(port61_in() | 0x03));
    if (rom_id == 0xfd) cx = (uint16_t)(cx >> 1);
    uint32_t iters = (cx == 0) ? 65536u : (uint32_t)cx;  /* `loop` with cx=0 wraps */
    speaker_spin_cycles(iters * EMU_CYC_LOOP_INSN);
    silence_speaker();
}

void play_catch_sound(void) {
    if (enemy_chasing == 0) play_tone(0x390, 0x1800);
    door_contact = 0;
}

void play_hit_sound(void) {
    if (enemy_chasing == 0) play_tone(0x400, 0x1800);
}

void play_death_melody(void) {
    play_tone(0x7d0, 0x1800);
    play_tone(0xa6e, 0x1800);
    play_tone(0xdec, 0x1800);
}

/* --- start_tone --- see sound.h for why this takes two frequencies. */
void start_tone(uint16_t ax, uint16_t bx) {
    if (!sound_enabled) return;
    tone_freq = bx;
    pit_out_43(0xb6);
    set_speaker_freq(ax);
    tone_duration  = 0x2;
    tone_last_tick = read_bios_tick();
}

void play_random_chirp(void) {
    if (!sound_enabled) return;
    if (tone_duration != 0) return;
    uint16_t ax = (uint16_t)(cga_random() & 0x7f);
    ax = (uint16_t)(ax + 0xaa);
    uint16_t bx = ax;
    ax = (uint16_t)(ax + 0x1e);
    start_tone(ax, bx);
}

/* --- play_meow_sound ---
 * meow_sound_data is used as a FREQUENCY here, not as a pointer: the
 * original loads the label's own DS offset (0x1312) into BX and adds the
 * running pitch offset. Verified against the disassembly, not assumed. */
void play_meow_sound(void) {
    if (!sound_enabled) return;
    uint16_t ax = (uint16_t)(0x1200 + meow_pitch_offset);
    uint16_t bx = (uint16_t)(DS_MEOW_SOUND_DATA + meow_pitch_offset);
    meow_pitch_offset = (uint16_t)(meow_pitch_offset + 0x15e);
    start_tone(ax, bx);
    post_explode_ticks = 0x18;
}

/* --- play_hiss_sound --- blocking direct-PWM hiss, ~5 BIOS ticks. */
void play_hiss_sound(void) {
    port61_out((uint8_t)(port61_in() & 0xfe));
    hiss_start_tick = read_bios_tick();
    hiss_phase      = 0;

    const uint32_t iter_cycles = EMU_CYC_INT1A + 30u;   /* the inner wait loop */

    for (;;) {
        /* lab_59df */
        uint16_t ax = (uint16_t)(hiss_phase >> 6);
        if (ax == 0) ax = 1;
        uint16_t cx = ax;

        bool finished = false;
        for (;;) {
            /* lab_59eb */
            speaker_spin_cycles(iter_cycles);
            uint16_t dx = (uint16_t)(read_bios_tick() - hiss_start_tick);
            if (dx < 2) continue;                 /* not started yet */
            if (dx >= 7) { finished = true; break; }
            if (--cx == 0) break;
        }
        if (finished) break;

        uint8_t dl = (uint8_t)(cga_random() & 0x2);
        dl = (uint8_t)(dl & (uint8_t)sound_enabled);
        port61_out((uint8_t)(port61_in() ^ dl));
        hiss_phase = (uint16_t)(hiss_phase + 0x7);
    }
    /* lab_5a18 */
    silence_speaker();
}

/* --- play_falling_sound --- per-frame wobbling descent whistle. Gated on
 * the PIT ch0 counter, not the BIOS tick, so it updates far faster. */
void play_falling_sound(void) {
    if (!sound_enabled) return;
    uint16_t ax = read_pit_timer();
    uint16_t bx = (uint16_t)(fall_snd_pit_prev - ax);
    bool carry  = fall_snd_pit_prev < ax;   /* borrow out of the SUB */
    if (!carry && bx <= 0x260) return;

    /* lab_5a35 */
    fall_snd_pit_prev = ax;
    pit_out_43(0xb6);
    fall_snd_wobble_idx++;
    bx = (uint16_t)(fall_snd_wobble_idx & 0x1e);

    ax = (uint16_t)(fall_sound_x & 0x3ff);
    if (ax >= 0x180) {
        /* mov cx,0x180 / sub cx,ax / xchg ax,cx */
        ax = (uint16_t)(0x180 - ax);
    }
    /* lab_5a59 */
    ax = (uint16_t)(ax >> 1);
    ax = (uint16_t)(ax >> 1);
    ax = (uint16_t)(ax + ds_word((uint16_t)(DS_FALL_SND_WOBBLE + bx)));

    uint16_t step = 0x1;
    if (rom_id == 0xfd) step = (uint16_t)((uint8_t)(step << 1));
    /* lab_5a6d */
    fall_sound_y = (uint16_t)(fall_sound_y + step);
    step = (uint16_t)(step << 1);
    step = (uint16_t)(step << 1);
    fall_sound_x = (uint16_t)(fall_sound_x + step);

    uint16_t dx = (uint16_t)(fall_sound_y >> 3);
    ax = (uint16_t)(ax + dx);
    pit_ch2_out((uint8_t)(ax & 0xff));
    pit_ch2_out((uint8_t)(ax >> 8));
    port61_out((uint8_t)(port61_in() | 0x03));
}

/* --- play_random_noise --- one random pitch per BIOS tick. */
void play_random_noise(void) {
    if (!sound_enabled) return;
    uint16_t dx = read_bios_tick();
    if (dx == rand_noise_tick) return;

    rand_noise_tick = dx;
    pit_out_43(0xb6);
    uint16_t ax = (uint16_t)(cga_random() & 0x70);
    ax = (uint16_t)(ax + 0x200);
    pit_ch2_out((uint8_t)(ax & 0xff));
    pit_ch2_out((uint8_t)(ax >> 8));
    port61_out((uint8_t)(port61_in() | 0x03));
}

/* --- play_crash_sound --- blocking descending crash, ~2 BIOS ticks, paced
 * off the PIT counter (0x9c40 ticks ≈ 33.5 ms per step). */
void play_crash_sound(void) {
    crash_base_freq  = 0x338;
    crash_start_tick = read_bios_tick();
    crash_pit_prev   = read_pit_timer();

    for (;;) {
        uint16_t ax = read_pit_timer();
        uint16_t dx = ax;
        ax = (uint16_t)(ax - crash_pit_prev);
        if (ax >= 0x9c40) {
            crash_pit_prev = dx;
            if (sound_enabled) {
                pit_out_43(0xb6);
                uint16_t f = (uint16_t)(cga_random() & 0x7ff);
                f = (uint16_t)(f + crash_base_freq);
                crash_base_freq = (uint16_t)(crash_base_freq - 0x2);
                pit_ch2_out((uint8_t)(f & 0xff));
                pit_ch2_out((uint8_t)(f >> 8));
                port61_out((uint8_t)(port61_in() | 0x03));
            }
        }
        /* lab_5b10 */
        speaker_spin_cycles(EMU_CYC_INT1A + 40u);
        if ((uint16_t)(read_bios_tick() - crash_start_tick) >= 2) break;
    }
    silence_speaker();
}

/* --- victory melody --- */
void init_victory_melody(void) {
    victory_melody_pos = 0;
    victory_last_tick  = read_bios_tick();
}

void play_victory_note(void) {
    if (!sound_enabled) return;
    uint16_t dx = read_bios_tick();
    if ((uint16_t)(dx - victory_last_tick) < 2) return;

    victory_last_tick = dx;
    uint16_t bx = (uint16_t)(victory_melody_pos & 0xfe);
    if (bx >= 0x86) {
        bx = 0;
        victory_melody_pos = 0;
    }
    victory_melody_pos = (uint16_t)(victory_melody_pos + 2);

    uint16_t ax = ds_word((uint16_t)(DS_VICTORY_MELODY + bx));
    uint16_t cx = victory_prev_freq;
    victory_prev_freq = ax;
    if (ax == cx) {                 /* repeated note: retrigger as a rest */
        silence_speaker();
        return;
    }
    pit_out_43(0xb6);
    pit_ch2_out((uint8_t)(ax & 0xff));
    pit_ch2_out((uint8_t)(ax >> 8));
    port61_out((uint8_t)(port61_in() | 0x03));
}

void play_full_victory(void) {
    if (!sound_enabled) return;
    do {
        play_victory_note();
    } while (victory_melody_pos < 0x7c);
}

/* --- show_extra_life ---
 * Lives in sound.asm but is half cutscene: animates two 32x68 frames plus
 * an icon and a text banner while the result melody plays.
 *
 * Sprite extents all verified zero-gap against the resolved data segment:
 *   extralife_sprites[0] 0x5b20 + 4 words x 68 rows (544 B) = 0x5d40 = [1]
 *   extralife_sprites[1] 0x5d40 + 544 B                     = 0x5f60 = next label
 *   extralife_icon_data  0x5f68 + 4 words x 16 rows (128 B) = next label
 *   extralife_text_data  0x5fe8 + 6 words x 21 rows (252 B) = next label
 * — i.e. the cx dims the code passes to blit_to_cga account for every byte.
 */
void show_extra_life(void) {
    extralife_tick      = read_bios_tick();
    extralife_anim_step = 0;

    do {
        /* lab_5bee */
        uint16_t bx = extralife_anim_step;
        extralife_anim_step = (uint16_t)(extralife_anim_step + 2);
        bx &= 0x2;
        uint16_t si = ds_word((uint16_t)(DS_EXTRALIFE_SPRITES + bx));
        blit_to_cga(&ds_pool[si], 0xa74, 0x04, 0x44);

        do {
            /* lab_5c0d */
            play_result_note();
        } while ((uint16_t)(read_bios_tick() - extralife_tick) < 4);
        extralife_tick = read_bios_tick();

        if (extralife_anim_step == 0x4) {
            blit_to_cga(&ds_pool[DS_EXTRALIFE_ICON_DATA], 0x668, 0x04, 0x10);
        }
        /* lab_5c36 */
        {
            uint16_t t = (uint16_t)(extralife_anim_step - 0x8);
            if (extralife_anim_step >= 0x8 && t < 0x6) {
                blit_to_cga(&ds_pool[DS_EXTRALIFE_TEXT_DATA],
                            ds_word((uint16_t)(DS_EXTRALIFE_TEXT_POS + t)), 0x06, 0x15);
            }
        }
    } while (extralife_anim_step < 0x10);

    silence_speaker();
}
