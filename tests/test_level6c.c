/* T31 — update_level6_timing (level_objects.asm L2666-2740). Sin SDL.
 * Los valores esperados NO salen del C: un modelo del bucle escrito desde el ASM (cx = 12..1, slot = cx-1), el LFSR de
 * cga.asm (L158-165), la colision de check_rect_collision (level_objects.asm L14-39) y una pantalla modelo. Ver PROGRESS.md §6ab. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "cga.h"
#include "cat_state.h"
#include "alley.h"
#include "level6.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint16_t W(uint16_t off) { return (uint16_t)(ds_pool[off] | (ds_pool[off + 1] << 8)); }

static uint16_t m_seed;
static uint16_t m_random(void) {            /* dl = lo^hi; shr dl,1 x2 (CF = bit 1); rcr seed,1 */
    uint8_t dl = (uint8_t)((m_seed & 0xff) ^ (m_seed >> 8));
    unsigned cf = (dl >> 1) & 1;
    m_seed = (uint16_t)((cf << 15) | (m_seed >> 1));
    return m_seed;
}

static uint8_t model[CGA_MEM_SIZE];
static void m_blit(const uint8_t *src, uint16_t at, int w, int h) {
    for (int r = 0; r < h; r++) memcpy(&model[at + (r & 1) * 0x2000 + (r >> 1) * 80], src + r * w * 2, (size_t)w * 2);
}
static void m_or(const uint8_t *src, uint16_t at, int w, int h) {
    for (int r = 0; r < h; r++)
        for (int b = 0; b < w * 2; b++) model[at + (r & 1) * 0x2000 + (r >> 1) * 80 + b] |= src[r * w * 2 + b];
}
static void fill(void) { for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + 3); memcpy(model, cga_mem, CGA_MEM_SIZE); }
static bool same(void) { return memcmp(cga_mem, model, CGA_MEM_SIZE) == 0; }
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }

/* tablas de datos (solo lectura en el DS) */
static const uint16_t THR[8] = {18, 16, 15, 14, 13, 12, 11, 10};          /* dat_44dc */
static const uint16_t RNG[8] = {40, 50, 60, 70, 85, 80, 85, 90};          /* dat_44ec */
static const uint16_t OX[12] = {0x2c,0x7c,0xc4,0x20,0x5c,0x9c,0xcc,0x10c,0x34,0x84,0xbc,0x124};   /* dat_43e1 */
static const uint8_t  OY[12] = {0x88,0x88,0x88,0x98,0x98,0x98,0x98,0x98,0xa8,0xa8,0xa8,0xa8};      /* dat_43f9 */

static void set_alley_buf(void) {
    for (int i = 0; i < 128; i++) alley_save_buf[i] = (uint16_t)(0x1234 + 0x0101 * i);
    cat_draw_pos = 0x1200; buffer_size = 0x0102; cat_sprite_ptr = NULL;     /* restore = 2x1 en 0x1200; foreground no dibuja */
}

/* ---- modelo del bucle ---- */
static uint16_t m_state[12], m_flag[12]; static uint8_t m_44fc, m_44d9; static uint16_t m_44d7;

static void m_alert_and_refresh(int slot) {      /* lab_4870 */
    uint16_t x0 = (uint16_t)(OX[slot] - 0x14);
    bool hit = (uint16_t)cat_x <= (uint16_t)(x0 + 0x28) && (uint16_t)cat_x >= (x0 >= 0x18 ? x0 - 0x18 : 0)
            && cat_y <= (uint8_t)(OY[slot] + 6) && cat_y >= (OY[slot] >= 0x0e ? OY[slot] - 0x0e : 0);
    m_44d9 = hit;
    if (hit) m_blit((const uint8_t *)alley_save_buf, 0x1200, 2, 1);          /* dat_44bd = 0 -> restore_alley_buffer */
    uint16_t sp = W((uint16_t)(0x4429 + 2 * slot));
    uint16_t src = (uint16_t)(0x4100 + 2 * m_state[slot]), dst = (uint16_t)(W((uint16_t)(0x4411 + 2 * slot)) + 0xa7);
    if (sp != 0x429c) { dst = (uint16_t)(dst - 6); src = (uint16_t)(src + 6); }
    m_blit(&ds_pool[src], dst, 1, 1);
    /* refresh: dat_44d9 && dat_44bd == 0 -> draw_alley_foreground con cat_sprite_ptr NULL: nada */
}

