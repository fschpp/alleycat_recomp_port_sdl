/* T32 — update_level6_movement (level_objects.asm L2817-2983). Sin SDL.
 * Los valores esperados NO salen del C: un modelo ESTRUCTURADO (if anidados, no goto) escrito desde el ASM, una pantalla modelo
 * (tracker AND-blit + save/restore, tiles 2x8) y un registro del ORDEN de las llamadas externas (envueltas con --wrap):
 * erase/draw_l1_thrown, save/restore_alley_buffer, draw_alley_foreground, start_tone, check_thrown_near_cat. Ver PROGRESS.md §6ac. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "alley.h"
#include "level6.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)


/* ---- registro de eventos (real y modelo) ---- */
enum { E_ERASE_L1 = 1, E_DRAW_L1, E_SAVE, E_RESTORE, E_FG, E_NEAR, E_TONE };
#define MAXEV 64
static int rev[MAXEV], mev[MAXEV]; static int nrev, nmev;
static void rlog(int v) { if (nrev < MAXEV) rev[nrev++] = v; }
static void mlog(int v) { if (nmev < MAXEV) mev[nmev++] = v; }
static bool g_near;                                   /* valor que devuelve check_thrown_near_cat */

void __wrap_erase_l1_thrown(void)   { rlog(E_ERASE_L1); }
void __wrap_draw_l1_thrown(void)    { rlog(E_DRAW_L1); }
void __wrap_save_alley_buffer(void) { rlog(E_SAVE); }
void __wrap_restore_alley_buffer(void) { rlog(E_RESTORE); }
void __wrap_draw_alley_foreground(void) { rlog(E_FG); }
void __wrap_start_tone(uint16_t a, uint16_t b) { rlog(E_TONE); rlog(a); rlog(b); }
bool __wrap_check_thrown_near_cat(void) { rlog(E_NEAR); return g_near; }

/* ---- pantalla modelo ---- */
static uint8_t model[CGA_MEM_SIZE];
static uint16_t addr(uint8_t row, uint16_t x) { return (uint16_t)((row & 1) * 0x2000 + (row >> 1) * 80 + (x >> 2)); }
/* fila r de un bloque que empieza en la direccion S (el banco de S puede ser impar: el entrelazado CGA continua desde ahi) */
static size_t rowoff(uint16_t S, int r) {
    int b0 = (S >> 13) & 1;
    return (size_t)(S & 0x1fff) + (size_t)(((r + b0) >> 1) * 80) + (size_t)(((b0 ^ (r & 1)) & 1) * 0x2000);
}
static void put(const uint8_t *src, uint16_t at, int w, int h) {
    for (int r = 0; r < h; r++) memcpy(&model[rowoff(at, r)], src + r * w * 2, (size_t)w * 2);
}
static void fill(void) { for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + 3); memcpy(model, cga_mem, CGA_MEM_SIZE); }
static bool same(void) { return memcmp(cga_mem, model, CGA_MEM_SIZE) == 0; }

/* tablas de datos (solo lectura en el DS) */
static const uint16_t TX[12] = {0x8, 0x90, 0xa0, 0x28, 0x38, 0x78, 0xe0, 0x120, 0x10, 0x98, 0xd0, 0x100};       /* l6_obj_dims (X) */
static const uint8_t  TY[12] = {0x90, 0x90, 0x90, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xb0, 0xb0, 0xb0, 0xb0};          /* l6_obj_y */
static const uint16_t SP[12] = {0x4184, 0x4184, 0x410c, 0x4184, 0x410c, 0x410c, 0x4184, 0x410c, 0x4184, 0x4184, 0x4184, 0x410c}; /* dat_44a5 */

/* ---- estado del modelo ---- */
static struct {
    uint16_t d3, bf, c1, d1, dc, de, dx7; uint8_t c3, d5, bd, be, d0, e0, d6, tiles[12];
    int8_t ih, iv, sdir, mode; uint16_t speed; uint8_t joy, caught; uint8_t trk[60];
} m;

