/* T71 — update_animation B (game_loop.asm L286-440): correcciones de update_cat_movement/update_cat_dive y
 * update_cat_frame. Sin SDL. Esperados: del ASM y de las tablas reales del DS (max_swim_speed 0x66c =
 * 4,6,8,10,12,12; max_dive_depth 0x67c = 3,3,4,4,4,4; walk_sprite_ptrs 0x9a6; walk_sprite_dims 0x9c0). */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "alley.h"
#include "animation.h"
#include "animation_entry.h"
#include "movement.h"
#include "level2.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)
static uint16_t W(unsigned o) { return (uint16_t)(ds_pool[o] | (ds_pool[o + 1] << 8)); }

static void reset(void) {
    memset(cga_mem, 0xff, CGA_MEM_SIZE);
    level_number = 2; difficulty_level = 2; input_horizontal = 0; input_vertical = 0;
    scroll_direction = 0; prev_scroll_dir = 0; prev_vert_dir = 0; in_level_mode = 1;
    speed_ramp = 0x20; anim_counter = 0x20; cat_x = 100; cat_y = 100; immune_flag = 0;
    cat_sprite_data = 0; level2_tick = 0; anim_last_tick = 777; walk_frame = 0;
    memset(l2_obj_active, 1, L2_OBJ_COUNT);        /* check_level_objects: todos activos -> los salta */
    cat_caught = 0; object_hit = 0;
}

int main(void) {
    /* 1) sin entrada vertical: anim_counter < 0x10 -> lab_0a7e (in_level_mode = 0xFF, anim_counter = 0x20) y luego
     *    buceo de bl = 0x20>>4 = 2 hacia afuera: cat_y 100 - 2 */
    reset(); anim_counter = 0x0f; in_level_mode = 1;
    update_cat_movement();
    CHECK(in_level_mode == -1 && anim_counter == 0x20 && cat_y == 98, "vertical 0, anim_counter<0x10: lab_0a7e (modo %d, ac %u, y %u)", in_level_mode, anim_counter, cat_y);
    reset(); anim_counter = 0x10; in_level_mode = 1;
    update_cat_movement();
    CHECK(in_level_mode == 1 && anim_counter == 0x0f, "vertical 0, anim_counter==0x10: decrementa (modo %d, ac %u)", in_level_mode, anim_counter);

    /* 2) indices sin mascara con difficulty_level = 2: tope de nado 8 (con `& 0x5` era 4), buceo maximo 4 (era 3) */
    reset(); input_horizontal = 1; scroll_direction = 1; speed_ramp = 0x30;
    update_cat_movement();
    CHECK(scroll_speed == 6, "scroll_speed = 0x30>>3 = 6 con tope 8 (obtuvo %u)", scroll_speed);
    reset(); input_horizontal = 1; scroll_direction = 1; speed_ramp = 0x50;   /* 0x50>>3 = 10 > tope 8 */
    update_cat_movement();
    CHECK(scroll_speed == 8, "tope max_swim_speed[2] = 8 (obtuvo %u)", scroll_speed);
    reset(); input_vertical = 1; in_level_mode = 1; anim_counter = 0x40;     /* bl = 4, tope max_dive_depth[2] = 4 */
    update_cat_movement();
    CHECK(cat_y == 104, "buceo 4 con difficulty 2 (obtuvo %u)", cat_y);
    reset(); difficulty_level = 0; input_vertical = 1; in_level_mode = 1; anim_counter = 0x40;
    update_cat_movement();
    CHECK(cat_y == 103, "buceo tope 3 con difficulty 0 (obtuvo %u)", cat_y);

    /* 3) chequeo de pasos: sub dl,bl con carry/<=3 -> cat_sprite_data == walk_sprite_ptrs[9] fija level2_tick */
    reset(); input_vertical = -1; in_level_mode = -1; cat_y = 3; anim_counter = 0x20; cat_sprite_data = W(0x9b8);
    update_cat_movement();
    CHECK(cat_y == 2 && level2_tick == 777, "pose de pasos: level2_tick = anim_last_tick (y %u, t %u)", cat_y, level2_tick);
    reset(); input_vertical = -1; in_level_mode = -1; cat_y = 3; anim_counter = 0x20; cat_sprite_data = W(0x9b8) ^ 2;
    update_cat_movement();
    CHECK(cat_y == 2 && level2_tick == 0, "otra pose: level2_tick intacto (t %u)", level2_tick);

    /* 4) update_cat_frame: pose, punteros del DS y dibujo */
    reset(); immune_flag = 1; cat_y = 60; cat_x = 100;
    update_cat_frame();
    CHECK(cat_sprite_data == W(0x9a6 + 16) && cat_sprite_dims == W(0x9c0 + 16), "inmune: pose idx 8 (data %04x dims %04x)", cat_sprite_data, cat_sprite_dims);
    CHECK(cat_screen_pos == (uint16_t)calc_cga_addr(60, 100, NULL) && cat_draw_pos == cat_screen_pos, "cat_screen_pos/cat_draw_pos");
    {   unsigned wb = 2u * (cat_sprite_dims & 0xff);
        CHECK(memcmp(&cga_mem[cat_draw_pos], cat_sprite_ptr, wb) == 0, "primera fila de la pose dibujada (AND con fondo 0xff)");
    }
    reset(); scroll_direction = 1; prev_scroll_dir = 0; cat_y = 60;       /* cambio de direccion -> pose de giro idx 12 */
    update_cat_frame();
    CHECK(cat_sprite_data == W(0x9a6 + 24) && cat_sprite_dims == W(0x9c0 + 24), "giro: pose idx 12");

    /* 5) camino completo del nivel 2 (T70 + T71) */
    reset(); ua_tick_override = 1000; anim_last_tick = 1000; pcjr_delay = 0;    /* mismo tick, sin cuenta atras -> ret */
    cat_y = 60; memset(cga_mem, 0xff, CGA_MEM_SIZE);
    update_animation_l2_path();
    CHECK(cat_sprite_data == 0 && cga_mem[calc_cga_addr(60, 100, NULL)] == 0xff, "UA_RET no dibuja");
    reset(); ua_tick_override = 1001; anim_last_tick = 1000; level2_tick = 1000; level_number = 2; cat_y = 60;   /* t=1: fase normal */
    update_animation_l2_path();
    CHECK(cat_sprite_data != 0 && prev_scroll_dir == 0 && cat_screen_pos == (uint16_t)calc_cga_addr(cat_y, 100, NULL),
          "fase normal: update_cat_movement + update_cat_frame");
    reset(); ua_tick_override = 1000 + 200; anim_last_tick = 1; level2_tick = 1000; meow_timer = 3; cat_y = 60;   /* >=192: muerte */
    update_animation_l2_path();
    CHECK(in_level_mode == 1 && immune_flag == 1 && cat_sprite_data == W(0x9a6 + 16), "fase de muerte: buceo forzado y pose inmune");

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_animation_b: OK\n");
    return 0;
}