/* devuelve 1 si el modelo llego al final del bucle (sin persecucion) */
static int m_timing(uint16_t tick) {
    if ((uint16_t)(tick - m_44d7) <= THR[difficulty_level]) return 0;
    m_44d7 = tick;
    if (enemy_active) return 0;
    m_44fc = 0;
    for (int slot = 11; slot >= 0; slot--) {
        if (!m_flag[slot]) continue;
        int near = 0;
        if (OY[slot] == cat_y) {
            uint16_t X = OX[slot], c = (uint16_t)cat_x;
            uint16_t d = X >= c ? (uint16_t)(X - c) : (uint16_t)(c - X - 1);   /* sub + not */
            near = d <= RNG[difficulty_level];
        }
        if (near) {
            if (m_state[slot] >= 2) return -1;                                  /* persecucion: lo prueba otro caso */
            m_state[slot]++;
            if (m_state[slot] >= 2) m_44fc++;
        } else {
            if (m_state[slot] == 0) continue;
            uint8_t dl = (uint8_t)(m_random() & 0xff);
            if (dl <= 0x38) m_state[slot]--;
        }
        m_alert_and_refresh(slot);
    }
    return 1;
}

static void load(const uint16_t *st, const uint16_t *fl) {
    for (int i = 0; i < 12; i++) { l6_obj_state[i] = m_state[i] = st[i]; l6_obj_flag[i] = m_flag[i] = fl[i]; }
}
static void both_vars(uint16_t d7, uint8_t d44fc, uint8_t d44d9) {
    l6_dat_44d7 = m_44d7 = d7; l6_dat_44fc = m_44fc = d44fc; l6_dat_44d9 = m_44d9 = d44d9; l6_dat_44bd = 0;
}
static bool states_same(void) {
    for (int i = 0; i < 12; i++) if (l6_obj_state[i] != m_state[i]) return false;
    return true;
}
static uint32_t lcg = 0x2545f491;
static uint32_t rnd(void) { lcg = lcg * 1664525u + 1013904223u; return lcg >> 8; }

