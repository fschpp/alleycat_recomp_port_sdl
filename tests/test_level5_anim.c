/* T27 — update_level5_anim (level_objects.asm L2198-2361). Sin SDL.
 * Los valores esperados vienen de un modelo Python INDEPENDIENTE escrito directamente desde el ASM
 * (no del C ni de un emulador x86, que no hay aquí); ver PROGRESS.md §6x. play_random_chirp se
 * intercepta con -Wl,--wrap para contar las llamadas. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level5.h"
#include "gen/ds_pool.h"
#include "test_level5_anim.inc"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static int chirps;
void __wrap_play_random_chirp(void) { chirps++; }

static uint8_t bg[CGA_MEM_SIZE];

static void reset(uint16_t diff, uint16_t seed, int16_t cx, uint8_t cy) {
    for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + 3);
    memcpy(bg, cga_mem, CGA_MEM_SIZE);
    l5_dat_40b2 = 0x80; l5_dat_40b4 = 0x70; l5_dat_40b5 = 0; l5_dat_40ff = 0; l5_dat_40aa = 0xa4;
    l5_dat_40b7 = 0; l5_dat_40b8 = 0; l5_dat_40c8 = 0xff; l5_dat_40b9 = 1; l5_dat_40be = 0;
    l5_dat_40ba = 0; l5_dat_40bc = 0;
    memset(l5_dat_3f2c, 0, sizeof l5_dat_3f2c);
    cat_x = cx; cat_y = cy; object_hit = 0; cat_caught = 0;
    thrown_obj_x = 300; thrown_obj_y = 0xf0;          /* lejos del objeto */
    difficulty_level = diff; rng_seed = seed; chirps = 0;
}
static void tick(int32_t t) { l5_tick_override = t; update_level5_anim(); }

/* sprite 1 word x 5 filas sobre el fondo `bg`: píxel 0 deja pasar el fondo, != 0 es opaco */
static uint16_t expect_word(uint16_t src, uint16_t dst) {
    uint16_t r = 0;
    for (int sh = 0; sh < 16; sh += 2) { uint16_t m = (uint16_t)(3u << sh); r |= (src & m) ? (src & m) : (dst & m); }
    return r;
}

