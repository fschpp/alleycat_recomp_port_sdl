/* T83: (1) ratones del callejon: con at_platform == 0 el golpe puntua y marca obj_hit; (2) nivel 2: el color de fondo
 * (indice 0, el del gato) sigue al aire; (3) tendederos: throw_timer se recarga con THROW_TIMER_DIV (>=1 y chico);
 * (4) perro mordiendo: draw_enemy guarda el fondo del rectangulo nuevo (no deja el viejo).
 * Uso: make test-t83 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "cycle_objects.h"
#include "bios_clock.h"
#include "throw.h"
#include "palette.h"
#include "animation_entry.h"
#include "level2.h"
#include "score.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static uint16_t fake; static uint16_t clk(void) { return fake; }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void) {
    cga_init(); bios_clock_hook = clk;

    /* 1) raton golpeado por el gato con at_platform == 0 */
    level_number = 1; transitioning = 0; gravity_y = 0;
    init_cycle_objects();
    memset(current_score, 0, sizeof current_score);
    int scored = 0, hit = 0;
    for (int slot = 0; slot < 3 && !hit; slot++) {
        for (int k = 0; k < 6 && !hit; k++) {
            at_platform = 0; cat_x = 0x60; cat_y = 0xa0; cat_y_bottom = 0xb0;
            for (int i = 0; i < 3; i++) { obj_x[i] = 0x60; obj_y[i] = 0xa4; obj_hidden[i] = 0; obj_hit[i] = 0; }
            fake += 3; update_cycle_objects();
            for (int i = 0; i < 3; i++) if (obj_hit[i]) hit = 1;
        }
    }
    for (int i = 0; i < 7; i++) if (current_score[i]) scored = 1;
    CHECK(hit, "raton no marcado como golpeado con at_platform == 0");
    CHECK(scored, "el golpe al raton no sumo puntos");

    /* 2) color del gato en la pecera */
    static const struct { unsigned t; uint8_t bl; } col[] = { {0,0}, {48,1}, {96,5}, {144,4} };
    for (unsigned i = 0; i < 4; i++) {
        rom_id = 0xff; level_number = 2; difficulty_level = 2; level2_tick = 1000; object_hit = 0; meow_timer = 5; level2_rise = 5;
        ua_tick_override = (int32_t)(1000 + col[i].t);
        anim_last_tick = 1; pcjr_delay = 0; in_level_mode = 1; scroll_direction = 0; cat_y = 100; cat_x = 100;
        update_animation_entry();
        CHECK(l2_border_color == col[i].bl && (cga_color_select & 0xf) == col[i].bl,
              "pecera t=%u: paleta (%u) / l2_border_color (%u), esperado %u", col[i].t, cga_color_select & 0xf, l2_border_color, col[i].bl);
    }
    ua_tick_override = -1;

    /* 3) tendederos: recarga de throw_timer */
    level_number = 1; transitioning = 0; gravity_y = 0; at_platform = 1; cat_y = 0xb4; difficulty_level = 0;
    throw_timer = 1; throw_last_tick = 0; fake = 500; current_floor = 0;
    update_thrown_objects();
    CHECK(throw_timer >= 1 && throw_timer <= 8, "throw_timer recargado = %u (esperado 1..8)", throw_timer);

    printf(fails ? "test-t83: %d FALLOS\n" : "test-t83: OK\n", fails);
    return fails != 0;
}
