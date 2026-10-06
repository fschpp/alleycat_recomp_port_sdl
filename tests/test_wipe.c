/* T45 — animate_screen_wipe (enemy.asm L159-233). Sin SDL. Las trazas esperadas salen de un modelo
 * independiente en Python escrito desde el ASM (no del C). PROGRESS.md §6ap. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

extern uint16_t wipe_width, wipe_rect_x; extern uint8_t wipe_rect_y, wipe_height, wipe_edge_flags;
static int got[64][5]; static int ngot;
static void rec(void) {
    if (ngot < 64) { int *g = got[ngot++]; g[0] = wipe_rect_x; g[1] = wipe_rect_y; g[2] = wipe_width; g[3] = wipe_height; g[4] = wipe_edge_flags; }
}
static size_t row_off(int r) { return (size_t)(r & 1) * 0x2000 + (size_t)(r >> 1) * 80; }

static const int exp0[][5] = {
    {160,104,1,8,0},
    {144,96,33,24,0},
    {128,88,65,40,0},
    {112,80,97,56,0},
    {96,72,129,72,0},
    {80,64,161,88,0},
    {64,56,193,104,0},
    {48,48,225,120,0},
    {32,40,257,136,0},
    {16,32,289,152,0},
    {0,24,320,168,2},
    {0,16,320,184,11},
    {0,8,320,192,11},
    {0,0,320,200,11},
    {0,0,320,200,15}
};
static const int exp1[][5] = {
    {0,188,1,8,0},
    {0,180,33,20,9},
    {0,172,65,28,9},
    {0,164,97,36,9},
    {0,156,129,44,9},
    {0,148,161,52,9},
    {0,140,193,60,9},
    {0,132,225,68,9},
    {0,124,257,76,9},
    {0,116,289,84,9},
    {0,108,320,92,11},
    {0,100,320,100,11},
    {0,92,320,108,11},
    {0,84,320,116,11},
    {0,76,320,124,11},
    {0,68,320,132,11},
    {0,60,320,140,11},
    {0,52,320,148,11},
    {0,44,320,156,11},
    {0,36,320,164,11},
    {0,28,320,172,11},
    {0,20,320,180,11},
    {0,12,320,188,11},
    {0,4,320,196,11},
    {0,0,320,200,15}
};
static const int exp2[][5] = {
    {304,103,1,8,0},
    {288,95,32,24,2},
    {272,87,48,40,2},
    {256,79,64,56,2},
    {240,71,80,72,2},
    {224,63,96,88,2},
    {208,55,112,104,2},
    {192,47,128,120,2},
    {176,39,144,136,2},
    {160,31,160,152,2},
    {144,23,176,168,2},
    {128,15,192,184,2},
    {112,7,208,193,10},
    {96,0,224,200,14},
    {80,0,240,200,14},
    {64,0,256,200,14},
    {48,0,272,200,14},
    {32,0,288,200,14},
    {16,0,304,200,14},
    {0,0,320,200,14},
    {0,0,320,200,15}
};

#define TRACE(idx, cx_, cy_) do { \
    memset(cga_mem, 0xff, sizeof cga_mem); cat_x = (cx_); cat_y = (cy_); wipe_fill_pattern = 0x5555; ngot = 0; \
    wipe_step_hook = rec; animate_screen_wipe(); \
    int n_ = (int)(sizeof exp##idx / sizeof exp##idx[0]); \
    CHECK(ngot == n_, "caso %d: %d pasos, esperaba %d", idx, ngot, n_); \
    for (int i_ = 0; i_ < ngot && i_ < n_; i_++) \
        CHECK(memcmp(got[i_], exp##idx[i_], sizeof got[i_]) == 0, "caso %d paso %d: %d,%d,%d,%d,f=%d", idx, i_, got[i_][0], got[i_][1], got[i_][2], got[i_][3], got[i_][4]); \
    int bad_ = 0; for (int r_ = 0; r_ < 200; r_++) for (int b_ = 0; b_ < 80; b_++) if (cga_mem[row_off(r_) + b_] != 0x55) bad_++; \
    CHECK(bad_ == 0, "caso %d: %d bytes visibles sin rellenar al final", idx, bad_); } while (0)

/* hook que comprueba el rectangulo tras el paso 2 del caso (0xa0,0x60): x=144..175 (4 palabras), filas 96..119 */
static int step; static int bad_in, bad_out;
static void check_step(void) {
    step++;
    if (step == 1) { for (int r = 0; r < 200; r++) for (int b = 0; b < 80; b++) if (cga_mem[row_off(r) + b] != 0xff) bad_out++; }  /* ancho 1 -> 0 palabras */
    if (step == 2) {
        for (int r = 0; r < 200; r++) for (int b = 0; b < 80; b++) {
            bool in = r >= 96 && r < 120 && b >= 36 && b < 44;      /* x 144/4=36, 8 bytes */
            uint8_t v = cga_mem[row_off(r) + b];
            if (in && v != 0x55) bad_in++;
            if (!in && v != 0xff) bad_out++;
        }
    }
}

int main(void) {
    TRACE(0, 0xa0, 0x60);
    TRACE(1, 0x0, 0xb4);
    TRACE(2, 0x128, 0x5f);

    memset(cga_mem, 0xff, sizeof cga_mem); cat_x = 0xa0; cat_y = 0x60; wipe_fill_pattern = 0x5555;
    step = 0; bad_in = bad_out = 0; wipe_step_hook = check_step; animate_screen_wipe();
    CHECK(bad_out == 0, "paso 1/2: %d bytes fuera del rectangulo tocados", bad_out);
    CHECK(bad_in == 0, "paso 2: %d bytes del rectangulo sin rellenar", bad_in);

    /* patron de 2 bytes distintos: se escribe low/high en ese orden (stosw) */
    memset(cga_mem, 0, sizeof cga_mem); cat_x = 0xa0; cat_y = 0x60; wipe_fill_pattern = 0xaa55; wipe_step_hook = 0;
    animate_screen_wipe();
    CHECK(cga_mem[0] == 0x55 && cga_mem[1] == 0xaa && cga_mem[row_off(199) + 78] == 0x55 && cga_mem[row_off(199) + 79] == 0xaa, "orden de bytes del patron");

    if (fails) { printf("%d fallos\n", fails); return 1; }
    printf("test_wipe: OK\n");
    return 0;
}
