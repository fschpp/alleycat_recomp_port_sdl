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


/* ---- T23: update_level4_anim ---- (los restore/save_alley_buffer y start_tone se interceptan con
 * -Wl,--wrap en el Makefile para registrar el orden de llamadas) */
static char alog[128]; static int alogn;
void __wrap_restore_alley_buffer(void) { alogn += snprintf(alog + alogn, sizeof alog - (size_t)alogn, "R "); }
void __wrap_save_alley_buffer(void)    { alogn += snprintf(alog + alogn, sizeof alog - (size_t)alogn, "S "); }
void __wrap_start_tone(uint16_t a, uint16_t b) { alogn += snprintf(alog + alogn, sizeof alog - (size_t)alogn, "T%x,%x ", a, b); }

static uint32_t crc32_(const uint8_t *p, size_t n) {
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1))); }
    return ~c;
}
typedef struct { uint8_t hit, act, anim; uint16_t frame, dims; uint8_t y; uint16_t addr; } obj_t;
typedef struct {
    const char *name;
    uint16_t tick, last, idx; uint8_t cnt, atp, delay; uint16_t diff, seed; uint16_t cx; uint8_t cy; uint16_t tx; uint8_t ty; uint8_t pad;
    obj_t o[4];
    uint16_t e_last, e_idx; uint8_t e_cnt, e_caught, e_atp; uint16_t e_sprite, e_3de4, e_seed; uint32_t e_crc, e_sbcrc; const char *e_log;
    obj_t eo[4];
} anim_case_t;

/* Valores esperados = ejecutar update_level4_anim REAL (nasm + unicorn x86-16) con la CGA inicial
 * cga_mem[i] = i*7+3 y los buffers de guardado k*37+i*11+5 (ver PROGRESS.md §6t). */