static void m_erase_trk(void) { if (m.e0 == 0) put(m.trk, m.de, 3, 10); }
static void m_draw_trk(void) {
    m.e0 = 0; m.de = m.dc;
    uint16_t si = m.d1; if (m.d0 >= 0x80) si = (uint16_t)(si + 0x3c);
    for (int r = 0; r < 10; r++)
        for (int b = 0; b < 6; b++) {
            size_t off = rowoff(m.dc, r) + (size_t)b;
            m.trk[r * 6 + b] = model[off];
            model[off] &= ds_pool[si + r * 6 + b];
        }
}
static void m_tile(int slot) { put(&ds_pool[0x41fc + m.tiles[slot] * 32], addr(TY[slot], TX[slot]), 2, 8); }

static void m_retire(void) {                              /* lab_49f9 + lab_4a0b */
    if (m.bd) { m_erase_trk(); mlog(E_FG); m.joy = 0x10; }
    m.bd = 0; m.e0 = 1; m.d0 = 0; m.be = 0;
}

static void m_move(uint16_t tick) {
    if (enemy_active) return;
    if (m.be) { m.ih = (int8_t)m.be; m.iv = 0; }
    if (tick == m.d3) return;
    m.d3 = tick;
    if (auto_walk) {
        if (m.bd) { m_erase_trk(); mlog(E_ERASE_L1); mlog(E_SAVE); mlog(E_DRAW_L1); m.bd = 0; m.e0 = 1; m.be = 0; }
        return;
    }
    if (m.joy) { m_retire(); return; }
    uint16_t best = 0xffff, idx = 0xffff; uint8_t dir = 0;
    uint8_t row = (uint8_t)(cat_y + 8);
    for (int slot = 11; slot >= 0; slot--) {
        if (m.tiles[slot] < 1 || row != TY[slot]) continue;
        uint16_t c = (uint16_t)cat_x, d; uint8_t dh;
        if (c >= TX[slot]) { d = (uint16_t)(c - TX[slot]); dh = 0xff; }
        else               { d = (uint16_t)(TX[slot] - c - 1); dh = 1; }        /* sub + not */
        if (d > best) continue;
        best = d; m.d1 = SP[slot]; idx = (uint16_t)slot; dir = dh; m.c3 = dh;   /* dat_44c3 solo se escribe con candidato */
    }
    m.bf = best; m.c1 = idx;
    if (idx >= 12) { m_retire(); return; }
    if (best >= 4) {
        if (best <= 8) m.speed = (uint16_t)((m.speed & 0xff00) | 4);
        m.ih = (int8_t)dir; m.sdir = (int8_t)dir; m.be = dir; m.iv = 0; m.mode = 0;
        m_retire(); return;
    }
    /* best < 4: el gato esta sobre el tile */
    m.be = 0;
    if (!m.bd) { mlog(E_RESTORE); mlog(E_SAVE); }
    m.bd = 1;
    unsigned sum = (unsigned)m.d0 + 0x30; m.d0 = (uint8_t)sum;
    m.d5 = sum > 0xff ? 1 : 0;
    uint16_t cx = (uint16_t)((uint16_t)cat_x & 0xffc);
    uint8_t dl = (uint8_t)(cat_y + 3);
    if (m.d1 == 0x410c) cx = cx >= 8 ? (uint16_t)(cx - 8) : 0;
    else { cx = (uint16_t)(cx + 8); if (cx >= 0x127) cx = 0x126; }
    m.dc = addr(dl, cx);
    m_erase_trk();
    if (m.d5 && m.tiles[idx]) {
        m.tiles[idx]--;
        if (m.tiles[idx] == 0) {
            mlog(E_TONE); mlog(0x8fd); mlog(0x723);
            m.ih = 0; m.be = 0; m.joy = 0x10;
            if (--m.d6 == 0) m.caught = 1;
        }
        mlog(E_NEAR);
        if (g_near) { mlog(E_ERASE_L1); m_tile(idx); mlog(E_DRAW_L1); }
        else m_tile(idx);
    }
    m_draw_trk();
}

/* ---- sincronizacion real <-> modelo ---- */
static uint32_t lcg = 0x2545f491;
static uint32_t rnd(void) { lcg = lcg * 1664525u + 1013904223u; return lcg >> 8; }

