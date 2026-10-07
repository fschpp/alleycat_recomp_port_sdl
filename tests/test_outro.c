/* T58 — love_scene_outro (ui.asm L489-570). Sin SDL. Los puertos del altavoz se interceptan con --wrap. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "score.h"
#include "ui.h"
#include "input.h"
#include "speaker.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* registro de puertos: 'C' = out 0x43, 'D' = out 0x42, 'S' = out 0x61 */
static struct { char k; uint8_t v; uint16_t tick; } log_[4096]; static int nlog;
static uint16_t now;
static void rec(char k, uint8_t v) { if (nlog < 4096) { log_[nlog].k = k; log_[nlog].v = v; log_[nlog].tick = now; nlog++; } }
void __real_pit_out_43(uint8_t); void __real_pit_ch2_out(uint8_t); void __real_port61_out(uint8_t);
void __wrap_pit_out_43(uint8_t v) { rec('C', v); __real_pit_out_43(v); }
void __wrap_pit_ch2_out(uint8_t v) { rec('D', v); __real_pit_ch2_out(v); }
void __wrap_port61_out(uint8_t v) { rec('S', v); __real_port61_out(v); }

static uint16_t tickfn(void) { return now; }
static int iter, step;                       /* step = ticks por vuelta de espera */
static void hook(void) { iter++; now = (uint16_t)(now + step); }

/* referencia independiente del layout CGA entrelazado: fila r en (r&1)*0x2000 + (r>>1)*80 respecto a la fila 0 */
static void ref_blit(uint8_t *mem, size_t off, const uint8_t *src, int w_words, int h) {
    size_t row0 = off % 0x2000 / 80;               /* fila de banco de la base */
    size_t col0 = off % 0x2000 % 80;
    size_t bank0 = off / 0x2000;
    size_t r0 = row0 * 2 + bank0;                  /* fila de pantalla */
    for (int r = 0; r < h; r++) {
        size_t R = r0 + (size_t)r;
        size_t o = (R & 1) * 0x2000 + (R >> 1) * 80 + col0;
        memcpy(&mem[o], src + (size_t)r * (size_t)w_words * 2, (size_t)w_words * 2);
    }
}

static void run(int snd, int stp, uint16_t t0) {
    memset(cga_mem, 0, sizeof cga_mem);
    nlog = 0; iter = 0; step = stp; now = t0; sound_enabled = snd ? 0xff : 0x00;
    title_scroll_tick_1 = (uint16_t)(t0 - 5); title_scroll_tick_2 = (uint16_t)(t0 - 9);
    love_scene_outro();
}

