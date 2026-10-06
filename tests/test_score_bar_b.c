/* T49 — animate_score_bar, binary_to_bcd (level_objects.asm L1317-1367). Sin SDL. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "score.h"
#include "sound.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint16_t fake_tick, tick_calls;
static uint16_t tickfn(void) { tick_calls++; return fake_tick++; }   /* avanza 1 tick por lectura: la espera de 2 ticks termina */
static int steps, silences;
void __wrap_play_melody_step(void) { steps++; }
void __wrap_silence_speaker(void) { silences++; }

/* BCD directo: 7 digitos MSB-first */
static void direct_bcd(uint32_t v, uint8_t out[7]) { for (int i = 6; i >= 0; i--) { out[i] = (uint8_t)(v % 10); v /= 10; } }

int main(void) {
    game_tick_fn = tickfn;

    /* ---- binary_to_bcd: solo cuenta los bits 12..0 (los bits 13-15 se ignoran, como el bucle del original) */
    uint32_t vals[] = { 0, 1, 9, 10, 99, 100, 255, 4096, 8191, 0x2000, 0xffff, 12345 };
    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        uint8_t want[7]; direct_bcd(vals[i] & 0x1fff, want);
        memset(bonus_bcd, 0xee, sizeof bonus_bcd);
        binary_to_bcd((uint16_t)vals[i]);
        CHECK(memcmp(bonus_bcd, want, 7) == 0 && bonus_bcd[7] == 0, "binary_to_bcd(%u)", (unsigned)vals[i]);
    }
    /* los valores < 8192 coinciden con el calculo directo sin enmascarar */
    for (uint32_t v = 0; v < 8192; v += 37) {
        uint8_t want[7]; direct_bcd(v, want);
        binary_to_bcd((uint16_t)v);
        CHECK(memcmp(bonus_bcd, want, 7) == 0, "binary_to_bcd barrido %u", (unsigned)v);
    }

    /* ---- animate_score_bar sin melodia: dibuja posiciones 0x1b80, 0x1900, ... mientras pos-0x280 >= limite */
    for (int i = 0; i < 60; i++) score_tiles[i] = (uint8_t)(0x40 + i);
    memset(cga_mem, 0, sizeof cga_mem);
    bonus_bar_flag = 0; steps = silences = 0; tick_calls = 0;
    animate_score_bar(0x1000);
    CHECK(silences == 1 && steps == 0 && tick_calls == 0, "sin melodia: silence=%d pasos=%d ticks=%u", silences, steps, tick_calls);
    /* la primera posicion (0x1b80) y la ultima dibujada (>= 0x1000: 0x1b80 - 0x280*k) tienen el primer bloque en +6 */
    CHECK(cga_mem[0x1b80 + 6] == 0x40, "bloque en 0x1b80+6 = %02x", cga_mem[0x1b80 + 6]);
    CHECK(cga_mem[0x1b80 - 0x280 * 3 + 6] == 0x40, "bloque 4a posicion");
    CHECK(cga_mem[0x1b80 + 0x45] == 0x40, "cuarto destino +0x45");
    CHECK(cga_mem[0x1b80 + 6 + 5] == 0, "no se pasa de 5 bytes por fila");
    /* 0x1b80 - 0x280*k >= 0x1000 -> k <= 4: ultima posicion dibujada 0x1b80-0xa00 = 0x1180 */
    CHECK(cga_mem[0x1180 + 6] == 0x40, "ultima posicion 0x1180");
    CHECK(cga_mem[0x1180 - 0x280 + 6] == 0, "no dibuja bajo el limite");

    /* ---- con melodia: un paso y una espera por posicion dibujada */
    memset(cga_mem, 0, sizeof cga_mem);
    bonus_bar_flag = 1; steps = silences = 0; tick_calls = 0; fake_tick = 0xfffe;   /* cruza el wrap de 16 bits */
    animate_score_bar(0x1000);
    CHECK(steps == 5 && silences == 1, "con melodia: pasos=%d (esperado 5) silence=%d", steps, silences);
    CHECK(tick_calls >= 5 * 3, "espera de 2 ticks por paso (lecturas %u)", tick_calls);

    /* ---- limite alto: una sola posicion; limite 0: baja hasta antes de que la resta desborde */
    memset(cga_mem, 0, sizeof cga_mem); bonus_bar_flag = 0; steps = silences = 0;
    animate_score_bar(0x1b80);
    CHECK(cga_mem[0x1b80 + 6] == 0x40 && cga_mem[0x1b80 - 0x280 + 6] == 0, "limite 0x1b80: una sola posicion");
    memset(cga_mem, 0, sizeof cga_mem);
    animate_score_bar(0);
    CHECK(cga_mem[0x1b80 - 0x280 * 11 + 6] == 0x40, "limite 0: 12 posiciones (0x1b80..0x0180+...)");
    CHECK(silences == 2, "termina y silencia (%d)", silences);

    if (fails) { printf("test_score_bar_b: %d FALLOS\n", fails); return 1; }
    printf("test_score_bar_b: OK\n");
    return 0;
}