static void setup(const uint8_t tiles[12], uint8_t bd, uint8_t be, uint8_t d0, uint16_t d1, uint8_t d6, uint8_t joy,
                  uint8_t aw, int8_t ih, int8_t iv, uint16_t speed, int16_t cx, uint8_t cy, uint16_t d3, bool near_, bool drawn) {
    fill();
    memcpy(l6_tile_type, tiles, 12); memcpy(m.tiles, tiles, 12);
    l6_dat_44d1 = m.d1 = d1; l6_dat_44d0 = m.d0 = d0; l6_dat_44d6 = m.d6 = d6; joy_button = m.joy = joy; auto_walk = aw;
    input_horizontal = m.ih = ih; input_vertical = m.iv = iv; scroll_speed = m.speed = speed; scroll_direction = m.sdir = 0x55;
    in_level_mode = m.mode = 0x66; cat_caught = m.caught = 0;
    cat_x = cx; cat_y = cy; l6_dat_44d3 = m.d3 = d3; g_near = near_; enemy_active = 0;
    l6_dat_44be = m.be = be; l6_dat_44bf = m.bf = 0x1111; l6_dat_44c1 = m.c1 = 0x2222; l6_dat_44c3 = m.c3 = 0x33; l6_dat_44d5 = m.d5 = 0x44;
    l6_dat_43dc = m.dc = 0x0a00; l6_dat_43e0 = m.e0 = 1; l6_dat_43de = m.de = 0;
    l6_dat_44bd = m.bd = bd;
    nrev = nmev = 0;
    if (drawn) {                                        /* deja un tracker dibujado: erase tiene algo que restaurar */
        l6_dat_43dc = m.dc = 0x0b00; l6_dat_44d1 = m.d1 = 0x4184; l6_dat_44d0 = m.d0 = 0x00;
        draw_l6_tracker(); m_draw_trk();
        l6_dat_44d1 = m.d1 = d1; l6_dat_44d0 = m.d0 = d0; l6_dat_43dc = m.dc = 0x0a00;
    }
}

static bool vars_same(void) {
    return l6_dat_44d3 == m.d3 && l6_dat_44bf == m.bf && l6_dat_44c1 == m.c1 && l6_dat_44c3 == m.c3 && l6_dat_44d5 == m.d5 &&
           l6_dat_44bd == m.bd && l6_dat_44be == m.be && l6_dat_44d0 == m.d0 && l6_dat_43e0 == m.e0 && l6_dat_44d6 == m.d6 &&
           l6_dat_44d1 == m.d1 && l6_dat_43dc == m.dc && l6_dat_43de == m.de &&
           input_horizontal == m.ih && input_vertical == m.iv && scroll_direction == m.sdir && in_level_mode == m.mode &&
           scroll_speed == m.speed && joy_button == m.joy && cat_caught == m.caught && memcmp(l6_tile_type, m.tiles, 12) == 0;
}
static bool ev_same(void) { return nrev == nmev && memcmp(rev, mev, sizeof(int) * (size_t)nrev) == 0; }

static void run(uint16_t tick) {
    l6_tick_override = tick;
    update_level6_movement();
    m_move(tick);
}
#define VERIFY(tag) do { \
    CHECK(vars_same(), "%s: variables != modelo", tag); \
    CHECK(same(), "%s: pantalla != modelo", tag); \
    CHECK(ev_same(), "%s: orden de llamadas != modelo (real %d eventos, modelo %d)", tag, nrev, nmev); } while (0)