int main(void) {
    game_tick_fn = tickfn; ui_wait_hook = hook; set_bios_tick(0); speaker_reset();

    /* datos: tamanos derivados de las etiquetas contiguas del DS */
    CHECK(0x6e58 - 0x6e10 == 3 * 2 * 12, "dat_6e10 = 3 words x 12 filas");
    CHECK(0x6f24 - 0x6e58 == 6 * 2 * 17, "dat_6e58 = 6 words x 17 filas");

    for (int snd = 0; snd < 2; snd++) {
        run(snd, 1, 0x4000);
        /* 14 pasos: 0x25 + 14*0x1e0 = 0x1a65 >= 0x1a40 (con 13 seria 0x1885) */
        CHECK(title_scroll_pos == 0x1a65, "snd=%d: pos final %04x", snd, title_scroll_pos);
        /* pantalla final: solo queda el ultimo sprite (los anteriores se borraron) con la imagen final encima */
        static uint8_t ref[sizeof cga_mem];
        memset(ref, 0, sizeof ref);
        ref_blit(ref, 0x1a65, &ds_pool[0x6e10], 3, 12);
        ref_blit(ref, 0x1a65, &ds_pool[0x6e58], 6, 17);
        CHECK(memcmp(cga_mem, ref, sizeof ref) == 0, "snd=%d: pantalla final = referencia independiente", snd);
        /* 14 ticks de fase 1 + 18 de fase 2 desde el ultimo tick de la fase 1 */
        CHECK(now == (uint16_t)(0x4000 + 14 + 18), "snd=%d: duracion exacta 14 + 18 ticks (now=%04x)", snd, now);
        CHECK(title_scroll_tick_1 == (uint16_t)(0x4000 + 14), "snd=%d: tick_1 = ultimo tick de la fase 1 (%04x)", snd, title_scroll_tick_1);
        CHECK(title_scroll_tick_2 == (uint16_t)(0x4000 + 14 + 18), "snd=%d: tick_2 (%04x)", snd, title_scroll_tick_2);

        if (!snd) {
            /* sin sonido: ni PIT ni tonos; solo el silence_speaker final (out 0x61 con bits 0-1 a 0) */
            CHECK(nlog == 1 && log_[0].k == 'S' && (log_[0].v & 3) == 0, "sin sonido solo silence_speaker (%d escrituras)", nlog);
            continue;
        }
        /* fase 1: 14 tonos con divisor = pos>>1, pos = 0x25 + k*0x1e0; luego fase 2: 18 tonos alternos; al final silencio */
        int i = 0;
        for (int k = 1; k <= 14; k++) {
            uint16_t div = (uint16_t)((0x25 + k * 0x1e0) >> 1);
            CHECK(i + 4 <= nlog && log_[i].k == 'C' && log_[i].v == 0xb6, "f1 k=%d: out 0x43,0xb6", k);
            CHECK(log_[i+1].k == 'D' && log_[i+1].v == (div & 0xff), "f1 k=%d: lo %02x != %02x", k, log_[i+1].v, div & 0xff);
            CHECK(log_[i+2].k == 'D' && log_[i+2].v == (div >> 8), "f1 k=%d: hi %02x != %02x", k, log_[i+2].v, div >> 8);
            CHECK(log_[i+3].k == 'S' && (log_[i+3].v & 3) == 3, "f1 k=%d: altavoz activado (61=%02x)", k, log_[i+3].v);
            i += 4;
        }
        for (int k = 1; k <= 18; k++) {
            uint16_t t = (uint16_t)(0x4000 + 14 + k);
            uint16_t div = (t & 1) ? 0xb54 : 0xc00;
            CHECK(i + 3 <= nlog && log_[i].k == 'C' && log_[i].v == 0xb6, "f2 k=%d: out 0x43,0xb6", k);
            CHECK(log_[i+1].k == 'D' && log_[i+1].v == (div & 0xff), "f2 k=%d: lo %02x (tick %04x)", k, log_[i+1].v, t);
            CHECK(log_[i+2].k == 'D' && log_[i+2].v == (div >> 8), "f2 k=%d: hi %02x (tick %04x)", k, log_[i+2].v, t);
            i += 3;
        }
        CHECK(i < nlog && log_[nlog-1].k == 'S' && (log_[nlog-1].v & 3) == 0, "silence_speaker al final (61=%02x)", log_[nlog-1].v);
        CHECK(i + 1 == nlog || i + 2 == nlog, "sin escrituras de mas (%d de %d)", i, nlog);
    }

    /* tick que avanza de a 3 por vuelta y cruza el wrap de 16 bits: una pasada por paso (la espera exige tick != anterior) */
    run(1, 3, 0xfff0);
    CHECK(title_scroll_pos == 0x1a65, "ticks de a 3 con wrap: pos %04x", title_scroll_pos);
    CHECK((uint16_t)(now - 0xfff0) >= 14 * 3 + 18, "ticks de a 3: duracion %u", (unsigned)(uint16_t)(now - 0xfff0));
    /* fase 2 usa resta sin signo: tick_2 - tick_1 >= 0x12 aun cruzando 0xffff */
    CHECK((uint16_t)(title_scroll_tick_2 - title_scroll_tick_1) >= 0x12, "fase 2 termina con diferencia sin signo");

    if (!fails) printf("test_outro: OK\n");
    return fails;
}
