/* T36 — animate_level2_blocks / update_entrance_anim (level_objects.asm L1017-1099) y el fondo del nivel 2 (score.asm L159-191,
 * que llena level2_block_types). Sin SDL. Modelo independiente escrito desde el ASM (LFSR propio, pantalla modelo con intercalado
 * de bancos CGA, rect_collision propio). Ver PROGRESS.md §6ag. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level2.h"
#include "level_background.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint16_t tick_now;
static uint16_t fake_tick(void) { return tick_now; }

static uint16_t mseed;
static uint8_t m_random(void) {
    uint8_t lo = (uint8_t)(mseed & 0xff), hi = (uint8_t)(mseed >> 8);
    unsigned c = (unsigned)(((lo ^ hi) >> 1) & 1);
    mseed = (uint16_t)((mseed >> 1) | (c << 15));
    return (uint8_t)(mseed & 0xff);
}
static uint8_t screen[CGA_MEM_SIZE];
static void m_blit(const uint8_t *src, uint16_t start, unsigned wwords, unsigned rows) {
    size_t a = start;
    for (unsigned r = 0; r < rows; r++) {
        memcpy(&screen[a], src + r * wwords * 2, wwords * 2);
        a ^= 0x2000; if (!(a & 0x2000)) a += 80;
    }
}
static bool m_rect(uint16_t ax, uint8_t dl, uint16_t si, uint8_t cl, uint16_t bx, uint8_t dh, uint16_t di, uint8_t ch) {
    uint32_t r = (uint32_t)ax + si; if (r > 0xffff || (uint16_t)r < bx) return false;
    int32_t a = (int32_t)ax - di; if (a < 0) a = 0;
    if ((uint16_t)a > bx) return false;
    uint16_t b = (uint16_t)dl + cl; if (b > 0xff || (uint8_t)b < dh) return false;
    int32_t c = (int32_t)dl - ch; if (c < 0) c = 0;
    return (uint8_t)c <= dh;
}

/* ---- modelo de animate_level2_blocks ---- */
static uint8_t mt[0x28]; static uint16_t m350d, m350f;
static void m_animate(uint16_t tick, int16_t cx_, uint8_t cy_) {
    if ((uint16_t)(tick - m350f) < 8) return;
    m350d++;
    unsigned bx = m350d;
    if (bx >= 0x28) { bx = 0; m350d = 0; m350f = tick; }
    unsigned di = bx * 2;
    if (cy_ <= 7) {
        uint16_t ax = (uint16_t)(((uint16_t)cx_ >> 2) + 1);
        ax = (uint16_t)(ax - di);
        if ((uint16_t)((uint16_t)(((uint16_t)cx_ >> 2) + 1)) < di) ax = (uint16_t)~ax;   /* sub ax,di; jnb / not ax */
        if (ax < 4) return;
    }
    mt[bx] = (uint8_t)(mt[bx] + 8);
    m_blit(&ds_pool[0x2020 + (mt[bx] & 0x18)], (uint16_t)(0xa0 + di), 1, 4);
}
/* ---- modelo de update_entrance_anim ---- */
static uint16_t m35d8, m35da; static uint16_t mlc;
static const uint16_t ENT[4] = {0x3530, 0x3558, 0x3580, 0x35a8};
static void m_entrance(uint16_t tick, int16_t cx_, uint8_t cy_) {
    if ((uint16_t)(tick - m35da) < 6) return;
    m35da = tick; m35d8 += 2;
    m_blit(&ds_pool[ENT[(m35d8 & 6) >> 1]], 0x15c9, 2, 10);
    if (m_rect(0xe4, 0x8a, 0x10, 0x0a, (uint16_t)cx_, cy_, 0x18, 0xe)) mlc = (uint16_t)((mlc & 0xff00) | 1);
}

