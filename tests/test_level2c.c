/* T35 — update_level2_objects (level_objects.asm L872-998). Sin SDL. Modelo independiente escrito desde el ASM (LFSR propio,
 * pantalla modelo con intercalado de bancos CGA, tablas de solo lectura copiadas a mano). Ver PROGRESS.md §6af. */
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

static const uint8_t INIT_Y[24] = {8,8,40,40,72,72,104,104,136,136,168,168, 24,24,56,56,88,88,120,120,152,152,168,168};
static uint16_t tick_now; static unsigned tick_reads;
static uint16_t fake_tick(void) { tick_reads++; return tick_now; }

/* ---- modelo ---- */
static uint16_t mseed;
static uint8_t m_random(void) {
    uint8_t lo = (uint8_t)(mseed & 0xff), hi = (uint8_t)(mseed >> 8);
    unsigned c = (unsigned)(((lo ^ hi) >> 1) & 1);
    mseed = (uint16_t)((mseed >> 1) | (c << 15));
    return (uint8_t)(mseed & 0xff);
}
static struct {
    uint16_t toggle, d3415, d3413, d3509, d350b, cur, x[24], cga[24];
    uint8_t d3417[24], d342f[24], y[24], hit[24], act[24];
    uint8_t screen[CGA_MEM_SIZE];
} M;
/* sprites y patron copiados a mano del volcado del DS (dos frames de cada uno bastan para distinguir; el resto se toma del ds_pool
 * por desplazamiento ya verificado en T33 para el patron) */
static size_t addr(unsigned y, unsigned x) { return (size_t)(y >> 1) * 80 + (y & 1) * 0x2000 + (x >> 2); }
static void m_blit(const uint8_t *src, uint16_t start, unsigned wwords, unsigned rows) {
    /* start = direccion de la fila inicial; filas sucesivas: alternar banco y avanzar 80 cada dos */
    size_t a = start;
    for (unsigned r = 0; r < rows; r++) {
        memcpy(&M.screen[a], src + r * wwords * 2, wwords * 2);
        a ^= 0x2000; if (!(a & 0x2000)) a += 80;
    }
}
static void m_erase(unsigned slot) {
    if (M.hit[slot]) return;
    static const uint8_t pat[12] = {0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55};
    if (slot < 12) m_blit(pat, M.cga[slot], 1, 6); else m_blit(pat, M.cga[slot], 2, 2);
}
static void m_update(uint16_t tick, uint8_t cat_y_, uint8_t rom) {
    if (tick == M.d3509) return;
    M.d350b = tick;
    M.d3415++;
    unsigned bx = M.d3415;
    if (bx >= 0x18) { bx = 0; M.d3415 = 0; M.toggle ^= 0xc; M.d3413 += 8; M.d3509 = M.d350b; }
    else if (bx == 0xc) { if (rom != 0xfd || cat_y_ >= 0x30) M.d3509 = M.d350b; }
    else { /* otros slots: no tocan dat_3509 */ }
    if (M.act[bx]) return;
    uint8_t dl = m_random();
    if (dl <= 0x10) {
        M.d3417[bx] = (dl & 1) ? 1 : 0xff;
        dl = m_random();
        M.d342f[bx] = (dl & 1) ? 1 : 0xff;
    }
    unsigned step = bx < 12 ? 4 : 2;
    unsigned x = M.x[bx];
    if (M.d3417[bx] == 1) { x += step; if (x >= 0x12f) { x = 0x12e; M.d3417[bx] = 0xff; } }
    else { if (x < step) { x = 0; M.d3417[bx] = 1; } else x -= step; }
    M.x[bx] = (uint16_t)x;
    unsigned y = M.y[bx];
    if (M.d342f[bx] == 1) { y = (y + 1) & 0xff; if (y > (unsigned)(INIT_Y[bx] + 0x18)) { y = INIT_Y[bx] + 0x18; M.d342f[bx] = 0xff; } }
    else { y = (y - 1) & 0xff; if (y < INIT_Y[bx]) { y = INIT_Y[bx]; M.d342f[bx] = 1; } }
    M.y[bx] = (uint8_t)y;
    M.cur = (uint16_t)addr(y, M.x[bx]);
    m_erase(bx);
    M.cga[bx] = M.cur; M.hit[bx] = 0;
    if (bx >= 12) {
        unsigned off = ((bx * 8 + M.d3413) & 0x18);
        m_blit(&ds_pool[0x3330 + off], M.cur, 2, 2);
    } else {
        unsigned off = M.toggle;
        if (!(bx & 1)) off ^= 0xc;
        if (M.d3417[bx] != 1) off += 0x18;
        m_blit(&ds_pool[0x3300 + off], M.cur, 1, 6);
    }
}

