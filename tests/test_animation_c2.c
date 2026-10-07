/* T72 C2 — update_animation lab_0ce7..lab_0e1f (game_loop.asm L561-684). Sin SDL.
 * Esperados de leer el ASM (no de ejecutar el C). Nivel 3: check_dog_collision (solo nivel 0) no actua. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "alley.h"
#include "animation_c.h"
#include "sprite.h"
#include "gen/cat_gap1_sprites.h"
#include "gen/cat_alley_walk_frames.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static const cat_walk_frame_t pose = { NULL, 3, 11 };
static uint8_t pose_data[3 * 2 * 11];

/* el AND-blit de draw_alley_foreground altera la CGA (el fondo es 0xff y las poses no); sin dibujo queda todo 0xff */
static bool drawn(void) { for (unsigned i = 0; i < CGA_MEM_SIZE; i++) if (cga_mem[i] != 0xff) return true; return false; }

static void reset(void) {
    memset(cga_mem, 0xff, CGA_MEM_SIZE);
    for (unsigned i = 0; i < sizeof pose_data; i++) pose_data[i] = (uint8_t)(i + 1);
    level_number = 3; enemy_active = 0; enemy_chasing = 0; enemy_approach_timer = 0; enemy_exit_timer = 0;
    entry_steps = 0; at_platform = 0; in_level_mode = 0; scroll_speed = 5; cat_x = 100; cat_y = 0; cat_y_bottom = 0;
    anim_counter = 3; transitioning = 1; transition_timer = 7; auto_walk = 0; game_mode = 1; cat_died = 0;
    sprite_hidden = 0; recoil_frame = 0; cat_sprite_ptr = NULL; cat_sprite_dims = 0; cat_sprite_data = 0;
    static cat_walk_frame_t vs; vs = pose; vs.data = pose_data; vert_sprite = &vs;
}

