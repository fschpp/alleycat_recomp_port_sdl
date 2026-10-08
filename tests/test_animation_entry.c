/* T70 — entrada de update_animation (game_loop.asm L158-285). Sin SDL.
 * Valores esperados: del ASM y de las tablas reales del data segment (difficulty_level = 2):
 *   level2_phase1_ticks[2]=192, level2_death_ticks[2]=264, color1/2/3_ticks[2]=48/96/144
 * (no se ejecuta el C para obtenerlos). Ver PROGRESS.md §6be. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "animation_entry.h"
#include "level2.h"
#include "level6.h"
#include "movement.h"
#include "palette.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void reset(void) {
    memset(cga_mem, 0, CGA_MEM_SIZE);
    ua_tick_override = 500; anim_last_tick = 500; pcjr_delay = 0; rom_id = 0xff;
    level_number = 3; difficulty_level = 2;
    in_level_mode = 1; scroll_direction = 0; l3_door_anim_frame = 0; l6_dat_44bd = 0;
    level2_tick = 1000; object_hit = 0; meow_timer = 5; level2_rise = 5;
    cat_x = 100; cat_y = 100; immune_flag = 0; anim_counter = 0x10; l2_border_color = 9;
}

static void l2_at(unsigned elapsed) { level_number = 2; ua_tick_override = (int32_t)(1000 + elapsed); anim_last_tick = 1; }

int main(void) {
    /* 1) compuerta tick / pcjr_delay */
    reset();
    CHECK(update_animation_entry() == UA_RET && pcjr_delay == 0, "mismo tick y pcjr_delay==0 -> ret");
    reset(); pcjr_delay = 2;
    CHECK(update_animation_entry() == UA_RET && pcjr_delay == 1, "pcjr_delay 2 -> dec, sigue >0 -> ret");
    reset(); pcjr_delay = 1;
    CHECK(update_animation_entry() == UA_L0BAC && pcjr_delay == 0 && anim_last_tick == 500, "pcjr_delay 1 -> 0 -> sigue");
    reset(); ua_tick_override = 501;
    CHECK(update_animation_entry() == UA_L0BAC && pcjr_delay == 0x20 && anim_last_tick == 501 && ua_anim_tick_delay == 0x20,
          "tick nuevo -> pcjr_delay=0x20 y anim_last_tick");
    reset(); ua_tick_override = 501; rom_id = 0xfd;
    CHECK(update_animation_entry() == UA_L0BAC && pcjr_delay == 0x10, "PCjr: pcjr_delay=0x10");

    /* 2) bloqueos de los niveles 4 y 6 (anim_last_tick ya se actualizo antes del bloqueo) */
    reset(); ua_tick_override = 501; level_number = 4; l3_door_anim_frame = 1;
    CHECK(update_animation_entry() == UA_RET && anim_last_tick == 501, "nivel 4 con puerta animandose -> ret");
    reset(); ua_tick_override = 501; level_number = 4;
    CHECK(update_animation_entry() == UA_L0BAC, "nivel 4 sin puerta -> sigue");
    reset(); ua_tick_override = 501; level_number = 6; l6_dat_44bd = 1;
    CHECK(update_animation_entry() == UA_RET, "nivel 6 con dat_44bd -> ret");
    reset(); ua_tick_override = 501; level_number = 6;
    CHECK(update_animation_entry() == UA_L0BAC, "nivel 6 sin dat_44bd -> sigue");
    reset(); ua_tick_override = 501; in_level_mode = 0; scroll_direction = 0;
    CHECK(update_animation_entry() == UA_L0BAC, "gato quieto: pasa por la rama de vsync y sigue");

    /* 3) nivel 2: color del borde segun el tiempo transcurrido (48/96/144) */
    static const struct { unsigned t; uint8_t bl; } col[] = { {0,0}, {47,0}, {48,1}, {95,1}, {96,5}, {143,5}, {144,4}, {191,4} };
    for (unsigned i = 0; i < sizeof col / sizeof col[0]; i++) {
        reset(); l2_at(col[i].t);
        CHECK(update_animation_entry() == UA_L09F6 && l2_border_color == col[i].bl && object_hit == 0,
              "nivel 2 t=%u -> borde %u (obtuvo %u)", col[i].t, col[i].bl, l2_border_color);
        CHECK((cga_color_select & 0xf) == col[i].bl, "nivel 2 t=%u: el color de fondo/borde (indice 0) sigue a l2_border_color", col[i].t);
    }

    /* 4) nivel 2: fase de muerte (>=192), maullido y object_hit (>=264) */
    reset(); l2_at(192); meow_timer = 3;
    CHECK(update_animation_entry() == UA_L0A86 && meow_timer == 2 && object_hit == 0 && in_level_mode == 1 &&
          scroll_direction == 0 && immune_flag == 1 && anim_counter == 0x20 && l2_border_color == 0,
          "t=192: lab_09b9 sin object_hit, meow_timer 3->2");
    reset(); l2_at(263); meow_timer = 3;
    CHECK(update_animation_entry() == UA_L0A86 && object_hit == 0, "t=263 aun sin object_hit");
    reset(); l2_at(264); meow_timer = 3;
    CHECK(update_animation_entry() == UA_L0A86 && object_hit == 1, "t=264 object_hit=1");

    /* 5) maullido: meow_timer 1 -> 0 -> se reinicia a 6 y level2_rise sube 0x1e solo con cat_y >= 0xb3 y rise < 0xc8 */
    /* blit_masked hace AND con el fondo: con fondo 0xff el resultado es el sprite tal cual */
    reset(); memset(cga_mem, 0xff, CGA_MEM_SIZE); l2_at(200); meow_timer = 1; cat_y = 0xb3; level2_rise = 5; cat_x = 100;
    CHECK(update_animation_entry() == UA_L0A86 && meow_timer == 6 && level2_rise == 0x23, "cat_y=0xb3: rise 5 -> 0x23");
    {   /* dl = 0xb3 - 0x23 = 0x90 (&0xf8 = 0x90); sprite de 3 palabras x 5 filas */
        size_t di = calc_cga_addr(0x90, 100, NULL);
        CHECK(memcmp(&cga_mem[di], &ds_pool[0x64e], 6) == 0, "primera fila del sprite del maullido en y=0x90");
    }
    reset(); l2_at(200); meow_timer = 1; cat_y = 0xb2; level2_rise = 5;
    update_animation_entry(); CHECK(level2_rise == 5, "cat_y<0xb3: rise sin cambio");
    reset(); l2_at(200); meow_timer = 1; cat_y = 0xb3; level2_rise = 0xc8;
    update_animation_entry(); CHECK(level2_rise == 0xc8, "rise>=0xc8: sin cambio");
    reset(); l2_at(200); meow_timer = 1; cat_y = 0x10; level2_rise = 0x23; memset(cga_mem, 0xff, CGA_MEM_SIZE);
    update_animation_entry();   /* 0x10 - 0x23 con borrow -> dl = 0 */
    {   size_t di = calc_cga_addr(0, 100, NULL);
        CHECK(memcmp(&cga_mem[di], &ds_pool[0x64e], 6) == 0, "borrow en sub dl,al -> y=0");
    }

    /* 6) update_cat_movement: speed_ramp < 0x10 sin entrada horizontal REINICIA direccion (jc lab_0a2e) */
    reset(); input_horizontal = 0; input_vertical = 0; scroll_direction = 1; speed_ramp = 0x0f; anim_counter = 0x20; in_level_mode = 1;
    update_cat_movement();
    CHECK(scroll_direction == 0 && speed_ramp == 0x20, "speed_ramp<0x10: scroll_direction=0, speed_ramp=0x20 (obtuvo %d,%u)", scroll_direction, speed_ramp);
    reset(); input_horizontal = 0; input_vertical = 0; scroll_direction = 1; speed_ramp = 0x10; anim_counter = 0x20; in_level_mode = 1;
    update_cat_movement();
    CHECK(scroll_direction == 1 && speed_ramp == 0x0f, "speed_ramp==0x10: decrementa y conserva direccion");

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_animation_entry: OK\n");
    return 0;
}
