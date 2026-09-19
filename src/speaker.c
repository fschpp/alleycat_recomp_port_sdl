/* speaker.c — PC speaker / 8253 PIT emulation. See include/speaker.h and
 * PROGRESS.md §6e for the model and its documented simplifications. */

#include "speaker.h"

#include <stdatomic.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------- clock */

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* ------------------------------------------------------- port-write log */

/* One recorded speaker state change. The audio side replays these against
 * their timestamps, which is what makes the direct-PWM effects (explosion,
 * hiss, noise, buzz) actually audible instead of being flattened to
 * whatever state happened to be latched when the callback ran. */
typedef struct {
    uint64_t t_ns;
    uint16_t divisor;   /* PIT ch2 reload value; 0 means 65536 */
    uint8_t  port61;    /* bit0 = ch2 gate, bit1 = speaker data */
} spk_event_t;

#define SPK_LOG_SIZE 16384u          /* power of two */
#define SPK_LOG_MASK (SPK_LOG_SIZE - 1u)

static spk_event_t      spk_log[SPK_LOG_SIZE];
static _Atomic uint32_t spk_head;     /* producer: game thread */
static _Atomic uint32_t spk_tail;     /* consumer: audio thread */

/* Live hardware state (game thread only). */
static uint16_t pit_ch2_divisor = 0;
static uint16_t pit_ch2_latch   = 0;
static int      pit_ch2_phase   = 0;  /* 0 = expect lo byte, 1 = expect hi */
static uint8_t  port61_value    = 0;

static void spk_push(void) {
    uint32_t head = atomic_load_explicit(&spk_head, memory_order_relaxed);
    spk_log[head & SPK_LOG_MASK] = (spk_event_t){
        .t_ns    = now_ns(),
        .divisor = pit_ch2_divisor,
        .port61  = port61_value,
    };
    atomic_store_explicit(&spk_head, head + 1u, memory_order_release);
}

void speaker_reset(void) {
    pit_ch2_divisor = 0;
    pit_ch2_latch   = 0;
    pit_ch2_phase   = 0;
    port61_value    = 0;
    atomic_store_explicit(&spk_tail, atomic_load_explicit(&spk_head, memory_order_relaxed),
                          memory_order_release);
    spk_push();
}

/* ------------------------------------------------------------ port I/O */

void pit_out_43(uint8_t control) {
    /* The game only ever writes 0xB6 (channel 2, access lo/hi, mode 3) and
     * 0x00 (latch channel 0, from read_pit_timer). Anything selecting
     * channel 2 restarts the two-byte divisor write sequence. */
    if ((control & 0xC0) == 0x80) pit_ch2_phase = 0;
}

void pit_ch2_out(uint8_t data) {
    if (pit_ch2_phase == 0) {
        pit_ch2_latch = data;
        pit_ch2_phase = 1;
    } else {
        pit_ch2_divisor = (uint16_t)(pit_ch2_latch | ((uint16_t)data << 8));
        pit_ch2_phase = 0;
        spk_push();
    }
}

uint8_t port61_in(void) {
    return port61_value;
}

void port61_out(uint8_t value) {
    port61_value = value;
    spk_push();
}

uint16_t read_pit_timer(void) {
    /* Channel 0 free-runs with a 65536 reload and counts DOWN, so elapsed
     * time is (prev - now) as a 16-bit wrap — exactly the arithmetic
     * play_timed_tone/play_crash_sound/play_falling_sound rely on. */
    uint64_t ticks = (uint64_t)((double)now_ns() * ((double)PIT_INPUT_HZ / 1e9));
    return (uint16_t)(0u - (uint16_t)(ticks & 0xFFFFu));
}

void speaker_spin_cycles(uint32_t cycles) {
    uint64_t target = now_ns() + (uint64_t)((double)cycles * 1e9 / (double)EMU_CPU_HZ);
    for (;;) {
        uint64_t t = now_ns();
        if (t >= target) return;
        uint64_t remain = target - t;
        if (remain > 150000ull) {
            struct timespec ts = { .tv_sec = 0, .tv_nsec = (long)(remain - 100000ull) };
            nanosleep(&ts, NULL);
        }
    }
}

uint16_t speaker_current_divisor(void) { return pit_ch2_divisor; }
uint8_t  speaker_current_port61(void)  { return port61_value; }

/* ------------------------------------------------------------ rendering */

/* Square-wave generator state (audio thread only). */
static double   gen_phase_ticks = 0.0;  /* position inside the ch2 period */
static uint16_t gen_divisor     = 0;
static uint8_t  gen_port61      = 0;
static double   gen_lowpass     = 0.0;

#define SPK_AMPLITUDE 7000.0
/* One-pole low-pass roughly modelling the speaker cone, which cannot
 * reproduce a mathematically sharp square edge. Also takes the worst of the
 * aliasing off the direct-PWM effects. */
