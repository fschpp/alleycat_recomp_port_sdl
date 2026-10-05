/* T21 — helpers A del nivel 4 (level_objects.asm L1897-1932, L2094-2149). Sin SDL.
 * Los valores esperados NO salen de la lectura del ASM sino de ejecutar las rutinas originales
 * (ensambladas con nasm) en un emulador x86-16 (unicorn); ver PROGRESS.md §6r. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level4.h"
#include "level45_state.h"
#include "level_collision.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void) {
    cga_init();

    /* calc_l4_obj_pos: los 16 frames (y = l4_platform_offset - (f>=3 ? 0xa : 0) + 3; x = x_table + 8) */
    static const uint16_t dims[16] = {88,112,128,24,72,120,40,80,200,96,152,136,48,104,32,192};
    static const uint8_t  ypos[16] = {35,35,43,65,57,73,97,89,97,113,129,121,137,145,153,153};
    for (uint16_t f = 0; f < 16; f++) {
        l5_obj_frame[2] = f; calc_l4_obj_pos(2);
        CHECK(l5_obj_dims[2] == dims[f] && l5_obj_y_pos[2] == ypos[f], "calc frame %u: dims=%u y=%u", f, l5_obj_dims[2], l5_obj_y_pos[2]);
    }

    /* init_level4_objects con semilla fija: frames, x, y, anim y semilla final del ASM */
    static const struct { uint16_t seed; int16_t cx; uint8_t cy; uint16_t fr[4], dm[4]; uint8_t yp[4], an[4]; uint16_t seed_end; } g[3] = {
        {0xFA59, 100, 100, {4,2,11,12},  {72,128,136,48},  {57,43,121,137}, {30,29,25,26}, 53754},
        {0x1234, 160,  72, {4,1,6,10},   {72,112,40,152},  {57,35,97,129},  {22,28,23,33}, 37650},
        {0xBEEF,  16, 180, {15,11,13,7}, {192,136,104,80}, {153,121,145,89},{35,33,34,31}, 51759},
    };
    for (int k = 0; k < 3; k++) {
        memset(l5_obj_frame, 0, sizeof l5_obj_frame);
        rng_seed = g[k].seed; cat_x = g[k].cx; cat_y = g[k].cy;
        init_level4_objects();
        for (int i = 0; i < 4; i++) {
            CHECK(l5_obj_frame[i] == g[k].fr[i] && l5_obj_dims[i] == g[k].dm[i] && l5_obj_y_pos[i] == g[k].yp[i] &&
                  l5_obj_anim[i] == g[k].an[i] && l5_obj_active[i] == 1 && l5_obj_hit[i] == 0,
                  "init caso %d obj %d: frame=%u dims=%u y=%u anim=%u", k, i, l5_obj_frame[i], l5_obj_dims[i], l5_obj_y_pos[i], l5_obj_anim[i]);
        }
        CHECK(rng_seed == g[k].seed_end, "init caso %d: semilla final %u != %u (nº de random() distinto)", k, rng_seed, g[k].seed_end);
        CHECK(l5_obj_index == 0 && l5_obj_count == 4, "init caso %d: index/count", k);
        for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++)
            CHECK(l5_obj_frame[i] != l5_obj_frame[j], "init caso %d: frames repetidos %d/%d", k, i, j);
    }

    /* check_l4_proximity: |dx|+|dy| (con `not`, no neg) < bp, en ambos órdenes y con wrap */
    l5_obj_dims[0] = 100; l5_obj_y_pos[0] = 50; cat_x = 100; cat_y = 50;
    CHECK(check_l4_proximity(0, 1) && !check_l4_proximity(0, 0), "proximidad exacta: 0 < 1, !(0 < 0)");
    cat_x = 90; cat_y = 50;   CHECK(!check_l4_proximity(0, 10) && check_l4_proximity(0, 11), "dx=+10");
    cat_x = 110; cat_y = 50;  /* préstamo: ax = ~(100-110) = 9 (no 10) */
    CHECK(check_l4_proximity(0, 10) && !check_l4_proximity(0, 9), "dx=-10 se mide 9 por el `not`");

    /* check_l4_thrown_collision con thrown_obj_x >= 0x8000 (wrap sin signo, corregido en T21) */
    cat_x = 11; cat_y = 68; thrown_obj_x = (int16_t)65519; thrown_obj_y = 79;
    CHECK(!check_l4_thrown_collision(), "x=0xFFEF no debe chocar con cat_x=11 (sin signo)");
    cat_x = 100; cat_y = 100; thrown_obj_x = 100; thrown_obj_y = 100;
    CHECK(check_l4_thrown_collision(), "solape exacto");

    /* erase_level4_sprite: solo restaura si active == 0 (blit a CGA 0x100 de 12 filas, 2 words) */
    memset(cga_mem, 0, sizeof cga_mem);
    for (int i = 0; i < 48; i++) l5_obj_save_buf[1][i] = 0x5a;
    l5_obj_index = 1; l5_obj_cga_addr[1] = 0x100;
    l5_obj_active[1] = 1; erase_level4_sprite();
    CHECK(cga_mem[0x100] == 0, "activo != 0: no restaura");
    l5_obj_active[1] = 0; erase_level4_sprite();
    CHECK(cga_mem[0x100] == 0x5a, "activo == 0: restaura");

    printf(fails ? "RESULT: %d FAIL\n" : "RESULT: all OK\n", fails);
    return fails != 0;
}