int main(void) {
    l2_tick_fn = fake_tick;
    uint32_t rs = 4242;
    #define RND() (rs = rs * 1664525u + 1013904223u, (rs >> 8))

    /* 0. el fondo del nivel 2: bloques en 0xa0 + 2*k, 1 palabra x 4 filas, tipos registrados en level2_block_types */
    for (int it = 0; it < 200; it++) {
        level_number = 2; rng_seed = (uint16_t)(RND() | 1); mseed = rng_seed;
        memset(cga_mem, 0x33, CGA_MEM_SIZE); memset(l2_block_types, 0x77, sizeof l2_block_types);
        memset(screen, 0x33, sizeof screen);
        for (int i = 0; i < 160; i++) { screen[i] = 0xaa; screen[0x2000 + i] = 0xaa; }
        uint8_t prev = 0xff;
        for (int k = 0; k < 0x28; k++) {
            uint8_t dl;
            do { dl = m_random() & 0x18; } while (dl == prev);
            prev = dl; mt[k] = dl;
            m_blit(&ds_pool[0x2020 + dl], (uint16_t)(0xa0 + 2 * k), 1, 4);
        }
        draw_level_background();
        CHECK(memcmp(cga_mem, screen, CGA_MEM_SIZE) == 0, "fondo it %d: pantalla", it);
        CHECK(memcmp(l2_block_types, mt, 0x28) == 0, "fondo it %d: level2_block_types", it);
        CHECK(rng_seed == mseed, "fondo it %d: estado del RNG", it);
        if (fails) break;
    }

    /* 1. animate_level2_blocks: aleatorio contra el modelo, pantalla completa */
    int draws = 0, skips_cat = 0, wraps = 0;
    for (int it = 0; it < 3000 && !fails; it++) {
        memset(l2_block_types, 0, sizeof l2_block_types);
        for (int k = 0; k < 0x28; k++) l2_block_types[k] = (uint8_t)(RND() & 0xf8);
        memcpy(mt, l2_block_types, 0x28);
        for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 3 + it);
        memcpy(screen, cga_mem, CGA_MEM_SIZE);
        l2_dat_350d = m350d = (uint16_t)(RND() % 3 == 0 ? 0x26 + RND() % 2 : RND() % 0x28);
        l2_dat_350f = m350f = (uint16_t)RND();
        tick_now = (uint16_t)(m350f + RND() % 12);
        for (int c = 0; c < 40; c++) {
            tick_now = (uint16_t)(tick_now + RND() % 5);
            cat_y = (uint8_t)(RND() % 2 ? RND() % 9 : RND());
            unsigned near_blk = RND() % 0x28;
            cat_x = (int16_t)(RND() % 3 == 0 ? (int)(near_blk * 2 + RND() % 9) * 4 - 4 : (int)(RND() % 0x140));
            if (cat_x < 0) cat_x = 0;
            uint16_t before = m350d, bf = m350f;
            m_animate(tick_now, cat_x, cat_y);
            animate_level2_blocks();
            if (m350d != before) draws++;
            if (m350d < before) wraps++;
            (void)bf; (void)skips_cat;
            if (l2_dat_350d != m350d || l2_dat_350f != m350f || memcmp(l2_block_types, mt, 0x28) || memcmp(cga_mem, screen, CGA_MEM_SIZE)) {
                printf("FAIL: animate it %d call %d: 350d %u/%u 350f %04x/%04x tipos %s pantalla %s\n", it, c, l2_dat_350d, m350d, l2_dat_350f, m350f,
                       memcmp(l2_block_types, mt, 0x28) ? "MAL" : "ok", memcmp(cga_mem, screen, CGA_MEM_SIZE) ? "MAL" : "ok");
                fails++; break;
            }
        }
    }
    CHECK(draws > 50000 && wraps > 500, "animate cubre pasos y vueltas (%d, %d)", draws, wraps);

    /* 1b. a mano: dos ticks consecutivos de la ola dibujan bloques distintos y cambian la franja (pixel counts) */
    memset(l2_block_types, 0, sizeof l2_block_types); memset(cga_mem, 0, CGA_MEM_SIZE);
    l2_dat_350d = 0; l2_dat_350f = 0; cat_y = 100; cat_x = 10; tick_now = 8;
    animate_level2_blocks();
    CHECK(l2_dat_350d == 1 && l2_block_types[1] == 8, "a mano: bloque 1 -> tipo 8");
    CHECK(memcmp(&cga_mem[0xa0 + 2], &ds_pool[0x2020 + 8], 2) == 0, "a mano: bloque 1 dibujado en 0xa2 con el frame 8");
    tick_now = 8; animate_level2_blocks();                       /* mismo tick: ya paso el umbral (ax = 8 - 0 >= 8) y sigue avanzando */
    CHECK(l2_dat_350d == 2, "a mano: sin refresco de dat_350f la ola sigue (dat_350d = %u)", l2_dat_350d);

    /* 2. update_entrance_anim: aleatorio contra el modelo */
    int e_draw = 0, e_hit = 0;
    for (int it = 0; it < 3000 && !fails; it++) {
        for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + it);
        memcpy(screen, cga_mem, CGA_MEM_SIZE);
        l2_dat_35d8 = m35d8 = (uint16_t)(RND() % 16 * 2); l2_dat_35da = m35da = (uint16_t)RND();
        level_complete = mlc = (RND() & 1) ? 0x0100 : 0;
        tick_now = (uint16_t)(m35da + RND() % 8);
        for (int c = 0; c < 30; c++) {
            tick_now = (uint16_t)(tick_now + RND() % 4);
            bool near = RND() % 2;
            cat_x = (int16_t)(near ? 0xc0 + RND() % 0x50 : RND() % 0x140);
            cat_y = (uint8_t)(near ? 0x70 + RND() % 0x30 : RND());
            uint16_t b35 = m35da; uint16_t blc = mlc;
            m_entrance(tick_now, cat_x, cat_y);
            update_entrance_anim();
            if (m35da != b35) e_draw++;
            if (mlc != blc) e_hit++;
            if (l2_dat_35d8 != m35d8 || l2_dat_35da != m35da || level_complete != mlc || memcmp(cga_mem, screen, CGA_MEM_SIZE)) {
                printf("FAIL: entrance it %d call %d: 35d8 %u/%u 35da %04x/%04x level_complete %04x/%04x pantalla %s\n", it, c, l2_dat_35d8, m35d8, l2_dat_35da, m35da,
                       level_complete, mlc, memcmp(cga_mem, screen, CGA_MEM_SIZE) ? "MAL" : "ok");
                fails++; break;
            }
        }
    }
    CHECK(e_draw > 20000 && e_hit > 200, "entrance cubre dibujos y choques (%d, %d)", e_draw, e_hit);

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_level2d: OK (animate: %d pasos, %d vueltas; entrance: %d dibujos, %d choques)\n", draws, wraps, e_draw, e_hit);
    return 0;
}
