/* tandy_music.c — traduccion literal del motor de 3 voces (ver docs/TANDY_MOTOR.md).
 * Los desplazamientos DS: se conservan (M(off) = byte en DS:off). */
#include "tandy_music.h"
#include "sn76496.h"

extern const uint8_t tandy_ds_init[0x250];
static uint8_t ds[0x250];
#define BASE 0x1010
#define B(o) ds[(o) - BASE]
#define W(o) ((uint16_t)(B(o) | (B((o) + 1) << 8)))
#define SETW(o, v) do { B(o) = (uint8_t)(v); B((o) + 1) = (uint8_t)((uint16_t)(v) >> 8); } while (0)
/* estado de trabajo (9 bytes en 0x1228) */
#define W_DUR 0x1228
#define W_VOL 0x1229
#define W_POS 0x122a
#define W_CHN 0x122c
#define W_TRK 0x122d
#define W_DURP 0x122f
#define V_IDX 0x1252
#define LOOPCNT 0x1254

void (*tandy_port_hook)(uint16_t, uint8_t) = 0;
static bool playing = true;                    /* DS:[1] */
static uint8_t tandy_flag = 1;   /* DS:[0]: hay hardware 3 voces */

static void out_c0(uint8_t v) {
    if (tandy_port_hook) tandy_port_hook(0xC0, v);
    sn76496_write(v);
}
static void silence(void) {                    /* CS:0x1732 */
    uint8_t al = 0x9f;
    for (int i = 0; i < 4; i++) { out_c0(al); al = (uint8_t)(al + 0x20); }
}
void tandy_music_init(uint16_t tick) {         /* CS:0x16a0 */
    playing = true;
    for (unsigned i = 0; i < sizeof ds; i++) ds[i] = tandy_ds_init[i];
    if (tandy_flag) { silence(); silence(); }  /* 0x170c: silencio, int1a 8003, silencio */
    if (tandy_port_hook) { tandy_port_hook(0x43, 0x36); tandy_port_hook(0x40, 0xff); tandy_port_hook(0x40, 0xcf); }
    B(0x1231) = 0; B(0x1232) = 0x0f; SETW(0x1233, 0);
    B(0x123a) = 0; B(0x123b) = 0x0f; SETW(0x123c, 0);
    B(0x1243) = 0; B(0x1244) = 0x0f; SETW(0x1245, 0);
    SETW(LOOPCNT, 0);
    SETW(0x1012, tick); SETW(0x1010, tick);
}
void tandy_music_set_playing(bool on) { playing = on; }
bool tandy_music_toggle(void) { playing = !playing; return playing; }   /* not byte [1] */
uint16_t tandy_music_start_tick(void) { return W(0x1012); }
void tandy_music_silence(void) { silence(); }  /* CS:0x1732 */
void tandy_music_stop(void) {                  /* CS:0x171f */
    if (!tandy_flag) return;
    silence();
    if (tandy_port_hook) { /* int 1a 8000: sin efecto en el chip */ }
}

static void voice_update(uint16_t idx) {       /* CS:0x178d */
    uint8_t chn;
    if (B(W_DUR) != 0) {
        B(W_DUR)--;
        if (B(W_DUR) != 0) {
            if (B(W_VOL) >= 0x0f) goto sil;
            B(W_VOL) = (uint8_t)(B(W_VOL) + 2);
            out_c0((uint8_t)(B(W_VOL) | B(W_CHN) | 0x10));
            return;
        }
    }
next:;
    uint16_t pos = W(W_POS), trk = W(W_TRK), dp = W(W_DURP);
    B(W_DUR) = ds[(uint16_t)(pos + dp) - BASE];
    uint16_t bx = ds[(uint16_t)(pos + trk) - BASE];
    if (bx == 0x86) {
        SETW(W_POS, 0);
        if (idx == 0) SETW(LOOPCNT, W(LOOPCNT) + 2);
        goto next;
    }
    SETW(W_POS, pos + 1);
    if (bx == 0) goto sil;
    {
        uint16_t lim = W(0x1256 + (W(LOOPCNT) & 6));
        if (lim < idx) return;
    }
    B(W_VOL) = 3;
    out_c0((uint8_t)(3 | B(W_CHN) | 0x10));
    uint16_t f = W(0x1014 + bx);
    out_c0((uint8_t)((f & 0x0f) | B(W_CHN)));
    out_c0((uint8_t)((f >> 4) & 0x3f));
    return;
sil:
    chn = B(W_CHN);
    out_c0((uint8_t)(0x1f | chn));
}
void tandy_music_tick(uint16_t tick) {         /* CS:0x173e */
    if (!playing) return;
    if (tick == W(0x1010)) return;
    SETW(0x1010, tick);
    for (uint16_t idx = 0; idx <= 4; idx += 2) {
        SETW(V_IDX, idx);
        uint16_t p = W(0x124c + idx);
        for (int i = 0; i < 9; i++) B(W_DUR + i) = ds[p + i - BASE];
        voice_update(idx);
        p = W(0x124c + idx);
        for (int i = 0; i < 9; i++) ds[p + i - BASE] = B(W_DUR + i);
    }
}
