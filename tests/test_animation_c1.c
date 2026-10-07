/* T72 C1 — update_animation lab_0bac..lab_0ce7 (game_loop.asm L441-560). Sin SDL.
 * Esperados de leer el ASM (no de ejecutar el C). Nivel 3 para que check_dog_collision (solo nivel 0) no actue. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "alley.h"
#include "animation_c.h"
#include "level_collision.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void reset(void) {
    memset(cga_mem, 0xff, CGA_MEM_SIZE);
    level_number = 3; enemy_active = 0; enemy_chasing = 0;
    entry_steps = 0; entry_delay = 0; at_platform = 0; in_level_mode = 0; scroll_direction = 0;
    scroll_speed = 2; cat_x = 100; cat_y = 100; cat_y_bottom = 0; anim_counter = 5; anim_step = 0; anim_accumulator = 0;
    transitioning = 0; transition_timer = 0; input_vertical = 0; cat_sprite_data = 0;
}

int main(void) {
    reset(); enemy_active = 1; entry_steps = 3;
    CHECK(update_animation_c1() == AC_RET && entry_steps == 3, "enemy_active -> ret sin tocar nada");

    /* cuenta atras de entry_delay solo si el perro NO persigue */
    reset(); entry_steps = 3; entry_delay = 2; enemy_chasing = 0;
    CHECK(update_animation_c1() == AC_RET && entry_delay == 1 && entry_steps == 3, "entry_delay 2 -> 1");
    reset(); entry_steps = 3; entry_delay = 2; enemy_chasing = 1;
    CHECK(update_animation_c1() == AC_RET && entry_delay == 2, "perro persiguiendo: entry_delay congelado");

    /* paso de entrada: 3 -> 2 (sin restore), update_viewport deja cat_sprite_data = 0xe y dims 0x0b00|(3-steps) */
    reset(); entry_steps = 3; entry_delay = 0; scroll_direction = 1;
    CHECK(update_animation_c1() == AC_RET && entry_steps == 2 && cat_sprite_data == 0xe && cat_sprite_dims == 0x0b01,
          "paso de entrada 3->2 (steps %u data %x dims %x)", entry_steps, cat_sprite_data, cat_sprite_dims);
    reset(); entry_steps = 2; entry_delay = 0; scroll_direction = 1;
    CHECK(update_animation_c1() == AC_RET && entry_steps == 1 && cat_sprite_data == 0xe && cat_sprite_dims == 0x0b02, "paso 2->1");

    /* ultimo paso: steps 1 -> 0 -> scroll_speed = 8, update_scroll mueve 8 y sigue a lab_0c1c (at_platform 0, modo 0 -> lab_0e23) */
    reset(); entry_steps = 1; entry_delay = 0; scroll_direction = 1; cat_x = 100;
    CHECK(update_animation_c1() == AC_L0E23 && entry_steps == 0 && scroll_speed == 8 && cat_x == 108, "ultimo paso (x %d)", cat_x);

    /* at_platform */
    reset(); at_platform = 1; input_vertical = 0; cat_y = 60;
    CHECK(update_animation_c1() == AC_RET && at_platform == 2 && scroll_speed == 6 && cat_sprite_data == 0x9da &&
          cat_sprite_dims == 0xe03 && cat_screen_pos == (uint16_t)calc_cga_addr(60, 100, NULL) && cat_draw_pos == cat_screen_pos,
          "at_platform 1 -> 2, pose 0x9da");
    CHECK(memcmp(&cga_mem[cat_draw_pos], &ds_pool[0x9da], 6) == 0, "primera fila de la pose 0x9da dibujada");
    reset(); at_platform = 1; input_vertical = 1;
    CHECK(update_animation_c1() == AC_L0E78 && at_platform == 2, "at_platform 1 con input_vertical -> lab_0e78");
    reset(); at_platform = 5; input_vertical = -1; cat_sprite_data = 0;
    CHECK(update_animation_c1() == AC_L0E78 && at_platform == 5 && cat_sprite_data == 0, "at_platform>1: solo mira input_vertical");
    reset(); at_platform = 5; input_vertical = 0;
    CHECK(update_animation_c1() == AC_RET, "at_platform>1 sin input -> ret");

    /* in_level_mode == 0 y at_platform == 0 -> lab_0e23 */
    reset(); in_level_mode = 0;
    CHECK(update_animation_c1() == AC_L0E23, "modo 0 -> lab_0e23");

    /* choque con el borde derecho (nivel 3: 0x123): scroll_direction=0, anim_counter=2, modo 1, timer 0 */
    reset(); in_level_mode = -1; scroll_direction = 1; scroll_speed = 5; cat_x = 0x122; transition_timer = 7; transitioning = 1;
    CHECK(update_animation_c1() == AC_L0CE7 && scroll_direction == 0 && anim_counter == 2 && in_level_mode == 1 && transition_timer == 0,
          "borde: reinicia direccion y modo");

    /* anim_accumulator/anim_step (scroll_direction 0 -> update_scroll no mueve y no choca) */
    reset(); in_level_mode = -1; anim_step = 3; anim_accumulator = 5; anim_counter = 5; transitioning = 1;
    update_animation_c1(); CHECK(anim_accumulator == 2 && anim_counter == 5, "sin prestamo: anim_counter intacto");
    reset(); in_level_mode = -1; anim_step = 3; anim_accumulator = 2; anim_counter = 5; transitioning = 1;
    update_animation_c1(); CHECK(anim_accumulator == 0xff && anim_counter == 4 && in_level_mode == -1, "prestamo, modo -1: anim_counter--");
    reset(); in_level_mode = -1; anim_step = 3; anim_accumulator = 2; anim_counter = 1; transitioning = 1;
    update_animation_c1(); CHECK(in_level_mode == 1 && anim_counter == 1, "prestamo, modo -1, counter<=1: modo = 1");
    reset(); in_level_mode = 1; anim_step = 3; anim_accumulator = 2; anim_counter = 3; transitioning = 1;
    update_animation_c1(); CHECK(anim_counter == 4, "prestamo, modo 1: anim_counter++");
    reset(); in_level_mode = 1; anim_step = 3; anim_accumulator = 2; anim_counter = 4; transitioning = 1;
    update_animation_c1(); CHECK(anim_counter == 4, "prestamo, modo 1, counter>=4: sin cambio");

    /* transition_timer */
    reset(); in_level_mode = 1; transition_timer = 2;
    CHECK(update_animation_c1() == AC_L0CE7 && transition_timer == 1, "timer 2 -> 1 -> lab_0ce7");
    reset(); in_level_mode = -1; transition_timer = 1;
    CHECK(update_animation_c1() == AC_L0CE7 && transition_timer == 0, "timer 1 -> 0 con modo -1 -> lab_0ce7");

    /* check_level_collision: CF=1 -> lab_0d29, CF=0 -> lab_0ce7 (barrido de cat_y_bottom en el nivel 3) */
    int saw_hit = 0, saw_miss = 0;
    for (unsigned yb = 0; yb < 256 && !(saw_hit && saw_miss); yb++) {
        reset(); in_level_mode = 1; cat_y_bottom = (uint8_t)yb; cat_y = (uint8_t)(yb > 0x20 ? yb - 0x20 : 0);
        bool cf = check_level_collision();
        reset(); in_level_mode = 1; cat_y_bottom = (uint8_t)yb; cat_y = (uint8_t)(yb > 0x20 ? yb - 0x20 : 0);
        ac_next_t r = update_animation_c1();
        CHECK(r == (cf ? AC_L0D29 : AC_L0CE7), "despacho segun CF (yb=%u cf=%d r=%d)", yb, cf, r);
        if (cf) saw_hit = 1; else saw_miss = 1;
    }
    CHECK(saw_hit && saw_miss, "el barrido debe ver CF=0 y CF=1 (hit %d miss %d)", saw_hit, saw_miss);

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_animation_c1: OK\n");
    return 0;
}