int main(void) {
    /* 1) retornos tempranos */
    reset(3, 0x1234, 0x120, 0x20);
    tick(5); CHECK(l5_dat_40ff == 1 && l5_dat_40b5 == 5, "perch alto: ff=%u b5=%u", l5_dat_40ff, l5_dat_40b5);
    /* dat_40aa = 0xa4 -> procesa; pone a 0x86 para probar el corte por perch */
    reset(3, 0x1234, 0x120, 0x20); l5_dat_40aa = 0x86;
    tick(5);
    CHECK(l5_dat_40ff == 1 && l5_dat_40b5 == 5 && rng_seed == 0x1234, "perch 0x86: ff=%u b5=%u seed=%04x", l5_dat_40ff, l5_dat_40b5, rng_seed);
    CHECK(memcmp(bg, cga_mem, CGA_MEM_SIZE) == 0, "perch 0x86 dibujó algo");
    tick(5);   /* mismo tick: ni siquiera cuenta */
    CHECK(l5_dat_40ff == 1, "mismo tick incrementó ff");
    reset(3, 0x1234, 0x120, 0x20); l5_dat_40aa = 0x86; l5_dat_40ff = 3;
    tick(9);   /* ff pasa a 4: (4 & 3) == 0 -> NO guarda el tick */
    CHECK(l5_dat_40ff == 4 && l5_dat_40b5 == 0, "cada 4.º tick no se guarda: ff=%u b5=%u", l5_dat_40ff, l5_dat_40b5);
    tick(9);   /* mismo tick, pero como no se guardó se procesa otra vez */
    CHECK(l5_dat_40ff == 5 && l5_dat_40b5 == 9, "mismo tick se reprocesa: ff=%u b5=%u", l5_dat_40ff, l5_dat_40b5);
    reset(3, 0x1234, 0x120, 0x20); thrown_obj_x = 0x80; thrown_obj_y = 0x70;   /* solapa al objeto */
    tick(1);
    CHECK(l5_dat_40b8 == 0xff && rng_seed == 0x1234 && l5_dat_40b9 == 1, "lanzado solapa: b8=%02x seed=%04x b9=%u", l5_dat_40b8, rng_seed, l5_dat_40b9);
    CHECK(memcmp(bg, cga_mem, CGA_MEM_SIZE) == 0, "lanzado solapa: dibujó algo");

    /* 2) primer tick del caso A: dibujo exacto de la fila 0 (0x11a0 = 112*0x28 + 0x80/4; sprite 0x3efa) */
    reset(CASE_A_DIFF, CASE_A_SEED, CASE_A_CATX, CASE_A_CATY);
    tick(1);
    CHECK(l5_dat_40bc == 0x11a0 && l5_dat_40ba == 0x11a0, "A tick1: bc=%x ba=%x", l5_dat_40bc, l5_dat_40ba);
    {
        uint16_t src = (uint16_t)(ds_pool[0x3efa] | (ds_pool[0x3efb] << 8));
        uint16_t dst = (uint16_t)(bg[0x11a0] | (bg[0x11a1] << 8));
        uint16_t got = (uint16_t)(cga_mem[0x11a0] | (cga_mem[0x11a1] << 8));
        CHECK(got == expect_word(src, dst), "A tick1 fila 0: %04x != %04x", got, expect_word(src, dst));
        CHECK(l5_dat_3f2c[0] == dst, "A tick1 guardó el fondo: %04x != %04x", l5_dat_3f2c[0], dst);
    }

    /* 3) secuencias completas contra el modelo, y que el borrado no deje residuo */
    struct { const char *n; const uint16_t (*seq)[7]; const uint32_t *fin; uint16_t diff, seed; int16_t cx; uint8_t cy; } sc[] = {
        {"A", seq_A, fin_A, CASE_A_DIFF, CASE_A_SEED, CASE_A_CATX, CASE_A_CATY},
        {"B", seq_B, fin_B, CASE_B_DIFF, CASE_B_SEED, CASE_B_CATX, CASE_B_CATY},
    };
    for (unsigned c = 0; c < 2; c++) {
        reset(sc[c].diff, sc[c].seed, sc[c].cx, sc[c].cy);
        for (int i = 0; i < 150; i++) {
            tick(1 + (i * 2) / 3);
            const uint16_t *e = sc[c].seq[i];
            uint16_t g[7] = { (uint16_t)l5_dat_40b2, l5_dat_40b4, l5_dat_40b7, l5_dat_40b8, l5_dat_40c8, l5_dat_40ff, l5_dat_40b5 };
            if (memcmp(g, e, sizeof g) != 0) {
                CHECK(0, "%s paso %d: b2,b4,b7,b8,c8,ff,b5 = %u,%u,%u,%u,%u,%u,%u esperado %u,%u,%u,%u,%u,%u,%u", sc[c].n, i,
                      g[0], g[1], g[2], g[3], g[4], g[5], g[6], e[0], e[1], e[2], e[3], e[4], e[5], e[6]);
                break;
            }
        }
        const uint32_t *f = sc[c].fin;
        CHECK(l5_dat_40be == f[7] && l5_dat_40ba == f[8] && l5_dat_40bc == f[9], "%s final be/ba/bc = %u,%u,%u", sc[c].n, l5_dat_40be, l5_dat_40ba, l5_dat_40bc);
        CHECK(rng_seed == f[10], "%s: rng_seed final %u != %u (orden/número de random)", sc[c].n, rng_seed, f[10]);
        CHECK((uint32_t)chirps == f[12], "%s: chirps %d != %u", sc[c].n, chirps, f[12]);
        CHECK(cat_caught == f[13], "%s: cat_caught %u != %u", sc[c].n, cat_caught, f[13]);
        blit_to_cga((const uint8_t *)l5_dat_3f2c, l5_dat_40ba, 1, 5);    /* borra el último dibujo */
        CHECK(memcmp(bg, cga_mem, CGA_MEM_SIZE) == 0, "%s: queda residuo en la CGA tras borrar el último dibujo", sc[c].n);
    }

    /* 4) casos largos: límites de movimiento (X 0..0x135, Y 0x30..0xa7) y estado final */
    for (unsigned c = 0; c < sizeof long_cases / sizeof long_cases[0]; c++) {
        const uint32_t *l = long_cases[c];
        reset((uint16_t)l[0], (uint16_t)l[1], (int16_t)l[2], (uint8_t)l[3]);
        uint32_t minx = 0xffff, maxx = 0, miny = 0xffff, maxy = 0;
        for (int i = 0; i < 1500; i++) {
            tick(1 + (i * 2) / 3);
            if (l5_dat_40b2 < minx) minx = l5_dat_40b2;
            if (l5_dat_40b2 > maxx) maxx = l5_dat_40b2;
            if (l5_dat_40b4 < miny) miny = l5_dat_40b4;
            if (l5_dat_40b4 > maxy) maxy = l5_dat_40b4;
        }
        CHECK(minx == l[4] && maxx == l[5] && miny == l[6] && maxy == l[7], "largo %u: rangos x %u..%u y %u..%u esperado %u..%u %u..%u", c, minx, maxx, miny, maxy, l[4], l[5], l[6], l[7]);
        CHECK(maxx <= 0x135 && miny >= 0x30 && maxy <= 0xa7, "largo %u fuera de límites", c);
        CHECK((uint32_t)chirps == l[8], "largo %u: chirps %d != %u", c, chirps, l[8]);
        CHECK(l5_dat_40b2 == l[9] && l5_dat_40b4 == l[10] && l5_dat_40b7 == l[11] && l5_dat_40b8 == l[12] && l5_dat_40c8 == l[13] &&
              l5_dat_40ff == l[14] && l5_dat_40b5 == l[15] && l5_dat_40be == l[16] && l5_dat_40ba == l[17] && rng_seed == l[18],
              "largo %u: estado final distinto (b2=%u b4=%u b7=%u b8=%u c8=%u ff=%u b5=%u be=%u ba=%u seed=%u)", c,
              l5_dat_40b2, l5_dat_40b4, l5_dat_40b7, l5_dat_40b8, l5_dat_40c8, l5_dat_40ff, l5_dat_40b5, l5_dat_40be, l5_dat_40ba, rng_seed);
    }

    if (fails) { printf("%d FAIL\n", fails); return 1; }
    printf("test_level5_anim: OK\n");
    return 0;
}