static void load_model(void) {
    M.toggle = l2_anim_toggle; M.d3415 = l2_dat_3415; M.d3413 = l2_dat_3413; M.d3509 = l2_dat_3509; M.d350b = l2_dat_350b; M.cur = l2_obj_cur_addr;
    memcpy(M.x, l2_obj_x, sizeof M.x); memcpy(M.cga, l2_obj_cga_addr, sizeof M.cga);
    memcpy(M.d3417, l2_dat_3417, 24); memcpy(M.d342f, l2_dat_342f, 24); memcpy(M.y, l2_obj_y, 24);
    memcpy(M.hit, l2_obj_hit, 24); memcpy(M.act, l2_obj_active, 24);
    memcpy(M.screen, cga_mem, CGA_MEM_SIZE);
}
static int same(const char *what, int it) {
    int bad = 0;
#define EQ(a, b, n) do { if ((a) != (b)) { printf("FAIL: it %d %s: " n " got %u exp %u\n", it, what, (unsigned)(a), (unsigned)(b)); bad++; } } while (0)
    EQ(l2_anim_toggle, M.toggle, "toggle"); EQ(l2_dat_3415, M.d3415, "3415"); EQ(l2_dat_3413, M.d3413, "3413");
    EQ(l2_dat_3509, M.d3509, "3509"); EQ(l2_dat_350b, M.d350b, "350b"); EQ(l2_obj_cur_addr, M.cur, "cur");
    if (memcmp(l2_obj_x, M.x, sizeof M.x)) { printf("FAIL: it %d %s: x\n", it, what); bad++; }
    if (memcmp(l2_obj_cga_addr, M.cga, sizeof M.cga)) { printf("FAIL: it %d %s: cga addr\n", it, what); bad++; }
    if (memcmp(l2_dat_3417, M.d3417, 24)) { printf("FAIL: it %d %s: dir x\n", it, what); bad++; }
    if (memcmp(l2_dat_342f, M.d342f, 24)) { printf("FAIL: it %d %s: dir y\n", it, what); bad++; }
    if (memcmp(l2_obj_y, M.y, 24)) { printf("FAIL: it %d %s: y\n", it, what); bad++; }
    if (memcmp(l2_obj_hit, M.hit, 24)) { printf("FAIL: it %d %s: hit\n", it, what); bad++; }
    if (memcmp(cga_mem, M.screen, CGA_MEM_SIZE)) { printf("FAIL: it %d %s: pantalla\n", it, what); bad++; }
    fails += bad;
    return bad == 0;
}