int main(void) {
    /* from distinto de C1: no hace nada */
    reset(); cat_y_bottom = 90;
    update_animation_c2(AC_RET); CHECK(cat_y_bottom == 90 && cat_sprite_ptr == NULL, "from invalido: sin efecto");

    /* lab_0ce7, modo 0: cat_y_bottom -= anim_counter */
    reset(); cat_y_bottom = 100; anim_counter = 3; in_level_mode = 0;
    update_animation_c2(AC_L0CE7);
    CHECK(cat_y_bottom == 97 && cat_y == 97 - 0x32 && cat_screen_pos == (uint16_t)calc_cga_addr(cat_y, 100, NULL) && cat_draw_pos == cat_screen_pos,
          "modo 0: 100-3 (yb %u y %u)", cat_y_bottom, cat_y);
    CHECK(cat_sprite_ptr == pose_data && cat_sprite_dims == 0x0b03 && in_level_mode == 0, "modo 0: dibuja vert_sprite completo");
    CHECK(drawn(), "modo 0: el AND-blit altera la CGA");

    /* modo 0 con prestamo: al = 0, modo 1, anim_counter = 1 */
    reset(); cat_y_bottom = 2; anim_counter = 3; in_level_mode = 0;
    update_animation_c2(AC_L0CE7);
    CHECK(cat_y_bottom == 0 && in_level_mode == 1 && anim_counter == 1 && cat_y == 0, "prestamo: yb 0, modo 1, counter 1");
    reset(); cat_y_bottom = 3; anim_counter = 3; in_level_mode = -1;
    update_animation_c2(AC_L0CE7);
    CHECK(cat_y_bottom == 0 && in_level_mode == -1, "3-3 = 0 sin prestamo: modo -1 se conserva");

    /* modo 1: += anim_counter, <= 0xe6 pasa; con vuelta de 8 bits */
    reset(); cat_y_bottom = 100; anim_counter = 4; in_level_mode = 1;
    update_animation_c2(AC_L0CE7); CHECK(cat_y_bottom == 104 && in_level_mode == 1 && game_mode == 1, "modo 1: 100+4");
    reset(); cat_y_bottom = 0xe2; anim_counter = 4; in_level_mode = 1;
    update_animation_c2(AC_L0CE7); CHECK(cat_y_bottom == 0xe6 && in_level_mode == 1, "0xe2+4 = 0xe6 (jbe)");
    reset(); cat_y_bottom = 0xfc; anim_counter = 8; in_level_mode = 1;
    update_animation_c2(AC_L0CE7); CHECK(cat_y_bottom == 4 && in_level_mode == 1, "0xfc+8 da la vuelta a 4 (<= 0xe6)");

    /* tope 0xe6 (no nivel 7): game_mode = 0 y lab_0d29 */
    reset(); cat_y_bottom = 0xe6; anim_counter = 2; in_level_mode = 1; auto_walk = 1; at_platform = 0;
    update_animation_c2(AC_L0CE7);
    CHECK(cat_y_bottom == 0xe6 && game_mode == 0 && in_level_mode == 0 && auto_walk == 0 && scroll_speed == 2 &&
          transition_timer == 0 && transitioning == 0 && cat_y == 0xe6 - 0x32, "tope 0xe6: fin de transicion");

    /* nivel 7: <0xf8 pasa, >=0xf8 -> 0xf8 + cat_died, game_mode intacto */
    reset(); level_number = 7; cat_y_bottom = 0xe6; anim_counter = 2; in_level_mode = 1;
    update_animation_c2(AC_L0CE7); CHECK(cat_y_bottom == 0xe8 && cat_died == 0 && game_mode == 1 && in_level_mode == 1, "nivel 7: 0xe8 pasa");
    reset(); level_number = 7; cat_y_bottom = 0xf6; anim_counter = 2; in_level_mode = 1;
    update_animation_c2(AC_L0CE7); CHECK(cat_y_bottom == 0xf8 && cat_died == 1 && in_level_mode == 1 && game_mode == 1, "nivel 7: 0xf8 -> cat_died");

    /* lab_0d29 directo (CF de check_level_collision): conserva al = cat_y_bottom */
    reset(); cat_y_bottom = 120; in_level_mode = 1; auto_walk = 1; at_platform = 0;
    update_animation_c2(AC_L0D29);
    CHECK(cat_y_bottom == 120 && in_level_mode == 0 && auto_walk == 0 && scroll_speed == 2 && transition_timer == 0 &&
          transitioning == 0 && game_mode == 1 && cat_y == 120 - 0x32, "lab_0d29 directo");

    /* cat_y saturado: cat_y_bottom < 0x32 -> cat_y = 0 */
    reset(); cat_y_bottom = 0x40; anim_counter = 0x20; in_level_mode = 0;
    update_animation_c2(AC_L0CE7); CHECK(cat_y_bottom == 0x20 && cat_y == 0, "cat_y saturado a 0");

    /* sprite_hidden: con 0 restaura el fondo, con 1 no; el dibujo lo pone a 0 */
    reset(); cat_y_bottom = 100; sprite_hidden = 1; update_animation_c2(AC_L0CE7);
    CHECK(sprite_hidden == 0, "draw_alley_foreground deja sprite_hidden = 0");

    /* choque con el enemigo: check_enemy_activate -> sprite_hidden = 1 y ret sin dibujar */
    reset(); cat_y_bottom = 0xd6; anim_counter = 1; in_level_mode = 1; enemy_approach_timer = 1; enemy_x = 100 - 0x20 + 0x10;
    update_animation_c2(AC_L0CE7);
    CHECK(sprite_hidden == 1 && !drawn() && enemy_chasing != 0, "enemigo activado: oculto y sin dibujar (hid %u)", sprite_hidden);

    /* auto_walk: ciclo de retroceso recoil, dims de la tabla 0x0fd2 */
    static const uint16_t dims[8] = { 0xd03, 0xc03, 0xb03, 0xa03, 0xd03, 0xa03, 0xb03, 0xc03 };
    static const uint16_t offs[8] = { 0xa2e, 0xaca, 0xbd2, 0xb5a, 0xa7c, 0xb96, 0xd5e, 0xb12 };
    for (unsigned i = 0; i < 8; i++) {
        reset(); auto_walk = 1; cat_y_bottom = 100; in_level_mode = 0; anim_counter = 0; recoil_frame = (uint16_t)(2 * i - 2 + 16);
        update_animation_c2(AC_L0CE7);
        const cat_walk_frame_t *want = (recoil_sprite[i].data == NULL) ? &alley_walk_frames[recoil_pool_b_frame_index[i]] : &recoil_sprite[i];
        CHECK(cat_sprite_dims == dims[i] && cat_sprite_data == offs[i] && cat_sprite_ptr == want->data,
              "recoil %u: dims %x data %x", i, cat_sprite_dims, cat_sprite_data);
    }
    reset(); auto_walk = 1; recoil_frame = 14; cat_y_bottom = 100; anim_counter = 0; update_animation_c2(AC_L0CE7);
    CHECK(recoil_frame == 16 && cat_sprite_dims == dims[0], "recoil_frame = 16 & 0xe = 0 (vuelta)");

    /* recorte por arriba: cat_y_bottom < 0x32 -> se saltan (0x32 - yb) filas de 6 bytes */
    reset(); cat_y_bottom = 0x2e; anim_counter = 0; in_level_mode = 0;       /* al = 4 */
    update_animation_c2(AC_L0CE7);
    CHECK(cat_sprite_dims == 0x0703 && cat_sprite_ptr == pose_data + 4 * 6, "recorte superior 4 filas: alto 11-4 (dims %x)", cat_sprite_dims);
    reset(); cat_y_bottom = 0x32 - 10; anim_counter = 0;                      /* al = 10 -> alto 1 */
    update_animation_c2(AC_L0CE7);
    CHECK(cat_sprite_dims == 0x0103 && cat_sprite_ptr == pose_data + 10 * 6, "recorte superior 10 filas: queda 1");
    reset(); cat_y_bottom = 0x32 - 11; anim_counter = 0;                      /* al = 11 == alto -> oculto */
    update_animation_c2(AC_L0CE7);
    CHECK(sprite_hidden == 1 && !drawn(), "recorte >= alto: sprite_hidden = 1 y sin dibujar");
    reset(); cat_y_bottom = 0; anim_counter = 0;                              /* al = 0x32 > alto */
    update_animation_c2(AC_L0CE7);
    CHECK(sprite_hidden == 1 && !drawn(), "recorte mucho mayor que el alto: oculto");
    /* particularidad del ASM: con auto_walk el offset se suma a vert_sprite_data, no al sprite de retroceso */
    reset(); auto_walk = 1; recoil_frame = 0; cat_y_bottom = 0x2e; anim_counter = 0;
    update_animation_c2(AC_L0CE7);
    CHECK(cat_sprite_ptr == pose_data + 4 * 6 && cat_sprite_dims == (uint16_t)(((recoil_sprite[1].height - 4) << 8) | 3), "auto_walk + recorte: base = vert_sprite");

    /* cat_y_bottom == 0x32 exacto: sin recorte superior (jz lab_0dde), nivel 3, game_mode != 2: dibuja */
    reset(); cat_y_bottom = 0x32; anim_counter = 0; update_animation_c2(AC_L0CE7);
    CHECK(cat_sprite_dims == 0x0b03 && cat_sprite_ptr == pose_data && anim_counter == 0, "yb == 0x32: sin recorte");

    /* lab_0dee: game_mode == 2, cat_y >= 0x5e: recorte inferior (alto 11): al = cat_y - 0x5e */
    reset(); game_mode = 2; cat_y_bottom = 0x32 + 0x5e + 3; anim_counter = 0;   /* cat_y = 0x5e+3 -> al = 3 */
    update_animation_c2(AC_L0CE7);
    CHECK(cat_sprite_dims == 0x0803 && anim_counter == 2 && cat_sprite_ptr == pose_data && sprite_hidden == 0, "game_mode 2, al=3: alto 8, anim_counter = 2 (dims %x)", cat_sprite_dims);
    reset(); game_mode = 2; cat_y_bottom = 0x32 + 0x5e - 1; anim_counter = 0;   /* cat_y < 0x5e: sin recorte */
    update_animation_c2(AC_L0CE7);
    CHECK(cat_sprite_dims == 0x0b03 && anim_counter == 0, "game_mode 2, cat_y < 0x5e: sin recorte");
    reset(); game_mode = 1; cat_y_bottom = 0xb0; anim_counter = 0;               /* game_mode != 2: sin recorte */
    update_animation_c2(AC_L0CE7);
    CHECK(cat_sprite_dims == 0x0b03, "game_mode != 2: sin recorte inferior");
    /* al == alto o mayor -> lab_0e02: nivel != 7 llama setup_alley + hiss y NO dibuja */
    reset(); game_mode = 2; cat_y_bottom = 0x32 + 0x5e + 11; anim_counter = 0; cat_x = 60; scroll_direction = 0;
    update_animation_c2(AC_L0CE7);
    CHECK(!drawn() && sprite_hidden == 0 && scroll_direction == 1, "al == alto: setup_alley (cat_x < 0xa0 -> scroll_direction 1)");

    /* nivel 7: recorte inferior desde cat_y >= 0xbb; fin -> cat_died */
    reset(); level_number = 7; cat_y_bottom = (uint8_t)(0xbb + 0x32 + 2); anim_counter = 0; in_level_mode = 0;
    update_animation_c2(AC_L0CE7);
    CHECK(cat_y == 0xbb + 2 && cat_sprite_dims == 0x0903 && anim_counter == 2 && cat_died == 0, "nivel 7, al=2: alto 9 (dims %x)", cat_sprite_dims);
    reset(); level_number = 7; cat_y_bottom = (uint8_t)(0xbb + 0x32 + 1); anim_counter = 0; in_level_mode = 0;
    update_animation_c2(AC_L0CE7); CHECK(cat_y == 0xbc && cat_sprite_dims == 0x0a03, "nivel 7, al=1: alto 10");
    reset(); level_number = 7; cat_y_bottom = 0xe0; anim_counter = 0; in_level_mode = 0;   /* cat_y = 0xae < 0xbb: sin recorte */
    update_animation_c2(AC_L0CE7); CHECK(cat_sprite_dims == 0x0b03 && cat_died == 0, "nivel 7, cat_y < 0xbb: sin recorte");
    reset(); level_number = 7; cat_y_bottom = 0xf8; anim_counter = 0; in_level_mode = 0;   /* cat_y = 0xc6, al = 11 */
    update_animation_c2(AC_L0CE7); CHECK(cat_died == 1 && !drawn(), "nivel 7, al == alto: cat_died, sin dibujar");

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_animation_c2: OK\n");
    return 0;
}