int main(void) {
    static const uint8_t none[12] = {0}, all1[12] = {1,1,1,1,1,1,1,1,1,1,1,1};
    level_number = 6;

    /* 1) guardas: enemy_active no cambia nada salvo las entradas (si dat_44be != 0); mismo tick no cambia nada mas */
    setup(all1, 1, 0xff, 0, 0x410c, 5, 0, 0, 7, 7, 0x1234, 0x40, 0x98, 500, false, true);
    enemy_active = 1; l6_tick_override = 501; update_level6_movement();
    CHECK(input_horizontal == 7 && input_vertical == 7 && l6_dat_44d3 == 500 && l6_dat_44bd == 1, "1a: enemy_active no toca nada (ni las entradas)");
    CHECK(nrev == 0, "1a: llamadas inesperadas");
    setup(all1, 1, 0x01, 0, 0x410c, 5, 0, 0, 7, 7, 0x1234, 0x40, 0x98, 500, false, true);
    run(500);
    CHECK(input_horizontal == 1 && input_vertical == 0 && l6_dat_44bd == 1, "1b: mismo tick: solo entradas");
    VERIFY("1b");
    setup(all1, 1, 0x00, 0, 0x410c, 5, 0, 0, 7, 7, 0x1234, 0x40, 0x98, 500, false, true);
    run(500);
    CHECK(input_horizontal == 7 && input_vertical == 7, "1c: dat_44be == 0 no toca las entradas");
    VERIFY("1c");

    /* 2) auto_walk: con tracker dibujado (dat_44bd = 1) lo retira y llama erase/save/draw del lanzado; sin tracker, nada */
    for (int bd = 0; bd <= 1; bd++) {
        setup(all1, (uint8_t)bd, 1, 0x40, 0x410c, 5, 0, 1, 7, 7, 0x1234, 0x40, 0x98, 500, false, bd);
        run(501);
        CHECK(l6_dat_44bd == 0 && (bd ? (l6_dat_44be == 0 && l6_dat_43e0 == 1) : l6_dat_44be == 1), "2.%d: estado", bd);
        CHECK(nrev == (bd ? 3 : 0), "2.%d: %d llamadas", bd, nrev);
        VERIFY("2");
    }

    /* 3) joy_button != 0: retira el tracker (con 44bd=1 pone joy_button=0x10 y dibuja el gato), sin buscar tiles */
    for (int bd = 0; bd <= 1; bd++) {
        setup(all1, (uint8_t)bd, 1, 0x40, 0x410c, 5, 0x05, 0, 7, 7, 0x1234, 0x40, 0x98, 500, false, bd);
        run(501);
        CHECK(joy_button == (bd ? 0x10 : 0x05) && l6_dat_44c1 == 0x2222 && l6_dat_44bf == 0x1111, "3.%d: joy=%02x", bd, joy_button);
        VERIFY("3");
    }

    /* 4) busqueda: sin tiles utiles (tipo 0 o fila distinta) -> dat_44c1 = 0xffff y retira */
    setup(none, 1, 1, 0x40, 0x410c, 5, 0, 0, 7, 7, 0x1234, 0x90, 0x88, 500, false, true);
    run(501);
    CHECK(l6_dat_44c1 == 0xffff && l6_dat_44bf == 0xffff && l6_dat_44bd == 0, "4a: sin tiles");
    VERIFY("4a");
    setup(all1, 0, 0, 0x40, 0x410c, 5, 0, 0, 7, 7, 0x1234, 0x90, 0x70, 500, false, false);
    run(501);
    CHECK(l6_dat_44c1 == 0xffff, "4b: fila distinta");
    VERIFY("4b");

    /* 5) distancias: >= 9 camina (sin tocar scroll_speed); 4..8 ademas scroll_speed = 4 (solo el byte bajo); < 4 el tracker */
    {
        /* fila 0xa0 (cat_y = 0x98): tile slot 5 en X=0x78; el gato a la izquierda y a la derecha */
        static const struct { int16_t cx; uint16_t dist; uint8_t dir; } t[] = {
            {0x78 + 9, 9, 0xff}, {0x78 + 8, 8, 0xff}, {0x78 + 4, 4, 0xff}, {0x78 + 3, 3, 0xff}, {0x78, 0, 0xff},
            {0x78 - 1, 0, 1}, {0x78 - 4, 3, 1}, {0x78 - 5, 4, 1}, {0x78 - 9, 8, 1}, {0x78 - 10, 9, 1}, };
        static uint8_t only5[12] = {0}; only5[5] = 3;
        for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
            setup(only5, 0, 0, 0x00, 0x410c, 5, 0, 0, 7, 7, 0x1234, t[i].cx, 0x98, 500, false, false);
            run(501);
            CHECK(l6_dat_44bf == t[i].dist && l6_dat_44c3 == t[i].dir && l6_dat_44c1 == 5, "5.%u: dist=%u dir=%02x c1=%u", i, l6_dat_44bf, l6_dat_44c3, l6_dat_44c1);
            if (t[i].dist >= 9) CHECK(scroll_speed == 0x1234, "5.%u: speed=%04x", i, scroll_speed);
            else if (t[i].dist >= 4) CHECK(scroll_speed == 0x1204, "5.%u: speed=%04x", i, scroll_speed);
            if (t[i].dist >= 4) CHECK(input_horizontal == (int8_t)t[i].dir && scroll_direction == (int8_t)t[i].dir && in_level_mode == 0 && input_vertical == 0, "5.%u: entradas", i);
            VERIFY("5");
        }
    }

    /* 6) tracker sobre el tile: acarreo de dat_44d0 (0xd0 + 0x30 = 0x100 -> d5 = 1 y gasta un uso), clamps de X, sprite 0x410c/0x4184 */
    {
        static const uint8_t d0s[] = {0x00, 0x30, 0xcf, 0xd0, 0xff, 0x80};
        static const int16_t xo[] = {0, 1, 2, 3};                         /* gato a 0..3 px a la derecha del tile */
        static uint8_t t[12]; memset(t, 0, 12);
        for (int slot = 0; slot < 12; slot++) {
            t[slot] = 3;
            uint8_t cy = (uint8_t)(TY[slot] - 8);
            for (unsigned a = 0; a < sizeof d0s; a++)
                for (unsigned b = 0; b < sizeof xo / sizeof xo[0]; b++)
                    for (int sp = 0; sp < 2; sp++) {
                        /* el sprite del tracker lo fija la busqueda desde dat_44a5; el gato cae sobre el tile -> distancia xo[b] */
                        setup(t, 0, 1, d0s[a], (uint16_t)(sp ? 0x410c : 0x4184), 5, 0, 0, 7, 7, 0x1234,
                              (int16_t)(TX[slot] + xo[b]), cy, 500, (a + b) & 1, (a & 1));
                        run(501);
                        VERIFY("6");
                    }
            t[slot] = 0;
        }
    }

    /* 7) agotar un tile (tipo 1 -> 0): sonido, entradas, joy_button, dat_44d6--, y cat_caught al llegar a 0 */
    for (int d6 = 1; d6 <= 2; d6++)
        for (int near_ = 0; near_ <= 1; near_++) {
            static uint8_t t[12]; memset(t, 0, 12); t[5] = 1;
            setup(t, 0, 1, 0xd0, 0x410c, (uint8_t)d6, 0, 0, 7, 7, 0x1234, 0x78, 0x98, 500, near_, false);
            run(501);
            CHECK(l6_tile_type[5] == 0 && joy_button == 0x10 && input_horizontal == 0 && l6_dat_44be == 0, "7.%d.%d: efectos", d6, near_);
            CHECK(l6_dat_44d6 == d6 - 1 && cat_caught == (d6 == 1), "7.%d.%d: d6=%u caught=%u", d6, near_, l6_dat_44d6, cat_caught);
            CHECK(nrev >= 5 && rev[0] == E_RESTORE && rev[1] == E_SAVE && rev[2] == E_TONE && rev[3] == 0x8fd && rev[4] == 0x723, "7.%d.%d: start_tone(0x8fd,0x723) tras restore/save", d6, near_);
            VERIFY("7");
        }

    /* 8) aleatorio: 4000 escenarios (tiles, gato, flags, tick, lanzado cerca); variables, pantalla y orden de llamadas == modelo */
    for (int it = 0; it < 4000; it++) {
        static uint8_t t[12];
        for (int i = 0; i < 12; i++) t[i] = (uint8_t)((rnd() % 3) ? (rnd() % 5) : 0);
        uint8_t cy; switch (rnd() % 5) { case 0: cy = 0x88; break; case 1: cy = 0x98; break; case 2: cy = 0xa8; break; case 3: cy = 0xa9; break; default: cy = (uint8_t)rnd(); }
        int16_t cx = (rnd() % 3) ? (int16_t)(TX[rnd() % 12] + (int)(rnd() % 21) - 10) : (int16_t)(rnd() % 0x140);
        if (cx < 0) cx = 0;
        setup(t, rnd() & 1, (rnd() % 3) ? 0 : (uint8_t)((rnd() & 1) ? 1 : 0xff), (uint8_t)((rnd() % 3) ? (rnd() % 0x100) : 0xd0),
              (rnd() & 1) ? 0x410c : 0x4184, (uint8_t)(1 + rnd() % 3), (rnd() % 8) ? 0 : 5, (rnd() % 8) ? 0 : 1,
              (int8_t)rnd(), (int8_t)rnd(), (uint16_t)rnd(), cx, cy, (uint16_t)(500 + (rnd() % 4 ? 0 : 3)), rnd() & 1, rnd() & 1);
        enemy_active = (rnd() % 12) == 0;
        run(501);
        char tag[32]; snprintf(tag, sizeof tag, "8.%d", it);
        VERIFY(tag);
        if (fails > 12) break;
    }

    if (fails) { printf("test_level6d: %d FALLOS\n", fails); return 1; }
    printf("test_level6d: OK\n");
    return 0;
}