int main(void) {
    l2_tick_fn = fake_tick;
    uint32_t rs = 777;
    #define RND() (rs = rs * 1664525u + 1013904223u, (rs >> 8))

    /* 1. mismo tick que dat_3509: no hace nada (ni lee random) */
    memset(cga_mem, 0x11, CGA_MEM_SIZE); init_level2_objects(); rng_seed = 0x1234;
    tick_now = 50; l2_dat_3509 = 50; uint16_t before = l2_dat_3415; uint16_t seed0 = rng_seed;
    update_level2_objects();
    CHECK(l2_dat_3415 == before && rng_seed == seed0 && l2_dat_350b == 0, "mismo tick: sin efecto");

    /* 2. aleatorio: escenarios con estado inicial arbitrario y secuencias de 60 llamadas con ticks que avanzan a veces */
    int total_calls = 0, draws = 0, wraps = 0, active_skips = 0, rev_x = 0, rev_y = 0;
    for (int it = 0; it < 3000; it++) {
        init_level2_objects();                                        /* deja arrays coherentes; despues se aleatorizan */
        for (int s = 0; s < 24; s++) {
            bool edge = RND() % 4 == 0;
            l2_obj_x[s] = edge ? (uint16_t)(RND() % 2 ? RND() % 6 : 0x129 + RND() % 6) : (uint16_t)(RND() % 0x130);
            unsigned yr = RND() % 6;
            l2_obj_y[s] = yr == 0 ? INIT_Y[s] : yr == 1 ? (uint8_t)(INIT_Y[s] + 0x18) : (uint8_t)(INIT_Y[s] + RND() % 0x19);
            l2_dat_3417[s] = (RND() & 1) ? 1 : 0xff; l2_dat_342f[s] = (RND() & 1) ? 1 : 0xff;
            l2_obj_active[s] = RND() % 6 == 0;
            l2_obj_hit[s] = RND() & 1;
            { unsigned yy = INIT_Y[s] + RND() % 25; l2_obj_cga_addr[s] = (uint16_t)addr(yy, RND() % 0x130); }
        }
        l2_dat_3415 = (uint16_t)(RND() % 3 == 0 ? 22 + RND() % 2 : RND() % 24);
        l2_anim_toggle = (RND() & 1) ? 0xc : 0; l2_dat_3413 = (uint16_t)(RND() % 64) * 2;
        l2_dat_3509 = (uint16_t)(RND() % 4); l2_dat_350b = (uint16_t)RND();
        rom_id = (RND() & 1) ? 0xfd : 0xff; cat_y = (uint8_t)(RND() % 3 == 0 ? 0x2f + RND() % 3 : RND());
        for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 5 + it);
        rng_seed = (uint16_t)(RND() | 1); mseed = rng_seed;
        load_model(); tick_now = (uint16_t)(RND() % 4);
        for (int c = 0; c < 60; c++) {
            if (RND() % 3 == 0) tick_now++;
            if (RND() % 11 == 0) cat_y = (uint8_t)(0x2e + RND() % 4);
            uint16_t px = M.d3415; bool wasact = M.act[(px + 1) % 24];
            unsigned xdir = M.d3417[(px + 1) % 24], ydir = M.d342f[(px + 1) % 24];
            m_update(tick_now, cat_y, rom_id);
            update_level2_objects();
            total_calls++;
            if (M.d3415 < px) wraps++;
            if (wasact && M.d3509 != tick_now) active_skips++;
            (void)xdir; (void)ydir;
            if (!same("seq", it)) { it = 3000; break; }
            if (M.d3415 && !M.act[M.d3415] && M.d3415 != px) { draws++; if (M.d3417[M.d3415] != xdir) rev_x++; if (M.d342f[M.d3415] != ydir) rev_y++; }
            if (rng_seed != mseed) { printf("FAIL: it %d rng_seed %04x vs %04x\n", it, rng_seed, mseed); fails++; it = 3000; break; }
        }
    }
    CHECK(wraps > 1000 && draws > 20000 && rev_x > 500 && rev_y > 500, "el aleatorio cubre vueltas/dibujos/rebotes (wraps %d draws %d revx %d revy %d)", wraps, draws, rev_x, rev_y);

    /* 3. caso a mano: slot 1 (bloque, impar, dir X=1, dir Y=1) con toggle 0: sprite_a + 0, paso x=+4, y=+1 */
    init_level2_objects(); memset(cga_mem, 0, CGA_MEM_SIZE);
    memset(l2_obj_active, 1, 24); l2_obj_active[1] = 0;
    l2_obj_x[1] = 100; l2_obj_y[1] = 41; l2_dat_3417[1] = 1; l2_dat_342f[1] = 1; l2_obj_hit[1] = 1;
    l2_dat_3415 = 0; l2_dat_3509 = 0; tick_now = 7; l2_anim_toggle = 0;
    rng_seed = 0;                                                      /* estado 0 del LFSR: random() = 0 siempre -> dl <= 0x10 -> dir X = 0xff, dir Y = 0xff */
    update_level2_objects();
    CHECK(l2_dat_3415 == 1 && l2_dat_350b == 7 && l2_dat_3509 == 0, "a mano: estado de tick");
    CHECK(l2_dat_3417[1] == 0xff && l2_dat_342f[1] == 0xff, "a mano: random()=0 -> and 1 = 0 -> not dl = 0xff (dirs 0xff/0xff)");
    CHECK(l2_obj_x[1] == 96 && l2_obj_y[1] == 40, "a mano: x 100-4, y 41-1 (es %u,%u)", l2_obj_x[1], l2_obj_y[1]);
    CHECK(l2_obj_cga_addr[1] == addr(40, 96) && l2_obj_hit[1] == 0, "a mano: direccion CGA y hit=0");
    {   /* bloque impar, toggle 0, dir != 1 -> sprite_a + 0x18 */
        int bad = 0; for (int r = 0; r < 6; r++) for (int b = 0; b < 2; b++)
            if (cga_mem[addr(40 + r, 96) + b] != ds_pool[0x3300 + 0x18 + r * 2 + b]) bad++;
        CHECK(bad == 0, "a mano: sprite_a+0x18 dibujado (%d bytes mal)", bad);
    }

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_level2c: OK (%d llamadas, %d dibujos, %d vueltas, %d rebotes X, %d rebotes Y)\n", total_calls, draws, wraps, rev_x, rev_y);
    return 0;
}
