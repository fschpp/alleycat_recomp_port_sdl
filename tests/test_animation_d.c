/* T73 — update_animation lab_0e23..lab_0f86 (game_loop.asm L685-817). Sin SDL.
 * Esperados de leer el ASM (no de ejecutar el C). */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "alley.h"
#include "animation_d.h"
#include "level_collision.h"
#include "gen/cat_gap1_sprites.h"
#include "gen/cat_alley_walk_frames.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static bool drawn(void) { for (unsigned i = 0; i < CGA_MEM_SIZE; i++) if (cga_mem[i] != 0xff) return true; return false; }

static const uint16_t walk_ds[12] = { 0x0bd2, 0x0c14, 0x0c98, 0x0c56, 0x0cda, 0x0d1c, 0x0d5e, 0x0da0, 0x0e24, 0x0de2, 0x0e66, 0x0ea8 };
static bool is_walk_ds(uint16_t v) { for (unsigned i = 0; i < 12; i++) if (walk_ds[i] == v) return true; return false; }

static void reset(void) {
    memset(cga_mem, 0xff, CGA_MEM_SIZE);
    level_number = 3; enemy_active = 0; enemy_chasing = 0; enemy_approach_timer = 0; enemy_exit_timer = 0;
    in_level_mode = 0; scroll_direction = 0; prev_scroll_dir = 0; scroll_speed = 2; cat_x = 100; cat_y = 0xb8; cat_y_bottom = 0;
    anim_counter = 3; anim_step = 0; anim_accumulator = 5; transition_timer = 0; transitioning = 0; auto_walk = 0; game_mode = 1;
    at_platform = 3; l3_platform_id = 7; door_contact = 0; jump_hit = 0; fall_counter = 0; sprite_hidden = 0;
    input_horizontal = 0; input_vertical = 0; cat_sprite_data = 0; cat_sprite_ptr = NULL; cat_sprite_dims = 0; vert_sprite = NULL;
    walk_anim_frame = 0;
}

