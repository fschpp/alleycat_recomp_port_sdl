/* T39 — draw_love_scene_bg / draw_bg_tile (level_objects.asm L209-299). Sin SDL.
 * Modelo independiente desde el ASM: LFSR propio (para contar las llamadas a random), pantalla modelo con bancos CGA, block list leido
 * del DS, 7 filas x 15 columnas de tiles, tabla window_open_state y los corazones iniciales segun [0x414]. Ver PROGRESS.md §6aj. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level_background.h"
#include "level7_epilogue.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint16_t mseed;
static uint8_t m_random(void) {
    uint8_t lo = (uint8_t)(mseed & 0xff), hi = (uint8_t)(mseed >> 8);
    unsigned c = (unsigned)(((lo ^ hi) >> 1) & 1);
    mseed = (uint16_t)((mseed >> 1) | (c << 15));
    return (uint8_t)(mseed & 0xff);
}
static uint8_t model[CGA_MEM_SIZE];
static size_t rowoff(uint16_t S, int r) {
    int b0 = (S >> 13) & 1;
    return (size_t)(S & 0x1fff) + (size_t)(((r + b0) >> 1) * 80) + (size_t)(((b0 ^ (r & 1)) & 1) * 0x2000);
}
static uint16_t caddr(uint8_t row, uint16_t x) { return (uint16_t)((row & 1) * 0x2000 + (row >> 1) * 80 + (x >> 2)); }
static uint16_t dsw(uint16_t o) { return (uint16_t)(ds_pool[o] | (ds_pool[o + 1] << 8)); }
static void put(uint16_t src, uint16_t at, int bytes, int rows) {
    for (int r = 0; r < rows; r++) memcpy(&model[rowoff(at, r)], &ds_pool[src + r * bytes], (size_t)bytes);
}

static uint8_t m_open[126]; static uint8_t m_active[8]; static uint8_t m_y[8]; static uint16_t m_x[8]; static uint16_t m_counter_out;
static void build_model(uint16_t counter) {
    memcpy(model, cga_mem, CGA_MEM_SIZE);
    /* block list 0x2e24: {dims: cols(bytes)=lo, rows=hi}, {src,dst}..., 0xffff */
    uint16_t l = 0x2e24, dims = dsw(l); int cols = dims & 0xff, rows = dims >> 8; l += 2;
    while (dsw(l) != 0xffff) { put(dsw(l), dsw((uint16_t)(l + 2)), cols, rows); l += 4; }
    memset(m_open, 0xAA, sizeof m_open);
    int row = 0;
    for (int y = 0xbf; y >= 0x2f; y -= 0x18, row++) {
        for (int x = 0x20; x < 0x111; x += 0x10) {
            int bl = 0;
            if (y != 0xbf) bl = m_random() & 2;
            put(dsw((uint16_t)(0x2e20 + bl)), caddr((uint8_t)y, (uint16_t)x), 4, 8);
            int col = (x >> 4) - 2; if (col < 0) col = 0; if (col > 0x11) col = 0x11;
            m_open[row * 18 + col] = (uint8_t)bl;                    /* window_row_col_offset[row] = row*18 */
        }
    }
    memset(m_active, 0, 8);
    m_counter_out = counter ? counter : 1;
    int n = m_counter_out > 8 ? 8 : m_counter_out;
    for (int k = n - 1; k >= 0; k--) {
        m_active[k] = 1; m_y[k] = 0xb0; m_x[k] = dsw((uint16_t)(0x2b4a + 2 * k));
        put(0x2af0, caddr(0xb0, m_x[k]), 6, 15);
    }
}

static void run_case(uint16_t seed, uint16_t counter, bool via_dispatch) {
    for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 5 + 1);
    memset(window_open_state, 0xAA, 126);
    memset(l7_obj_active, 0x55, 8); l7_obj_spawn_slot = 3; l7_obj_last_picked = 4; memset(l7_obj_y, 0, 8); memset(l7_obj_x, 0, sizeof l7_obj_x);
    l7_completion_counter = counter;
    rng_seed = seed; mseed = seed;
    build_model(counter);
    if (via_dispatch) { level_number = 7; draw_level_background(); } else draw_love_scene_bg();
    CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) == 0, "seed=%04x counter=%u: pantalla distinta", seed, counter);
    CHECK(rng_seed == mseed, "seed=%04x: cantidad de llamadas a random distinta (%04x vs %04x)", seed, rng_seed, mseed);
    CHECK(memcmp(window_open_state, m_open, 126) == 0, "seed=%04x: window_open_state", seed);
    CHECK(l7_obj_spawn_slot == 0xffff && l7_obj_last_picked == 0xffff, "spawn_slot/last_picked");
    CHECK(l7_completion_counter == m_counter_out, "counter=%u -> %u (esperado %u)", counter, l7_completion_counter, m_counter_out);
    int n = m_counter_out > 8 ? 8 : m_counter_out;
    for (int k = 0; k < 8; k++) {
        CHECK(l7_obj_active[k] == m_active[k], "counter=%u: active[%d]=%u", counter, k, l7_obj_active[k]);
        if (k < n) CHECK(l7_obj_y[k] == 0xb0 && (uint16_t)l7_obj_x[k] == m_x[k], "counter=%u: obj %d (%u,%u)", counter, k, l7_obj_x[k], l7_obj_y[k]);
    }
}

int main(void) {
    static const uint16_t COUNTERS[] = {0, 1, 2, 3, 5, 7, 8, 9, 20, 0xffff};
    int n = 0;
    for (int s = 0; s < 12; s++) for (size_t c = 0; c < sizeof COUNTERS / sizeof *COUNTERS; c++) {
        run_case((uint16_t)(0x0123 + s * 0x1357), COUNTERS[c], false); n++;
    }
    run_case(0xFA59, 4, true); n++;                                  /* mismo resultado via draw_level_background con level_number==7 */

    /* draw_bg_tile suelto: tile 0 y tile 2 en (x=0x40, y=0x50) */
    for (int bl = 0; bl <= 2; bl += 2) {
        for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 3);
        memcpy(model, cga_mem, CGA_MEM_SIZE);
        put(dsw((uint16_t)(0x2e20 + bl)), caddr(0x50, 0x40), 4, 8);
        draw_bg_tile(0x40, 0x50, (uint16_t)bl);
        CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) == 0, "draw_bg_tile bl=%d", bl);
    }
    if (fails) { printf("test_level7c: %d FALLOS\n", fails); return 1; }
    printf("test_level7c: OK (%d escenarios)\n", n);
    return 0;
}
