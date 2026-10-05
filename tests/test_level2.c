/* T33 — init_level2_objects / reset_caught_objects / erase_level_object (level_objects.asm L806-871, L1001-1016). Sin SDL.
 * Valores esperados: modelo ESTRUCTURADO escrito desde el ASM con su propio LFSR (cga.asm `random`: xor dl,dh; shr dl,1; shr dl,1;
 * rcr word [rng_seed],1) y una pantalla modelo; nada sale del C bajo prueba. Ver PROGRESS.md §6ad. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level2.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* datos de solo lectura, copiados a mano del volcado del DS (no leidos de ds_pool) */
static const uint8_t INIT_Y[24] = {8,8,40,40,72,72,104,104,136,136,168,168, 24,24,56,56,88,88,120,120,152,152,168,168};
static const uint8_t PER_DIFF[10] = {10, 8, 6, 4, 3, 2, 2, 1, 0, 0};

/* ---- modelo ---- */
static uint16_t mseed;
static uint16_t lfsr_step(uint16_t x) { unsigned c = (unsigned)((((x & 0xff) ^ (x >> 8)) >> 1) & 1); return (uint16_t)((x >> 1) | (c << 15)); }
/* El LFSR de `random` tiene el estado 0 como punto fijo (devuelve siempre 0): una semilla que cae ahi cuelga el reintento del
 * original (slot 12 ya activo para siempre). El test solo usa semillas cuya trayectoria no pasa por 0. */
static uint8_t seed_ok[65536];
static void build_seed_table(void) {
    static uint8_t state[65536];                                     /* 0 = sin ver, 1 = en curso, 2 = bueno, 3 = malo */
    state[0] = 3;
    for (uint32_t s0 = 1; s0 < 65536; s0++) {
        if (state[s0]) continue;
        uint32_t s = s0;
        while (!state[s]) { state[s] = 1; s = lfsr_step((uint16_t)s); }
        uint8_t res = state[s] == 3 ? 3 : 2;                         /* si cierra un ciclo nuevo en curso, es bueno */
        s = s0;
        while (state[s] == 1) { state[s] = res; s = lfsr_step((uint16_t)s); }
    }
    for (uint32_t i = 0; i < 65536; i++) seed_ok[i] = state[i] == 2;
}
static uint8_t m_random(void) {                                   /* devuelve dl; dx = nueva semilla */
    uint8_t lo = (uint8_t)(mseed & 0xff), hi = (uint8_t)(mseed >> 8);
    unsigned c = (unsigned)(((lo ^ hi) >> 1) & 1);                 /* carry de los dos shr dl,1 */
    mseed = (uint16_t)((mseed >> 1) | (c << 15));
    return (uint8_t)(mseed & 0xff);
}
static struct {
    uint8_t d3410; uint16_t toggle, d3415, x[24], cur; uint8_t d3417[24], d342f[24], y[24], hit[24], act[24]; uint16_t cga[24]; uint8_t d351b;
} m;

static void m_init(unsigned diff) {
    m.toggle = 0; m.d3415 = 0; m.d3410 = 0xc;
    for (int s = 23; s >= 0; s--) {
        m.hit[s] = 1; m.act[s] = 0; m.y[s] = INIT_Y[s]; m.d342f[s] = 1;
        m.d3417[s] = (m_random() & 1) ? 1 : 0xff;
        m.x[s] = m_random();
    }
    unsigned n = PER_DIFF[diff];
    unsigned left = n ? n : 0x10000;                              /* cx = 0 -> 65536 vueltas */
    while (left--) {
        int s;
        for (;;) {
            uint8_t r = m_random() & 0xf;
            if (r >= 12) continue;
            s = 12 + r;
            if (!m.act[s]) break;
            int all = 1; for (int i = 12; i < 24; i++) if (!m.act[i]) all = 0;
            if (all) return;                                      /* la desviacion documentada (el original se cuelga) */
        }
        m.act[s] = 1;
    }
}
static void m_reset(int16_t catx) {
    for (;;) {
        int s = -1;
        for (int c = 12; c >= 1; c--) if (m.act[c + 11]) { s = c + 11; break; }
        if (s < 0) return;
        m.act[s] = 0;
        if ((uint16_t)catx > 0xa0) { m.d3417[s] = 1; m.x[s] = 0; }
        else                       { m.d3417[s] = 0xff; m.x[s] = 0x12e; }
        if (--m.d351b == 0) return;
    }
}

