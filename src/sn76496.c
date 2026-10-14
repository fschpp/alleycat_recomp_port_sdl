#include "sn76496.h"
#define RATE 44100.0
#define CHIP_STEP (3579545.0 / 32.0)
static volatile int lock;
static void lk(void) { while (__atomic_test_and_set(&lock, __ATOMIC_ACQUIRE)) {} }
static void ul(void) { __atomic_clear(&lock, __ATOMIC_RELEASE); }
static struct { uint16_t tone[3]; uint8_t vol[4]; uint8_t noise; uint8_t latch; double cnt[4]; int out[4]; uint16_t lfsr; double acc; } c;
static const int amp[16] = {8000,6355,5048,4010,3185,2530,2009,1596,1268,1007,800,635,504,400,318,0};
void sn76496_reset(void) {
    lk();
    for (int i = 0; i < 3; i++) { c.tone[i] = 0; c.out[i] = 1; c.cnt[i] = 0; }
    for (int i = 0; i < 4; i++) c.vol[i] = 15;
    c.noise = 0; c.lfsr = 0x8000; c.out[3] = 1; c.cnt[3] = 0; c.acc = 0;
    ul();
}
void sn76496_write(uint8_t v) {
    lk();
    if (v & 0x80) {
        c.latch = (v >> 4) & 7;
        int ch = c.latch >> 1;
        if (c.latch & 1) c.vol[ch] = v & 15;
        else if (ch < 3) c.tone[ch] = (c.tone[ch] & 0x3f0) | (v & 15);
        else { c.noise = v & 7; c.lfsr = 0x8000; }
    } else {
        int ch = c.latch >> 1;
        if (c.latch & 1) c.vol[ch] = v & 15;
        else if (ch < 3) c.tone[ch] = (uint16_t)((c.tone[ch] & 0x0f) | ((v & 0x3f) << 4));
    }
    ul();
}
static double noise_period(void) {
    int n = c.noise & 3;
    if (n == 3) return c.tone[2] ? c.tone[2] : 1;
    return (double)(16 << n);
}
void sn76496_mix(int16_t *out, size_t frames) {
    lk();
    for (size_t i = 0; i < frames; i++) {
        c.acc += CHIP_STEP / RATE;
        while (c.acc >= 1.0) {
            c.acc -= 1.0;
            for (int ch = 0; ch < 3; ch++) {
                if (--c.cnt[ch] <= 0) { c.cnt[ch] += c.tone[ch] ? c.tone[ch] : 1; c.out[ch] = -c.out[ch]; }
            }
            if (--c.cnt[3] <= 0) {
                c.cnt[3] += noise_period(); c.out[3] = -c.out[3];
                if (c.out[3] > 0) {
                    int fb = (c.noise & 4) ? ((c.lfsr ^ (c.lfsr >> 3)) & 1) : (c.lfsr & 1);
                    c.lfsr = (uint16_t)((c.lfsr >> 1) | (fb << 14));
                }
            }
        }
        int s = 0;
        for (int ch = 0; ch < 3; ch++) s += c.out[ch] * amp[c.vol[ch]];
        s += ((c.lfsr & 1) ? 1 : -1) * amp[c.vol[3]];
        s += out[i];
        out[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
    }
    ul();
}