int main(void) {
    reset(); cat_x = 77; update_animation_d(AC_RET);
    CHECK(cat_x == 77 && !drawn() && scroll_direction == 0, "from invalido: sin efecto");

    /* lab_0e23, nivel 3, cat_y >= 0xb4 -> lab_0e78: copia las entradas; sin vertical -> paso de caminata y dibujo */
    reset(); input_horizontal = 1; scroll_direction = -1;
    update_animation_d(AC_L0E23);
    CHECK(prev_scroll_dir == -1 && scroll_direction == 1 && in_level_mode == 0, "lab_0e78: prev/dir/modo");
    CHECK(cat_x > 100 && cat_sprite_dims == 0xb03 && is_walk_ds(cat_sprite_data) && drawn() && cat_draw_pos == cat_screen_pos &&
          cat_screen_pos == (uint16_t)calc_cga_addr(cat_y, (uint16_t)cat_x, NULL),
          "caminata: x %d dims %x data %x", cat_x, cat_sprite_dims, cat_sprite_data);
    {   /* cat_sprite_data es el offset DS del frame que dibuja */
        unsigned idx = 99; for (unsigned i = 0; i < 12; i++) if (walk_ds[i] == cat_sprite_data) idx = i;
        CHECK(idx < 12 && cat_sprite_ptr == alley_walk_frames[idx].data, "cat_sprite_data y cat_sprite_ptr coinciden");
    }

    /* idle: sin direccion ni modo -> spawn_window_event, sin paso de caminata */
    reset(); input_horizontal = 0; input_vertical = 0; cat_sprite_data = 0x1234;
    update_animation_d(AC_L0E78);
    CHECK(cat_sprite_data == 0x1234 && cat_x == 100 && cat_sprite_dims != 0xb03, "idle: ventana, sin caminata");

    /* AC_L0E78 salta la geometria */
    reset(); cat_y = 0x20; input_horizontal = -1;
    update_animation_d(AC_L0E78);
    CHECK(scroll_direction == -1 && cat_x < 100 && in_level_mode == 0, "AC_L0E78 no consulta check_level_collision");

    /* nivel 3, cat_y < 0xb4: barrido buscando suelo (CF=1) y hueco (CF=0) */
    int saw_floor = 0, saw_gap = 0;
    for (unsigned y = 0; y < 0xb4 && !(saw_floor && saw_gap); y++) {
        reset(); cat_y = (uint8_t)y; bool cf = check_level_collision();
        reset(); cat_y = (uint8_t)y; input_horizontal = 1; scroll_direction = 1;
        update_animation_d(AC_L0E23);
        if (!cf) {   /* hueco: scroll_direction = 0, modo 1, lab_0eb1 (sin mirar cat_y) */
            saw_gap = 1;
            CHECK(scroll_direction == 0 && in_level_mode == 1 && anim_step == 0x20 && anim_counter == 1 && transition_timer == 8 &&
                  game_mode == 0 && anim_accumulator == 1 && at_platform == 0 && l3_platform_id == 0 && vert_sprite == &enter_sprite[3] && !drawn(),
                  "hueco y=%u: bajada (dir %d modo %d step %x)", y, scroll_direction, in_level_mode, anim_step);
        } else {     /* suelo: nivel != 0 -> lab_0e78 con input_horizontal = 1 */
            saw_floor = 1;
            CHECK(scroll_direction == 1 && prev_scroll_dir == 1 && in_level_mode == 0, "suelo y=%u: lab_0e78", y);
        }
    }
    CHECK(saw_floor && saw_gap, "el barrido debe ver suelo y hueco (suelo %d hueco %d)", saw_floor, saw_gap);

    /* lab_0e91: modo 1 (input_vertical = 1) */
    reset(); cat_y = 0x40; input_horizontal = 1; input_vertical = 1; game_mode = 1; door_contact = 1;
    update_animation_d(AC_L0E78);
    CHECK(in_level_mode == 1 && anim_step == 0x20 && anim_counter == 1 && transition_timer == 8 && game_mode == 0 && at_platform == 0 &&
          anim_accumulator == 1 && vert_sprite == &enter_sprite[4] && l3_platform_id == 0 && door_contact == 0 && !drawn(),
          "modo 1, dir 1: enter_sprite[4], catch sound limpia door_contact");
    reset(); cat_y = 0x40; input_horizontal = -1; input_vertical = 1; game_mode = 2;
    update_animation_d(AC_L0E78);
    CHECK(vert_sprite == &enter_sprite[2] && game_mode == 2, "modo 1, dir -1: bl = 0 + 6 -> indice 3 -> enter_sprite[2]; game_mode != 1 intacto");
    reset(); cat_y = 0x40; input_horizontal = 0; input_vertical = 1;
    update_animation_d(AC_L0E78);
    CHECK(vert_sprite == &enter_sprite[3], "modo 1, dir 0: indice 4 -> enter_sprite[3]");

    /* modo 1 con cat_y >= 0xb4: vuelve al callejon y camina */
    reset(); cat_y = 0xb4; input_horizontal = 1; input_vertical = 1; auto_walk = 1;
    update_animation_d(AC_L0E78);
    CHECK(in_level_mode == 0 && auto_walk == 0 && input_vertical == 0 && cat_x > 100 && drawn(), "modo 1, cat_y == 0xb4: reset y caminata");
    reset(); cat_y = 0xb3; input_horizontal = 1; input_vertical = 1;
    update_animation_d(AC_L0E78);
    CHECK(in_level_mode == 1 && !drawn(), "modo 1, cat_y == 0xb3: aun baja");

    /* lab_0ec9: modo -1 (subida) */
    reset(); input_horizontal = -1; input_vertical = -1; scroll_speed = 5; game_mode = 1;
    update_animation_d(AC_L0E78);
    CHECK(in_level_mode == -1 && transition_timer == 0 && scroll_speed == 3 && anim_step == 0xa0 && anim_counter == 8 && game_mode == 2 &&
          at_platform == 0 && anim_accumulator == 1 && vert_sprite == &climb_sprite && !drawn(),
          "modo -1: speed 5 -> 3, step ((5^0xf)<<4) = 0xa0 (speed %u step %x)", scroll_speed, anim_step);
    reset(); input_horizontal = 1; input_vertical = -1; scroll_speed = 2; transition_timer = 4;
    update_animation_d(AC_L0E78);
    CHECK(scroll_speed == 2 && anim_step == (uint8_t)((2 ^ 0xf) << 4) && transition_timer == 0 && vert_sprite == &enter_sprite[1] && game_mode == 2,
          "modo -1, speed 2 (jbe): sin resta, dir 1 -> indice 2 -> enter_sprite[1]");
    reset(); input_horizontal = 0; input_vertical = -1; scroll_speed = 0x0108;
    update_animation_d(AC_L0E78);
    CHECK(scroll_speed == 0x0106 && anim_step == (uint8_t)((8 ^ 0xf) << 4) && vert_sprite == &enter_sprite[0], "modo -1: el byte alto de scroll_speed se conserva");
    reset(); input_horizontal = 0; input_vertical = -1; game_mode = 0;
    update_animation_d(AC_L0E78);
    CHECK(game_mode == 0, "modo -1: game_mode != 1 no cambia");

    /* choque con el enemigo en el paso de caminata */
    reset(); cat_y = 0xb8; input_horizontal = 1; enemy_approach_timer = 1; enemy_x = 108;
    update_animation_d(AC_L0E23);
    CHECK(enemy_chasing != 0 && !drawn() && cat_sprite_dims != 0xb03, "check_enemy_activate: sin dibujar");

    /* nivel 0: salto forzado por check_jump_collision */
    int saw_pos = 0, saw_neg = 0;
    for (unsigned n = 0; n < 40; n++) {
        reset(); level_number = 0; cat_y = 0x60; cat_x = 100; input_horizontal = 0; input_vertical = 0; jump_hit = 0;
        fall_counter = 1; fall_target_x = 100; fall_cur_y = 0x60; fall_sprite_dims = 0x0d02;
        update_animation_d(AC_L0E23);
        CHECK(jump_hit == 1 && input_vertical == 1 && (input_horizontal == 1 || input_horizontal == -1) && in_level_mode == 1 &&
              scroll_direction == input_horizontal && anim_step == 0x20 && transition_timer == 8, "salto forzado (n=%u jump_hit %u vert %d hor %d)", n, jump_hit, input_vertical, input_horizontal);
        if (input_horizontal == 1) saw_pos = 1; else saw_neg = 1;
    }
    CHECK(saw_pos && saw_neg, "el salto forzado debe dar las dos direcciones (+ %d, - %d)", saw_pos, saw_neg);
    /* nivel 0 con suelo y sin choque: jump_hit se limpia y sigue lab_0e78 con las entradas actuales */
    reset(); level_number = 0; cat_y = 0x60; jump_hit = 1; fall_counter = 0; input_horizontal = 1;
    update_animation_d(AC_L0E23);
    CHECK(jump_hit == 0 && scroll_direction == 1 && in_level_mode == 0, "nivel 0 sin choque: jump_hit = 0");

    /* nivel 7: no hay limite cat_y; el despacho sigue a check_level_collision (escaneo de cat_x) */
    int l7_gap = 0, l7_floor = 0;
    for (int x = 0; x < 300; x += 3) for (unsigned y = 0x10; y < 0x100; y += 0x20) {
        reset(); level_number = 7; cat_y = (uint8_t)y; cat_x = (int16_t)x; bool cf = check_level_collision();
        reset(); level_number = 7; cat_y = (uint8_t)y; cat_x = (int16_t)x; input_horizontal = 1; scroll_direction = 1;
        update_animation_d(AC_L0E23);
        if (!cf) { l7_gap++; CHECK(in_level_mode == 1 && scroll_direction == 0 && anim_step == 0x20, "nivel 7 sin suelo x=%d: lab_0eb1", x); }
        else     { l7_floor++; CHECK(scroll_direction == 1 && in_level_mode == 0, "nivel 7 con suelo x=%d: lab_0e78", x); }
    }
    printf("(nivel 7: %d sin suelo, %d con suelo)\n", l7_gap, l7_floor);

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_animation_d: OK\n");
    return 0;
}