static bool same_state(void) {
    return l2_dat_3410 == m.d3410 && l2_anim_toggle == m.toggle && l2_dat_3415 == m.d3415 && l2_obj_cur_addr == m.cur && l2_dat_351b == m.d351b &&
           !memcmp(l2_dat_3417, m.d3417, 24) && !memcmp(l2_dat_342f, m.d342f, 24) && !memcmp(l2_obj_x, m.x, sizeof m.x) &&
           !memcmp(l2_obj_y, m.y, 24) && !memcmp(l2_obj_hit, m.hit, 24) && !memcmp(l2_obj_active, m.act, 24) &&
           !memcmp(l2_obj_cga_addr, m.cga, sizeof m.cga) && rng_seed == mseed;
}

/* pantalla modelo (bancos CGA entrelazados; la fila inicial puede ser impar) */
static uint8_t model[CGA_MEM_SIZE];
static size_t rowoff(uint16_t S, int r) {
    int b0 = (S >> 13) & 1;
    return (size_t)(S & 0x1fff) + (size_t)(((r + b0) >> 1) * 80) + (size_t)(((b0 ^ (r & 1)) & 1) * 0x2000);
}
static void fill(void) { for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + 3); memcpy(model, cga_mem, CGA_MEM_SIZE); }

static uint32_t lcg = 0x1234567;
static uint32_t rnd(void) { lcg = lcg * 1664525u + 1013904223u; return lcg >> 8; }

static void sync_in(void) {                                       /* real -> modelo (estado inicial arbitrario) */
    m.d3410 = l2_dat_3410; m.toggle = l2_anim_toggle; m.d3415 = l2_dat_3415; m.cur = l2_obj_cur_addr; m.d351b = l2_dat_351b;
    memcpy(m.d3417, l2_dat_3417, 24); memcpy(m.d342f, l2_dat_342f, 24); memcpy(m.x, l2_obj_x, sizeof m.x); memcpy(m.y, l2_obj_y, 24);
    memcpy(m.hit, l2_obj_hit, 24); memcpy(m.act, l2_obj_active, 24); memcpy(m.cga, l2_obj_cga_addr, sizeof m.cga);
}