static const anim_case_t anim_cases[] = {
    {"mismo tick: retorno inmediato",
     77,77,0,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     77,0,4,0,1,0,0,64089,0x72a4967a,0xc184abb3,"",
     {{0,1,16,4,72,57,1280},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"anim 5 (impar): sprite dat_3de0[2]=0x3d20",
     100,50,0,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,15696,9904,64089,0xc9977639,0xe47cb611,"",
     {{0,1,16,4,72,57,1280},{0,0,4,2,128,43,9904},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"anim 0x10 (par): sprite dat_3de0[0]=0x3d50",
     100,50,0,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,17,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,15696,9904,64089,0xc9977639,0xe47cb611,"",
     {{0,1,16,4,72,57,1280},{0,0,16,2,128,43,9904},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"anim 1 -> 0: sprite 0x3db0",
     100,50,0,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,1,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,15792,1872,64089,0x5191e154,0x8a415593,"",
     {{0,1,16,4,72,57,1280},{0,0,0,2,128,46,1872},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"anim 2 -> 1: sprite 0x3d80",
     100,50,0,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,2,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,15744,1872,64089,0x632351da,0x8a415593,"",
     {{0,1,16,4,72,57,1280},{0,0,1,2,128,46,1872},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"anim 0 -> randomiza posicion y anim",
     100,50,0,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,0,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,0,3930,16203,0x72a4967a,0xc184abb3,"",
     {{0,1,16,4,72,57,1280},{0,1,22,6,40,98,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"anim 0x13 -> 0x12: sprite 0x3d80",
     100,50,0,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,19,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,15744,1872,64089,0x632351da,0x8a415593,"",
     {{0,1,16,4,72,57,1280},{0,0,18,2,128,46,1872},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"anim 0x14 -> 0x13: sprite 0x3db0",
     100,50,0,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,20,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,15792,1872,64089,0x5191e154,0x8a415593,"",
     {{0,1,16,4,72,57,1280},{0,0,19,2,128,46,1872},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"anim 0x15 -> 0x14: no se dibuja (ptr 0, active=1)",
     100,50,0,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,21,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,0,1872,64089,0x72a4967a,0xc184abb3,"",
     {{0,1,16,4,72,57,1280},{0,1,20,2,128,46,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"gato toca, active=1: no hace nada",
     100,50,0,4,1,0,0,64089,128,43,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,0,0,64089,0x72a4967a,0xc184abb3,"",
     {{0,1,16,4,72,57,1280},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"gato toca, active=0, anim<0x14: atrapa, icono 1/4",
     100,50,0,4,1,0,0,64089,128,43,310,175,0,
     {{0,1,16,4,72,57,1280},{0,0,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,3,0,0,0,0,64089,0xd63af545,0xc184abb3,"R S T3e8,2ee ",
     {{0,1,16,4,72,57,1280},{1,0,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"gato toca el ultimo (count 1): cat_caught=1",
     100,50,0,1,1,0,0,64089,128,43,310,175,0,
     {{0,1,16,4,72,57,1280},{0,0,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,0,1,0,0,0,64089,0xfecc6175,0xc184abb3,"R S T3e8,2ee ",
     {{0,1,16,4,72,57,1280},{1,0,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"gato toca, active=0 pero anim>=0x14: nada",
     100,50,0,4,1,0,0,64089,128,43,310,175,0,
     {{0,1,16,4,72,57,1280},{0,0,21,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,0,0,64089,0x72a4967a,0xc184abb3,"",
     {{0,1,16,4,72,57,1280},{0,0,21,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"objeto ya atrapado (hit=1): nada",
     100,50,0,4,1,0,0,64089,128,43,310,175,0,
     {{0,1,16,4,72,57,1280},{1,0,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,0,0,64089,0x72a4967a,0xc184abb3,"",
     {{0,1,16,4,72,57,1280},{1,0,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"lanzado solapa: se queda como esta",
     100,50,0,4,1,0,0,64089,300,170,128,43,0,
     {{0,1,16,4,72,57,1280},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,0,0,64089,0x72a4967a,0xc184abb3,"",
     {{0,1,16,4,72,57,1280},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"idx 3 -> 4 -> 0: procesa obj 0 y actualiza tick",
     100,50,3,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,0,4,0,1,15648,10450,64089,0x6f6aedbb,0xe9361711,"",
     {{0,0,15,4,72,57,10450},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
    {"idx 2 -> 3: procesa obj 3, NO actualiza last_tick",
     100,50,2,4,1,0,0,64089,300,170,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     50,3,4,0,1,15648,13644,64089,0xdc56cbf6,0xce2f1bb9,"",
     {{0,1,16,4,72,57,1280},{0,1,5,2,128,43,1280},{0,1,15,11,136,121,1280},{0,0,9,12,48,137,13644}}},
    {"dificultad 5: umbral 140 (cerca del gato: anim a 1)",
     100,50,0,4,1,0,5,64089,178,43,310,175,0,
     {{0,1,16,4,72,57,1280},{0,1,8,2,128,43,1280},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}},
     100,1,4,0,1,15744,1872,64089,0x632351da,0x8a415593,"",
     {{0,1,16,4,72,57,1280},{0,0,1,2,128,46,1872},{0,1,15,11,136,121,1280},{0,1,10,12,48,137,1280}}},
};

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


    for (size_t k = 0; k < sizeof anim_cases / sizeof anim_cases[0]; k++) {
        const anim_case_t *a = &anim_cases[k];
        for (size_t i = 0; i < sizeof cga_mem; i++) cga_mem[i] = (uint8_t)(i * 7 + 3);
        for (int i = 0; i < 4; i++) {
            l5_obj_hit[i] = a->o[i].hit; l5_obj_active[i] = a->o[i].act; l5_obj_anim[i] = a->o[i].anim;
            l5_obj_frame[i] = a->o[i].frame; l5_obj_dims[i] = a->o[i].dims; l5_obj_y_pos[i] = a->o[i].y;
            l5_obj_cga_addr[i] = a->o[i].addr;
            for (int b = 0; b < 48; b++) l5_obj_save_buf[i][b] = (uint8_t)(b * 37 + i * 11 + 5);
        }
        l4_tick_override = a->tick; l5_last_tick = a->last; l5_obj_index = a->idx; l5_obj_count = a->cnt;
        cat_caught = 0; at_platform = a->atp; l5_anim_delay = a->delay; difficulty_level = a->diff;
        rng_seed = a->seed; cat_x = (int16_t)a->cx; cat_y = a->cy; thrown_obj_x = (int16_t)a->tx; thrown_obj_y = a->ty;
        l5_obj_sprite_ptr = 0; l4_dat_3de4 = 0; alogn = 0; alog[0] = 0;
        update_level4_anim();
        CHECK(l5_last_tick == a->e_last && l5_obj_index == a->e_idx && l5_obj_count == a->e_cnt &&
              cat_caught == a->e_caught && at_platform == a->e_atp,
              "anim '%s': last=%u idx=%u cnt=%u caught=%u atp=%u", a->name, l5_last_tick, l5_obj_index, l5_obj_count, cat_caught, at_platform);
        CHECK(l5_obj_sprite_ptr == a->e_sprite && l4_dat_3de4 == a->e_3de4, "anim '%s': sprite=0x%x addr=%u", a->name, l5_obj_sprite_ptr, l4_dat_3de4);
        CHECK(rng_seed == a->e_seed, "anim '%s': semilla %u != %u (nº de random())", a->name, rng_seed, a->e_seed);
        CHECK(crc32_(cga_mem, sizeof cga_mem) == a->e_crc, "anim '%s': CGA distinta", a->name);
        uint8_t sb[192]; for (int i = 0; i < 4; i++) memcpy(sb + 48 * i, l5_obj_save_buf[i], 48);
        CHECK(crc32_(sb, sizeof sb) == a->e_sbcrc, "anim '%s': buffers de guardado distintos", a->name);
        CHECK(strcmp(alog, a->e_log) == 0, "anim '%s': llamadas '%s' != '%s'", a->name, alog, a->e_log);
        for (int i = 0; i < 4; i++)
            CHECK(l5_obj_hit[i] == a->eo[i].hit && l5_obj_active[i] == a->eo[i].act && l5_obj_anim[i] == a->eo[i].anim &&
                  l5_obj_frame[i] == a->eo[i].frame && l5_obj_dims[i] == a->eo[i].dims && l5_obj_y_pos[i] == a->eo[i].y &&
                  l5_obj_cga_addr[i] == a->eo[i].addr, "anim '%s': objeto %d distinto", a->name, i);
    }
    l4_tick_override = -1;

    printf(fails ? "RESULT: %d FAIL\n" : "RESULT: all OK\n", fails);
    return fails != 0;
}
