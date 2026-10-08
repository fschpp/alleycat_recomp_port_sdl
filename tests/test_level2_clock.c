/* T81: la pecera (nivel 2) no se quedaba: setup_level ponia level2_tick = 0 en vez de leer el tick BIOS, y con el reloj real
 * (grande) object_hit se activaba en el 1.er frame y se volvia al callejon. Se prueba con un reloj que parte de 40000 y con
 * uno que parte cerca del desborde de 16 bits. Uso: make test-level2-clock */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "animation_entry.h"
#include "score.h"
#include "bios_clock.h"
int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
void __wrap_speaker_spin_cycles(uint32_t c) { (void)c; }
uint16_t __wrap_read_bios_tick(void) { static uint16_t t; return t++; }
static uint16_t fake_tick; static uint16_t tick_fn(void) { return fake_tick++; }
static long fr, rd;
static uint16_t clk_base;
static uint16_t clk(void) { return (uint16_t)(clk_base + fr / 3 + rd++ / 16); }
static uint16_t pit_fn(void) { return (uint16_t)(rand() & 0xffff); }

static int run(uint16_t base) {
    srand(1); cga_init(); game_tick_fn = tick_fn; pit_counter_fn = pit_fn;
    bios_clock_hook = clk; clk_base = base; fr = 0; rd = 0;
    game_flow_run(GF_LAB_00AE);
    game_death_handler();
    level_number = 1; game_level_enter();
    level_complete = 1;                       /* el gato toco la entrada del nivel 1 (update_entrance_anim) */
    long px = 0;
    for (long f = 0; f < 400; f++) {
        fr = f; rd = 0; ua_tick_override = -1;
        input_horizontal = 0; input_vertical = 0; restart_game = false; show_attract = false;
        if (game_level_frame() == GL_EXIT) { printf("FALLO base=%u: salio en el frame %ld (object_hit=%u)\n", base, f, (unsigned)object_hit); return 1; }
        if (level_number != 2) { printf("FALLO base=%u: level_number=%u\n", base, (unsigned)level_number); return 1; }
    }
    for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) {
        size_t off = (size_t)((y & 1) ? 0x2000 : 0) + (size_t)(y >> 1) * 80 + (size_t)(x >> 2);
        if (((cga_mem[off] >> (6 - 2 * (x & 3))) & 3) == 1) px++;   /* color 1 = agua de la pecera */
    }
    if (px < 20000) { printf("FALLO base=%u: la pantalla no es la pecera (agua=%ld px)\n", base, px); return 1; }
    printf("OK base=%u: 400 frames en el nivel 2, agua=%ld px\n", base, px);
    return 0;
}
int main(void) {
    return run(0) | run(40000) | run(65500);
}