int main(void) {
    build_seed_table();
    CHECK(!seed_ok[0] && !seed_ok[1] && seed_ok[0xfa59] && seed_ok[0xffff] && seed_ok[0x8000], "0: tabla de semillas");
    /* 0) el DS arranca a cero (verificado contra el volcado) */
    { uint16_t z = 0; for (int i = 0; i < 24; i++) z |= (uint16_t)(l2_obj_x[i] | l2_obj_cga_addr[i] | l2_obj_hit[i] | l2_obj_active[i]);
      CHECK(z == 0 && l2_anim_toggle == 0 && l2_dat_351b == 0, "0: estado inicial"); }
    for (int i = 0; i < 24; i++) CHECK(ds_pool[L2_OBJ_INIT_Y + i] == INIT_Y[i], "0: l2_obj_init_y[%d]", i);
    for (int i = 0; i < 10; i++) CHECK(ds_pool[L2_DAT_351C + i] == PER_DIFF[i], "0: dat_351c[%d]", i);

    /* 1) init_level2_objects: todas las dificultades (0..9) x muchas semillas; estado, RNG y propiedades */
    for (unsigned diff = 0; diff < 10; diff++)
        for (int it = 0; it < 300; it++) {
            uint16_t seed = it == 0 ? 0xfa59 : it == 1 ? 0xffff : it == 2 ? 0x8000 : (uint16_t)rnd();
            while (!seed_ok[seed]) seed = (uint16_t)rnd();
            rng_seed = mseed = seed; difficulty_level = (uint16_t)diff;
            /* ensucia el estado para comprobar que init lo reescribe todo */
            for (int i = 0; i < 24; i++) { l2_obj_hit[i] = 0; l2_obj_active[i] = (uint8_t)rnd(); l2_obj_x[i] = (uint16_t)rnd(); l2_dat_3417[i] = 7; l2_dat_342f[i] = 9; l2_obj_y[i] = 0; }
            l2_dat_3410 = 0; l2_anim_toggle = 0x55; l2_dat_3415 = 0x66; l2_dat_351b = 0x77; l2_obj_cur_addr = 0x1234;
            memcpy(m.cga, l2_obj_cga_addr, sizeof m.cga); m.cur = l2_obj_cur_addr; m.d351b = l2_dat_351b;
            /* el modelo parte del mismo sucio para lo que init NO escribe (cur, cga, 351b); lo que escribe se rehace */
            for (int i = 0; i < 24; i++) { m.d3417[i] = 7; }
            init_level2_objects(); m_init(diff);
            CHECK(same_state(), "1: diff=%u seed=%04x != modelo", diff, seed);
            int n = 0; for (int i = 0; i < 24; i++) { n += l2_obj_active[i]; CHECK(l2_obj_hit[i] == 1 && l2_obj_y[i] == INIT_Y[i] && l2_dat_342f[i] == 1 && (l2_dat_3417[i] == 1 || l2_dat_3417[i] == 0xff) && l2_obj_x[i] <= 255, "1: slot %d", i); }
            unsigned want = PER_DIFF[diff] ? PER_DIFF[diff] : 12;
            CHECK((unsigned)n == want, "1: diff=%u activos=%d esperados=%u", diff, n, want);
            for (int i = 0; i < 12; i++) CHECK(!l2_obj_active[i], "1: slot %d bajo activo", i);
            if (fails > 10) goto done;
        }

    /* 2) reset_caught_objects: subconjuntos activos aleatorios, dat_351b, cat_x en los bordes del `ja` */
    for (int it = 0; it < 3000; it++) {
        for (int i = 0; i < 24; i++) { l2_obj_active[i] = (rnd() % 3) ? (i >= 12 && (rnd() & 1)) : (rnd() & 1); l2_obj_x[i] = (uint16_t)rnd(); l2_dat_3417[i] = (uint8_t)rnd(); }
        static const int16_t xs[] = {0, 0x9f, 0xa0, 0xa1, 0x12e, 0x7fff, -1, -0x60};
        cat_x = (rnd() % 3) ? xs[rnd() % 8] : (int16_t)rnd();
        l2_dat_351b = (rnd() % 3) ? (uint8_t)(rnd() % 4) : (uint8_t)rnd();
        sync_in(); mseed = rng_seed;
        reset_caught_objects(); m_reset(cat_x);
        CHECK(same_state(), "2.%d: != modelo", it);
        if (fails > 10) goto done;
    }
    /* 2b) caso a mano: dos activos (13 y 20), cat_x = 0xa0 (no `ja`) -> x = 0x12e / dir 0xff; contador 2 -> los atiende a ambos */
    memset(l2_obj_active, 0, 24); l2_obj_active[13] = l2_obj_active[20] = 1; l2_dat_351b = 2; cat_x = 0xa0;
    reset_caught_objects();
    CHECK(!l2_obj_active[13] && !l2_obj_active[20] && l2_obj_x[13] == 0x12e && l2_obj_x[20] == 0x12e && l2_dat_3417[20] == 0xff && l2_dat_351b == 0, "2b");
    memset(l2_obj_active, 0, 24); l2_obj_active[15] = 1; l2_dat_351b = 5; cat_x = 0xa1;
    reset_caught_objects();
    CHECK(!l2_obj_active[15] && l2_obj_x[15] == 0 && l2_dat_3417[15] == 1 && l2_dat_351b == 4, "2c: cat_x=0xa1 -> x=0, dir=1");

    /* 3) erase_level_object: hit != 0 no hace nada; hit == 0 borra 1x6 (slots 0..11) o 2x2 (12..23) con 0x55 */
    for (int it = 0; it < 2000; it++) {
        fill();
        for (int i = 0; i < 24; i++) { l2_obj_hit[i] = (rnd() % 3) ? 0 : 1; uint16_t base = (rnd() & 1) ? 0x2000 : 0; l2_obj_cga_addr[i] = (uint16_t)(base + rnd() % 0x1c00); }
        int slot = (int)(rnd() % 24);
        if (!l2_obj_hit[slot]) {
            int w = slot < 12 ? 1 : 2, h = slot < 12 ? 6 : 2;
            for (int r = 0; r < h; r++) memset(&model[rowoff(l2_obj_cga_addr[slot], r)], 0x55, (size_t)w * 2);
        }
        erase_level_object((uint16_t)slot);
        CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) == 0, "3.%d: slot %d hit=%d != modelo", it, slot, l2_obj_hit[slot]);
        if (fails > 10) goto done;
    }
done:
    if (fails) { printf("test_level2: %d FALLOS\n", fails); return 1; }
    printf("test_level2: OK\n");
    return 0;
}