#define SPK_LOWPASS_A 0.35

static int16_t gen_sample(void) {
    double level;
    if ((gen_port61 & 0x02) == 0) {
        level = 0.0;                     /* data bit low: cone at rest */
    } else if ((gen_port61 & 0x01) == 0) {
        level = 1.0;                     /* gate low: ch2 OUT idles high,
                                          * so the cone follows the data bit
                                          * — this is the direct-PWM path */
    } else {
        uint32_t div = gen_divisor ? gen_divisor : 65536u;
        double half = (double)div * 0.5;
        level = (gen_phase_ticks < half) ? 1.0 : 0.0;
    }
    gen_lowpass += SPK_LOWPASS_A * (level - gen_lowpass);
    return (int16_t)((gen_lowpass - 0.5) * 2.0 * SPK_AMPLITUDE);
}

static void gen_advance(void) {
    uint32_t div = gen_divisor ? gen_divisor : 65536u;
    gen_phase_ticks += (double)PIT_INPUT_HZ / (double)SPEAKER_SAMPLE_RATE;
    while (gen_phase_ticks >= (double)div) gen_phase_ticks -= (double)div;
}

/* Renders [from_ns, from_ns + frames*sample_period) by applying every
 * logged event whose timestamp has been reached. Consumes from spk_tail. */
static void render_span(uint64_t from_ns, int16_t *out, size_t frames) {
    const double ns_per_sample = 1e9 / (double)SPEAKER_SAMPLE_RATE;
    uint32_t tail = atomic_load_explicit(&spk_tail, memory_order_relaxed);

    for (size_t i = 0; i < frames; i++) {
        uint64_t t = from_ns + (uint64_t)((double)i * ns_per_sample);
        uint32_t head = atomic_load_explicit(&spk_head, memory_order_acquire);
        /* Drop anything already overrun by the producer. */
        if ((uint32_t)(head - tail) > SPK_LOG_SIZE) tail = head - SPK_LOG_SIZE;
        while (tail != head && spk_log[tail & SPK_LOG_MASK].t_ns <= t) {
            const spk_event_t *e = &spk_log[tail & SPK_LOG_MASK];
            gen_divisor = e->divisor;
            gen_port61  = e->port61;
            tail++;
        }
        out[i] = gen_sample();
        gen_advance();
    }

    atomic_store_explicit(&spk_tail, tail, memory_order_release);
}

/* Wall-clock position the live renderer is currently emitting. Trails real
 * time by RENDER_LATENCY_NS so the events for a window are already logged
 * by the time that window is rendered. */
static uint64_t render_ns = 0;
#define RENDER_LATENCY_NS  40000000ull   /* 40 ms */
#define RENDER_RESYNC_NS  200000000ull   /* 200 ms of drift forces a resync */

size_t speaker_render(int16_t *out, size_t frames) {
    const double ns_per_sample = 1e9 / (double)SPEAKER_SAMPLE_RATE;
    uint64_t t = now_ns();

    if (render_ns == 0 || t > render_ns + RENDER_RESYNC_NS ||
        render_ns > t + RENDER_RESYNC_NS) {
        render_ns = (t > RENDER_LATENCY_NS) ? t - RENDER_LATENCY_NS : 0;
    }

    render_span(render_ns, out, frames);
    render_ns += (uint64_t)((double)frames * ns_per_sample);
    return frames;
}

void speaker_capture_begin(void) {
    atomic_store_explicit(&spk_head, 0u, memory_order_relaxed);
    atomic_store_explicit(&spk_tail, 0u, memory_order_relaxed);
    gen_phase_ticks = 0.0;
    gen_lowpass     = 0.0;
    gen_divisor     = 0;
    gen_port61      = 0;
    render_ns       = 0;
    spk_push();
}

size_t speaker_capture_render(int16_t *out, size_t max_frames) {
    const double ns_per_sample = 1e9 / (double)SPEAKER_SAMPLE_RATE;
    uint32_t head = atomic_load_explicit(&spk_head, memory_order_acquire);
    if (head == 0) return 0;
    if (head > SPK_LOG_SIZE) head = SPK_LOG_SIZE;

    uint64_t t0 = spk_log[0].t_ns;
    uint64_t t1 = spk_log[(head - 1) & SPK_LOG_MASK].t_ns;
    size_t frames = (size_t)((double)(t1 - t0) / ns_per_sample);
    if (frames > max_frames) frames = max_frames;

    atomic_store_explicit(&spk_tail, 0u, memory_order_release);
    gen_phase_ticks = 0.0;
    gen_lowpass     = 0.0;
    render_span(t0, out, frames);
    return frames;
}