int main(void) {
    static const uint16_t zero[12] = {0};
    static const uint16_t ones[12] = {1,1,1,1,1,1,1,1,1,1,1,1};
    enemy_active = 0; level_number = 6; enemy_chasing = 0;

    /* 1) umbral de ticks: delta <= dat_44dc[dif] no hace nada; delta > umbral dispara (incluye wrap de 16 bits) */
    static const struct { int dif; uint16_t last, tick; int fires; } g[] = {
        {0, 1000, 1018, 0}, {0, 1000, 1019, 1}, {7, 500, 510, 0}, {7, 500, 511, 1}, {3, 0xfff0, 0xfff0 + 14, 0},
        {0, 0xfff0, 2, 0}, {0, 0xfff0, 3, 1}, {0, 0xfff0, 5, 1}, {0, 700, 600, 1} /* tick < last: delta enorme */ };
    for (unsigned i = 0; i < sizeof g / sizeof g[0]; i++) {
        fill(); load(zero, zero); set_alley_buf(); difficulty_level = (uint16_t)g[i].dif; enemy_active = 0;
        l6_dat_44d7 = g[i].last; l6_dat_44fc = 9; rng_seed = 0xace1;
        l6_tick_override = g[i].tick;
        update_level6_timing();
        CHECK(l6_dat_44d7 == (g[i].fires ? g[i].tick : g[i].last), "1.%u: dat_44d7=%u", i, l6_dat_44d7);
        CHECK(l6_dat_44fc == (g[i].fires ? 0 : 9), "1.%u: dat_44fc=%u (dispara=%d)", i, l6_dat_44fc, g[i].fires);
        CHECK(same() && rng_seed == 0xace1, "1.%u: no debe tocar pantalla ni rng", i);
    }

    /* 2) enemy_active != 0: actualiza el tick pero NO limpia dat_44fc ni toca los slots */
    {
        static const uint16_t st[12] = {1,1,1,1,1,1,1,1,1,1,1,1};
        fill(); load(st, ones); set_alley_buf(); difficulty_level = 2; enemy_active = 1;
        l6_dat_44d7 = 100; l6_dat_44fc = 9; rng_seed = 0xace1; l6_tick_override = 200;
        update_level6_timing();
        CHECK(l6_dat_44d7 == 200 && l6_dat_44fc == 9, "2: d7=%u fc=%u", l6_dat_44d7, l6_dat_44fc);
        int ok = 1; for (int i = 0; i < 12; i++) if (l6_obj_state[i] != 1) ok = 0;
        CHECK(ok && same() && rng_seed == 0xace1, "2: slots/pantalla/rng intactos");
        enemy_active = 0;
    }

    /* 3) transiciones explicitas con cronometro: estado 0->1 (sin explosion), 1->2 (dat_44fc=1 y explosion bloqueante) */
    {
        static const uint16_t fl[12] = {0,0,0,0,0,0,0,0,0,1,0,0};
        double t0, dt;
        for (int s0 = 0; s0 <= 1; s0++) {
            static uint16_t st[12]; memset(st, 0, sizeof st); st[9] = (uint16_t)s0;
            fill(); load(st, fl); set_alley_buf(); difficulty_level = 0; enemy_active = 0;
            cat_y = 0xa8; cat_x = 0x84;                    /* slot 9: Y=0xa8, X=0x84 -> d=0 */
            both_vars(0, 7, 0x55); rng_seed = 0xace1; m_seed = rng_seed; l6_tick_override = 100;
            t0 = now_ms(); update_level6_timing(); dt = now_ms() - t0;
            int r = m_timing(100);
            CHECK(r == 1, "3.%d: el modelo no llego al final", s0);
            CHECK(l6_obj_state[9] == (uint16_t)(s0 + 1), "3.%d: state=%u", s0, l6_obj_state[9]);
            CHECK(l6_dat_44fc == (s0 == 1 ? 1 : 0) && l6_dat_44fc == m_44fc, "3.%d: dat_44fc=%u", s0, l6_dat_44fc);
            CHECK(l6_dat_44d9 == m_44d9 && same() && rng_seed == m_seed, "3.%d: pantalla/dat_44d9/rng != modelo", s0);
            CHECK(s0 == 1 ? dt >= 40.0 : dt < 20.0, "3.%d: explosion %s (%.1f ms)", s0, s0 ? "no sono" : "sono sin motivo", dt);
        }
    }

    /* 4) persecucion: slot 4 (Y=0x98, X=0x5c) con state 2 y el gato encima; antes (slot 11) pasa por lab_4870;
     *    los slots 3..0 no se procesan; sin explosion */
    {
        static const uint16_t fl[12] = {1,1,1,1,1,0,0,0,0,0,0,1};
        static const uint16_t st[12] = {1,1,1,1,2,0,0,0,0,0,0,1};
        fill(); load(st, fl); set_alley_buf(); difficulty_level = 0; enemy_active = 0; enemy_chasing = 0;
        level_number = 6; cat_y = 0x98; cat_x = 0x5c;
        both_vars(0, 7, 0x55); l6_dat_44da = 0; rng_seed = 0xbeef; m_seed = rng_seed; l6_tick_override = 100;
        uint16_t thrown = W(0x3260);
        CHECK(thrown != 0, "4: fixture: primer frame del objeto lanzado es 0");
        double t0 = now_ms(); update_level6_timing(); double dt = now_ms() - t0;
        /* modelo: slot 11 (no cerca: otra fila, state 1) -> random y lab_4870; luego slot 4 */
        m_44d7 = 100; m_44fc = 0;
        { uint8_t dl = (uint8_t)(m_random() & 0xff); if (dl <= 0x38) m_state[11]--; }
        m_alert_and_refresh(11);
        /* slot 4 con state>=2: dat_44da = l6_obj_x[4]; prepare (restore 2x1), clear 0xAA 5x13, OR del frame lanzado en 0 */
        uint16_t x4 = W(0x4411 + 8);
        m_blit((const uint8_t *)alley_save_buf, 0x1200, 2, 1);
        { uint8_t blk[0x82]; memset(blk, 0xaa, sizeof blk); m_blit(blk, x4, 5, 13); }
        m_or(&ds_pool[thrown], 0, 2, 30);
        CHECK(l6_dat_44da == x4, "4: dat_44da=%04x esperado %04x", l6_dat_44da, x4);
        CHECK(same(), "4: pantalla != modelo (restore, clear 5x13, OR del lanzado)");
        CHECK(states_same() && l6_obj_state[3] == 1 && l6_obj_state[4] == 2, "4: estados (slots 3..0 no se procesan)");
        CHECK(rng_seed == m_seed, "4: rng_seed %04x != %04x", rng_seed, m_seed);
        CHECK(l6_dat_44fc == 0 && dt < 20.0, "4: sin explosion (dat_44fc=%u, %.1f ms)", l6_dat_44fc, dt);
        CHECK(enemy_active != 0 && enemy_chasing == 1, "4: activate_enemy_chase no se ejecuto (active=%u chasing=%u)", enemy_active, enemy_chasing);
        CHECK(cat_x == 0 || cat_x == 0x122, "4: cat_x=%d", cat_x);
        CHECK(l6_dat_44d7 == 100, "4: dat_44d7");
        enemy_active = 0; enemy_chasing = 0;
    }

    /* 5) aleatorio contra el modelo (estados 0..1 en la fila del gato para no disparar la persecucion) */
    {
        static const uint8_t rows[4] = {0x88, 0x98, 0xa8, 0x70};
        int expl = 0, nearN = 0, rndN = 0, fires = 0;
        for (int it = 0; it < 150; it++) {
            uint16_t st[12], fl[12];
            difficulty_level = rnd() & 7; enemy_active = 0;
            cat_y = rows[rnd() & 3];
            cat_x = (int16_t)(rnd() % 0x140);
            if (rnd() % 4 == 0) cat_x = (int16_t)(OX[rnd() % 12] + (int)(rnd() % 3) - 1 + ((rnd() & 1) ? (int)RNG[difficulty_level] : 0));
            for (int i = 0; i < 12; i++) {
                fl[i] = rnd() % 4 != 0;
                st[i] = (OY[i] == cat_y) ? (uint16_t)(rnd() % 2) : (uint16_t)(rnd() % 3);
            }
            fill(); load(st, fl); set_alley_buf();
            both_vars((uint16_t)rnd(), (uint8_t)rnd(), (uint8_t)rnd());
            uint16_t tick = (uint16_t)(l6_dat_44d7 + THR[difficulty_level] + 1 + rnd() % 5);
            do { rng_seed = (uint16_t)rnd(); } while (rng_seed < 2);
            m_seed = rng_seed; l6_tick_override = tick;
            double t0 = now_ms(); update_level6_timing(); double dt = now_ms() - t0;
            int r = m_timing(tick);
            CHECK(r == 1, "5.%d: el modelo no termino el bucle (r=%d)", it, r);
            CHECK(states_same(), "5.%d: l6_obj_state != modelo (dif %u, cat %d,%02x)", it, difficulty_level, cat_x, cat_y);
            CHECK(l6_dat_44fc == m_44fc, "5.%d: dat_44fc=%u modelo %u", it, l6_dat_44fc, m_44fc);
            CHECK(l6_dat_44d9 == m_44d9, "5.%d: dat_44d9=%u modelo %u", it, l6_dat_44d9, m_44d9);
            CHECK(l6_dat_44d7 == m_44d7, "5.%d: dat_44d7", it);
            CHECK(rng_seed == m_seed, "5.%d: rng_seed %04x != %04x (numero de random())", it, rng_seed, m_seed);
            CHECK(same(), "5.%d: pantalla != modelo", it);
            CHECK(m_44fc ? dt >= 40.0 : dt < 20.0, "5.%d: explosion %s (%.1f ms)", it, m_44fc ? "no sono" : "sono sin motivo", dt);
            expl += m_44fc != 0; fires++;
            for (int i = 0; i < 12; i++) if (m_flag[i] && OY[i] == cat_y) nearN++;
            if (rng_seed != 0) rndN++;
        }
        CHECK(expl > 5 && expl < 100, "5: cobertura de explosion %d/150", expl);
        CHECK(nearN > 100, "5: cobertura de slots en la fila del gato %d", nearN);
        (void)rndN; (void)fires;
    }

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_level6c: OK\n");
    return 0;
}
