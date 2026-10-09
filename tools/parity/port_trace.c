/* Traza del port por tick BIOS simulado, en el mismo formato que tools/orig_state/read_state.py (plan de paridad §3.4).
 * Sin SDL. Reproduce el bucle del callejon (game_alley_frame) con un reloj falso: ITERS iteraciones por tick
 * (en DOSBox el original hace ~100 por tick). La entrada se guioniza por numero de tick (n=1 es el primero).
 * Uso: port_trace NTICKS ITERS "h:desde-hasta:valor v:desde-hasta:valor ..." > traza.txt
 *   h = input_horizontal (1 derecha, -1 izquierda), v = input_vertical (-1 arriba, 1 abajo); rangos de ticks incluidos.
 * Columnas: n cat_x cat_y level_number scroll_speed scroll_direction in_level_mode input_horizontal current_floor at_platform */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "animation_entry.h"
#include "score.h"
#include "bios_clock.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
void __wrap_speaker_spin_cycles(uint32_t cycles) { (void)cycles; }
uint16_t __wrap_read_bios_tick(void) { static uint16_t t; return t++; }

static uint16_t cur_tick = 0x5000;
static uint16_t clk(void) { return cur_tick; }
static uint16_t pit_seed = 0x1234;
static uint16_t pit_fn(void) { return pit_seed; }          /* semilla del azar: env PIT_SEED (decimal o 0x..) */

typedef struct { char k; int a, b, v; } rule_t;
static rule_t rules[64]; static int nrules;

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: %s NTICKS ITERS [script]\n", argv[0]); return 2; }
    int nticks = atoi(argv[1]), iters = atoi(argv[2]);
    if (argc > 3) for (char *t = strtok(argv[3], " "); t && nrules < 64; t = strtok(NULL, " ")) {
        rule_t r; if (sscanf(t, "%c:%d-%d:%d", &r.k, &r.a, &r.b, &r.v) == 4) rules[nrules++] = r;
    }
    if (getenv("PIT_SEED")) pit_seed = (uint16_t)strtoul(getenv("PIT_SEED"), NULL, 0);
    cga_init();
    bios_clock_hook = clk; game_tick_fn = clk; pit_counter_fn = pit_fn; ua_tick_override = -1;
    rng_seed = pit_seed ? pit_seed : 0xfa59;           /* cga_init() siembra con el reloj real: se pisa para ser determinista */
    game_flow_run(GF_LAB_00AE);                        /* nueva partida -> callejon listo */
    rng_seed = pit_seed ? pit_seed : 0xfa59;           /* el original lo lee del PIT una vez; aqui se fija (env PIT_SEED = rng_seed del original en n=1) */
    printf("n cat_x cat_y level_number scroll_speed scroll_direction in_level_mode input_horizontal current_floor at_platform enemy_active enemy_x enemy_y_pos enemy_chasing enemy_approach_timer\n");
    for (int n = 1; n <= nticks; n++) {
        cur_tick++;
        int8_t h = 0, v = 0;
        for (int i = 0; i < nrules; i++) if (n >= rules[i].a && n <= rules[i].b) { if (rules[i].k == 'h') h = (int8_t)rules[i].v; else v = (int8_t)rules[i].v; }
        for (int it = 0; it < iters; it++) {
            input_horizontal = h; input_vertical = v; input_fire = false;
            restart_game = false; show_attract = false;
            if (it == 0)
                printf("%d %u %u %u %u %d %d %d %u %u %u %u %u %u %u\n", n, (unsigned)(uint16_t)cat_x, (unsigned)cat_y, (unsigned)level_number,
                       (unsigned)scroll_speed, (int)(int8_t)scroll_direction, (int)(int8_t)in_level_mode, (int)input_horizontal,
                       (unsigned)current_floor, (unsigned)at_platform, (unsigned)enemy_active, (unsigned)enemy_x, (unsigned)enemy_y_pos,
                       (unsigned)enemy_chasing, (unsigned)enemy_approach_timer);
            gf_next_t nx = game_alley_frame();
            if (nx != GF_STAY) { printf("# n=%d salida del callejon: next=%d level=%u\n", n, (int)nx, (unsigned)level_number); return 0; }
        }
    }
    return 0;
}
