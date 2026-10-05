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


    /* ---- T22: helpers B (check_l4_obj_cat / check_l4_obj_thrown) ---- */
    /* Objeto 2: x=100, y=50 -> rect 0x10 x 0x0c. Contra el gato (0x18 x 0x0e) choca con cat_x en
     * [76,116] y cat_y en [36,62]; contra el lanzado (0x10 x 0x1e): thrown_x en [84,116], thrown_y en [20,62]. */
    l5_obj_dims[2] = 100; l5_obj_y_pos[2] = 50;
    cat_y = 50;
    cat_x = 76;  CHECK(check_l4_obj_cat(2),  "obj-gato: borde izquierdo (76) debe chocar");
    cat_x = 75;  CHECK(!check_l4_obj_cat(2), "obj-gato: 75 no choca");
    cat_x = 116; CHECK(check_l4_obj_cat(2),  "obj-gato: borde derecho (116) debe chocar");
    cat_x = 117; CHECK(!check_l4_obj_cat(2), "obj-gato: 117 no choca");
    cat_x = 100;
    cat_y = 36;  CHECK(check_l4_obj_cat(2),  "obj-gato: y=36 choca");
    cat_y = 35;  CHECK(!check_l4_obj_cat(2), "obj-gato: y=35 no choca");
    cat_y = 62;  CHECK(check_l4_obj_cat(2),  "obj-gato: y=62 choca");
    cat_y = 63;  CHECK(!check_l4_obj_cat(2), "obj-gato: y=63 no choca");
    thrown_obj_y = 50;
    thrown_obj_x = 84;  CHECK(check_l4_obj_thrown(2),  "obj-lanzado: x=84 choca");
    thrown_obj_x = 83;  CHECK(!check_l4_obj_thrown(2), "obj-lanzado: x=83 no choca");
    thrown_obj_x = 116; CHECK(check_l4_obj_thrown(2),  "obj-lanzado: x=116 choca");
    thrown_obj_x = 117; CHECK(!check_l4_obj_thrown(2), "obj-lanzado: x=117 no choca");
    thrown_obj_x = 100;
    thrown_obj_y = 20;  CHECK(check_l4_obj_thrown(2),  "obj-lanzado: y=20 choca");
    thrown_obj_y = 19;  CHECK(!check_l4_obj_thrown(2), "obj-lanzado: y=19 no choca");
    thrown_obj_y = 62;  CHECK(check_l4_obj_thrown(2),  "obj-lanzado: y=62 choca");
    thrown_obj_y = 63;  CHECK(!check_l4_obj_thrown(2), "obj-lanzado: y=63 no choca");
    /* x del lanzado >= 0x8000 (sin signo, como el fix de T21) */
    thrown_obj_x = (int16_t)65519; thrown_obj_y = 50;
    CHECK(!check_l4_obj_thrown(2), "obj-lanzado: 0xFFEF no choca con un objeto en x=100");

    /* ---- T22: cola de init_level4_bg (dat_3cc3 -> dat_3ce3/3cf3), valores del emulador x86 ---- */
    static const struct { uint16_t diff; const char *ce3; } tl[3] = {
        {0, "0f06090b0a0c010e0d02040305080700"},
        {1, "0e0b060f0c0a02090d07050104080003"},   /* difficulty 5 usa el mismo grupo (5 & 3 == 1) */
        {3, "020d000e0f06050b0c0a090708010304"},
    };
    for (int k = 0; k < 3; k++) {
        memset(l4_dat_3ce3, 0xEE, sizeof l4_dat_3ce3); memset(l4_dat_3cf3, 0xEE, sizeof l4_dat_3cf3);
        for (int d = 0; d < 2; d++) {
            uint16_t diff = tl[k].diff == 1 && d ? 5 : tl[k].diff;
            difficulty_level = diff; init_level4_bg_tail();
            char got[33]; for (int i = 0; i < 16; i++) snprintf(got + 2 * i, 3, "%02x", l4_dat_3ce3[i]);
            CHECK(strncmp(got, tl[k].ce3, 32) == 0, "cola init_level4_bg diff=%u: %s != %.32s", diff, got, tl[k].ce3);
            for (int i = 0; i < 16; i++) CHECK(l4_dat_3cf3[i] == 0, "cola diff=%u: dat_3cf3[%d] != 0", diff, i);
        }
    }

    printf(fails ? "RESULT: %d FAIL\n" : "RESULT: all OK\n", fails);
    return fails != 0;
}
