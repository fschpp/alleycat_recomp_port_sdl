/* T82: (1) nivel 6: draw_level_background debe llamar init_level6_objects (perros y platos); (2) nivel 1: la escoba
 * (thrown_obj) al tocar al gato arma start_auto_walk y despues el gato se aparta y la escoba vuelve a moverse.
 * Uso: make test-t82 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "level6.h"
#include "level_background.h"
#include "animation_entry.h"
#include "bios_clock.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
void __wrap_speaker_spin_cycles(uint32_t c) { (void)c; }
uint16_t __wrap_read_bios_tick(void) { static uint16_t t; t += 7; return t; }
static uint16_t fake_tick; static uint16_t tick_fn(void) { return fake_tick++; }
static long fr, rd;
static uint16_t clk(void) { return (uint16_t)(fr / 3 + rd++ / 16); }
static uint16_t pit_fn(void) { return (uint16_t)(rand() & 0xffff); }

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void) {
    srand(3); cga_init(); game_tick_fn = tick_fn; bios_clock_hook = clk; pit_counter_fn = pit_fn;

    /* --- nivel 6: perros/platos --- */
    game_flow_run(GF_LAB_00AE);
    level_number = 6; difficulty_level = 0;
    memset(cga_mem, 0xaa, sizeof cga_mem);
    draw_level_background();
    int flags = 0; for (int i = 0; i < 12; i++) flags += l6_obj_flag[i] != 0;
    CHECK(flags > 0, "nivel 6: no hay perros sembrados (flags=%d)", flags);
    CHECK(l6_dat_44d6 == 0xc, "nivel 6: l6_dat_44d6=%u (esperado 0xc, init_level6_objects no corrio)", (unsigned)l6_dat_44d6);

    /* --- nivel 1: escoba --- */
    game_flow_run(GF_LAB_00AE);
    game_death_handler();
    level_number = 1; game_level_enter();
    cat_x = 0x90; cat_y = 0xb4;
    thrown_obj_x = cat_x; thrown_obj_y = 0xa0;
    auto_walk = 0;
    int armed = 0, moved_away = 0, broom_moved = 0; uint16_t bx0 = 0;
    for (long f = 0; f < 600; f++) {
        fr = f; rd = 0; ua_tick_override = -1;
        input_horizontal = 0; input_vertical = 0; restart_game = false; show_attract = false;
        if (f == 0) bx0 = thrown_obj_x;
        game_level_frame();
        if (auto_walk) armed = 1;
        if (armed && (cat_x != 0x90 || cat_y != 0xb4)) moved_away = 1;
        if (armed && (thrown_obj_x != bx0)) broom_moved = 1;
        if (level_number != 1) break;
    }
    CHECK(armed, "nivel 1: la escoba toco al gato y auto_walk nunca se armo");
    CHECK(moved_away, "nivel 1: tras el golpe el gato no inicio la animacion de salto/auto-walk");
    CHECK(broom_moved, "nivel 1: la escoba se quedo detenida tras tocar al gato");
    printf(fails ? "t82: FALLO (%d)\n" : "t82: OK\n", fails);
    return fails != 0;
}
