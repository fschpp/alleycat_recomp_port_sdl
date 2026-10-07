/* T77 — sprite del pez (level_physics.asm lab_1a9a..lab_1ae6) y flags que antes se trataban como 0:
 *   [0x418] = force_level7, [0x556] = mode_start_tick, [0x558] = entry_steps.
 * Los bytes esperados de los sprites se leen A MANO del texto del ASM (data/enemy_sprites.asm y
 * cat.asm), no de ds_pool, para que el test compruebe tambien los offsets DS 0x15e0 / 0x2681. */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "enemy.h"
#include "jump_gravity.h"
#include "bios_clock.h"
#include "sound.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
bool audio_init(void) { return false; } void audio_shutdown(void) {}

static uint16_t fake_tick;
static uint16_t hook(void) { return fake_tick; }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void row_bytes(int x, int y, uint8_t out[8]) {
    size_t a = calc_cga_addr((uint8_t)y, (uint16_t)x, NULL);
    memcpy(out, &cga_mem[a], 8);
}

static void test_draw(void) {
    /* jump_land_sprite_data fila 3 (offs 24..31 del label, del texto de data/enemy_sprites.asm) */
    static const uint8_t asc_row3[8] = {0x00, 0x01, 0x50, 0x3c, 0x0c, 0x00, 0x00, 0x00};
    /* jump_land_sprites (cat.asm): fila 0 = 55x8 (+3f ec de relleno), fila 1 = 55x8 (+3b f8) */
    static const uint8_t desc_row0[8] = {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
    uint8_t got[8];
    cga_init(); jump_x = 0x68; jump_y = 0x38;

    /* subida, fila 3 (counter 0x11 -> draw_y = jump_y + 3), force_level7 = 1 */
    memset(cga_mem, 0xff, CGA_MEM_SIZE); force_level7 = 1; jump_anim_counter = 0x11; jump_draw_y = (uint8_t)(jump_y + 3);
    draw_fish_line(); row_bytes(0x68, jump_y + 3, got);
    CHECK(memcmp(got, asc_row3, 8) == 0, "subida force_level7=1: fila 3 distinta (%02x %02x %02x %02x ...)", got[0], got[1], got[2], got[3]);

    /* subida con force_level7 = 0: `rep stosw` con 0 borra las 4 words */
    memset(cga_mem, 0xff, CGA_MEM_SIZE); force_level7 = 0;
    draw_fish_line(); row_bytes(0x68, jump_y + 3, got);
    static const uint8_t zero8[8] = {0};
    CHECK(memcmp(got, zero8, 8) == 0, "subida force_level7=0: no borra la linea");

    /* bajada (counter <= 0xe): fila 0 y fila 1 de jump_land_sprites; paso de 10 bytes */
    memset(cga_mem, 0, CGA_MEM_SIZE); jump_anim_counter = 0x0e; jump_draw_y = jump_y;
    draw_fish_line(); row_bytes(0x68, jump_y, got);
    CHECK(memcmp(got, desc_row0, 8) == 0, "bajada fila 0 distinta");
    jump_draw_y = (uint8_t)(jump_y + 1); jump_anim_counter = 0x0d;
    draw_fish_line(); row_bytes(0x68, jump_y + 1, got);
    CHECK(memcmp(got, desc_row0, 8) == 0, "bajada fila 1 distinta (paso de 10 bytes mal)");
    /* la fila 2 empieza en el byte 20 del label (cat.asm): 55 55 f5 55 55 55 55 55 | 2f ec (relleno) */
    static const uint8_t desc_row2[8] = {0x55, 0x55, 0xf5, 0x55, 0x55, 0x55, 0x55, 0x55};
    jump_draw_y = (uint8_t)(jump_y + 2); jump_anim_counter = 0x0c;
    draw_fish_line(); row_bytes(0x68, jump_y + 2, got);
    CHECK(memcmp(got, desc_row2, 8) == 0, "bajada fila 2 distinta (%02x %02x ...)", got[0], got[1]);
}

static void test_idle_aggro(void) {
    cga_init(); bios_clock_hook = hook; level_number = 0; difficulty_level = 0;
    struct { uint16_t age; uint8_t force; uint8_t want; const char *what; } c[] = {
        {0x50, 0, 1, "ocioso >= 0x48 ticks"}, {0x10, 0, 0, "reciente < 0x48"},
        {0x48, 0, 1, "justo 0x48"}, {0x50, 1, 0, "force_level7 desactiva el aggro"},
    };
    for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
        fake_tick = 1000; game_mode = 1; mode_start_tick = (uint16_t)(fake_tick - c[i].age);
        force_level7 = c[i].force; jump_tick_delay = 1; jump_anim_counter = 0; gravity_y = 0;
        cat_y = 0x60; idle_aggro_flag = 0xAA;
        update_cat_jump();
        /* tras el calculo el ASM vuelve a usarlo para elegir el spawn; idle_aggro_flag queda 0/1 */
        CHECK(idle_aggro_flag == c[i].want, "%s: idle_aggro_flag=%u esperado %u", c[i].what, idle_aggro_flag, c[i].want);
    }
    /* sin reloj propio: el tick de 16 bits da la vuelta (sub dx,[0x556]) */
    fake_tick = 0x0010; game_mode = 1; mode_start_tick = 0xffe0; force_level7 = 0;
    jump_tick_delay = 1; jump_anim_counter = 0; gravity_y = 0; cat_y = 0x60;
    update_cat_jump();
    CHECK(idle_aggro_flag == 0, "wrap de 16 bits: dt=0x30 < 0x48 deberia dar 0, da %u", idle_aggro_flag);
    force_level7 = 0;
}

static void test_entry_steps(void) {
    /* check_enemy_activate: cmp byte [0x558],0 / jnz -> false */
    level_number = 1; enemy_active = 0; enemy_chasing = 1; enemy_approach_timer = 0; enemy_exit_timer = 0;
    cat_y = 0xb0; enemy_x = 0x80; cat_x = 0x90;
    entry_steps = 3; CHECK(!check_enemy_activate(), "entry_steps=3 no debe activar la persecucion");
    entry_steps = 0; enemy_chasing = 1; enemy_active = 0;
    CHECK(check_enemy_activate(), "entry_steps=0 debe activar la persecucion");

    /* puerta de aparicion (lab_1f0c): con entry_steps != 0 el perro nunca aparece; con 0 si */
    cga_init(); bios_clock_hook = hook; level_number = 0; difficulty_level = 7; lives_count = 3;
    for (int pass = 0; pass < 2; pass++) {
        init_sound(); silence_speaker(); fall_hit = 0; cat_y = 0xb8; cat_x = 0x30; enemy_last_tick = 0;
        enemy_tick_counter = 0; entry_steps = pass == 0 ? 2 : 0; fake_tick = 100;
        bool spawned = false;
        for (int t = 0; t < 3000 && !spawned; t++) {
            fake_tick = (uint16_t)(fake_tick + 3); update_enemies();
            if (enemy_approach_timer != 0 || enemy_chasing != 0) spawned = true;
        }
        CHECK(spawned == (pass == 1), "entry_steps=%u: spawned=%d", pass == 0 ? 2u : 0u, (int)spawned);
    }
    entry_steps = 0;
}

int main(void) {
    test_draw();
    test_idle_aggro();
    test_entry_steps();
    if (fails) { printf("test_fish_flags: %d FALLO(S)\n", fails); return 1; }
    printf("test_fish_flags: OK\n");
    return 0;
}
