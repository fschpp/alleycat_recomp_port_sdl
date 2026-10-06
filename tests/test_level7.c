/* T37 — spawn_thrown_object (level_objects.asm L41-138). Sin SDL.
 * Modelo independiente escrito desde el ASM (distancia con NOT, empates al indice mas bajo, clamp de X, AABB 0x18x0xf propio,
 * pantalla modelo con intercalado de bancos CGA) + registro del orden de llamadas externas (--wrap). Ver PROGRESS.md §6ah. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level7_epilogue.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

enum { E_RESTORE = 1, E_FG, E_TONE };
#define MAXEV 16
static int rev[MAXEV]; static int nrev;
static void rlog(int v) { if (nrev < MAXEV) rev[nrev++] = v; }
void __wrap_restore_alley_buffer(void) { rlog(E_RESTORE); }
void __wrap_draw_alley_foreground(void) { rlog(E_FG); }
void __wrap_start_tone(uint16_t a, uint16_t b) { rlog(E_TONE); rlog(a); rlog(b); }

static const uint8_t ROWS[7] = {176, 152, 128, 104, 80, 56, 32};   /* window_row_y_table (7 usadas; la 8a es 0) */
#define SPRITE 0x2af0

static uint8_t model[CGA_MEM_SIZE];
static size_t rowoff(uint16_t S, int r) {
    int b0 = (S >> 13) & 1;
    return (size_t)(S & 0x1fff) + (size_t)(((r + b0) >> 1) * 80) + (size_t)(((b0 ^ (r & 1)) & 1) * 0x2000);
}
static uint16_t caddr(uint8_t row, uint16_t x) { return (uint16_t)((row & 1) * 0x2000 + (row >> 1) * 80 + (x >> 2)); }
static void fill(void) { for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + 3); memcpy(model, cga_mem, CGA_MEM_SIZE); }

/* modelo: fila elegida para un cat_y (NOT en vez de NEG; empate -> indice mas bajo) */
static int model_dist;
static int model_row(uint8_t cy) {
    int best = -1, bd = 0xff;
    for (int i = 6; i >= 0; i--) {
        int d = cy >= ROWS[i] ? cy - ROWS[i] : (uint8_t)~(uint8_t)(cy - ROWS[i]);
        if (d <= bd) { bd = d; best = i; }
    }
    model_dist = bd;
    return best;
}
static uint16_t mseed;
static uint8_t m_random(void) {
    uint8_t lo = (uint8_t)(mseed & 0xff), hi = (uint8_t)(mseed >> 8);
    unsigned c = (unsigned)(((lo ^ hi) >> 1) & 1);
    mseed = (uint16_t)((mseed >> 1) | (c << 15));
    return (uint8_t)(mseed & 0xff);
}
static uint16_t model_x(uint16_t cx) { if (cx >= 0x108) cx = 0x107; return cx & 0xffc; }
static bool model_overlap(uint16_t x1, uint8_t y1, uint16_t x2, uint8_t y2) {
    int dx = (int)x1 - (int)x2, dy = (int)y1 - (int)y2;
    return dx <= 0x18 && -dx <= 0x18 && dy <= 0xf && -dy <= 0xf;
}

static void reset(void) {
    memset(l7_obj_x, 0, sizeof l7_obj_x); memset(l7_obj_y, 0, sizeof l7_obj_y); memset(l7_obj_active, 0, sizeof l7_obj_active);
    l7_obj_spawn_slot = 0xffff; joy_button = 0; nrev = 0; fill();
}

int main(void) {
    /* 1) cat_y exhaustivo x algunos cat_x: fila, X, slot activo, pantalla y orden de eventos */
    static const uint16_t XS[] = {0, 3, 0x54, 0x100, 0x107, 0x108, 0x123, 0x1ff, 0xffff};
    int n = 0;
    for (int cy = 0; cy < 256; cy++) for (size_t xi = 0; xi < sizeof XS / sizeof *XS; xi++) {
        reset();
        int slot = (cy + (int)xi) & 7;
        cat_y = (uint8_t)cy; cat_x = (int16_t)XS[xi];
        l7_obj_spawn_slot = (uint16_t)slot;
        spawn_thrown_object();
        int r = model_row((uint8_t)cy); uint16_t mx = model_x(XS[xi]);
        /* los gatos-corazon siguen a cero (cat en 0,0 y filas >= 32): ningun objeto recien puesto se atrapa solo */
        CHECK(l7_obj_y[slot] == ROWS[r], "cy=%d x=%u: y=%u esperado %u", cy, XS[xi], l7_obj_y[slot], ROWS[r]);
        CHECK((uint16_t)l7_obj_x[slot] == mx, "cy=%d x=%u: x=%u esperado %u", cy, XS[xi], l7_obj_x[slot], mx);
        CHECK(l7_obj_closest_row == (uint16_t)r && l7_obj_closest_dist == model_dist, "cy=%d: closest row=%u dist=%u esperado %d/%d",
              cy, l7_obj_closest_row, l7_obj_closest_dist, r, model_dist);   /* dist distingue `not` de `neg` (la fila elegida no) */
        CHECK(l7_obj_cur_x == mx && l7_obj_cur_y == ROWS[r], "cur_x/cur_y");
        CHECK(l7_obj_spawn_slot == 0xffff && l7_obj_last_picked == slot, "spawn_slot/last_picked");
        CHECK(nrev == 5 && rev[0] == E_RESTORE && rev[1] == E_FG && rev[2] == E_TONE && rev[3] == 0x3e8 && rev[4] == 0x4a5, "eventos (%d)", nrev);
        n++;
    }

    /* 2) escenario sin captura: cat (0,0) lejos del objeto -> queda activo y el sprite esta dibujado */
    reset(); cat_y = 100; cat_x = 0x54; l7_obj_spawn_slot = 2;
    spawn_thrown_object();
    {
        uint16_t at = caddr(104, 0x54);
        for (int r = 0; r < 15; r++) memcpy(&model[rowoff(at, r)], &ds_pool[SPRITE + r * 6], 6);
        CHECK(l7_obj_active[2] == 1, "slot 2 activo");
        CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) == 0, "pantalla: sprite 3x15 en fila 104, x=0x54");
    }

    /* 3) guardas: slot >= 8 y joy_button != 0 no hacen nada */
    {
        static const uint16_t BAD[] = {8, 9, 0x100, 0xffff};
        for (size_t i = 0; i < sizeof BAD / sizeof *BAD; i++) {
            reset(); cat_y = 100; cat_x = 0x54; l7_obj_spawn_slot = BAD[i]; l7_obj_closest_row = 0x1234; spawn_thrown_object();
            bool none = true; for (int k = 0; k < 8; k++) if (l7_obj_active[k]) none = false;
            CHECK(nrev == 0 && l7_obj_spawn_slot == BAD[i] && none && l7_obj_closest_row == 0x1234, "slot=%u", BAD[i]);
        }
    }
    reset(); cat_y = 100; cat_x = 0x54; l7_obj_spawn_slot = 1; joy_button = 0x10; l7_obj_closest_row = 0x1234; spawn_thrown_object();
    CHECK(nrev == 0 && l7_obj_spawn_slot == 1 && l7_obj_active[1] == 0 && l7_obj_closest_row == 0x1234, "joy_button bloquea antes de tocar estado");

    /* 4) solapamiento con otro objeto activo (rect 0x18x0xf): aborta sin dibujar ni sonar; el limite es inclusivo */
    for (int dxs = -0x20; dxs <= 0x20; dxs += 4) for (int dys = -0x14; dys <= 0x14; dys += 4) {
        reset(); cat_y = 104; cat_x = 0x54;                           /* nuevo: fila 104, x = 0x54 */
        int ox = 0x54 + dxs, oy = 104 + dys;
        if (ox < 0 || oy < 0) continue;
        l7_obj_x[5] = (int16_t)ox; l7_obj_y[5] = (uint8_t)oy; l7_obj_active[5] = 1;
        l7_obj_spawn_slot = 2; spawn_thrown_object();
        bool ov = model_overlap((uint16_t)ox, (uint8_t)oy, 0x54, 104);
        CHECK((l7_obj_active[2] == 0) == ov, "dx=%d dy=%d solape modelo=%d activo=%d", dxs, dys, ov, l7_obj_active[2]);
        CHECK(ov ? (nrev == 0 && l7_obj_spawn_slot == 2) : (nrev == 5 && l7_obj_spawn_slot == 0xffff), "dx=%d dy=%d efectos", dxs, dys);
    }
    /* un objeto INACTIVO en la misma posicion no cuenta; ni el propio slot */
    reset(); cat_y = 104; cat_x = 0x54; l7_obj_x[5] = 0x54; l7_obj_y[5] = 104; l7_obj_active[5] = 0;
    l7_obj_x[2] = 0x54; l7_obj_y[2] = 104; l7_obj_active[2] = 1; l7_obj_spawn_slot = 2; spawn_thrown_object();
    CHECK(nrev == 5, "inactivos y el propio slot no bloquean");

    /* 5) check_l7_object_overlap deja de ser inerte: con init_level7_objects (RNG con semilla; modelo LFSR propio) el gato-corazon 1
     *    queda en y=155, x=0x60+(rand&0x7f). Un spawn lejos no lo toca; uno encima se atrapa solo y su objeto se desactiva. */
    for (int k = 0; k < 40; k++) {
        uint16_t seed = (uint16_t)(0x1111 + k * 0x0707);
        mseed = seed; (void)m_random();
        uint16_t sx = (uint16_t)((m_random() & 0x7f) + 0x60);          /* 2o valor = slot (heart_index) 1 */
        reset(); rng_seed = seed; init_level7_objects();
        uint16_t far_x = (sx + 0x30 <= 0x104) ? (uint16_t)(sx + 0x30) : (uint16_t)(sx - 0x30);
        cat_y = 152; cat_x = (int16_t)far_x; l7_obj_spawn_slot = 3; spawn_thrown_object();
        CHECK(l7_obj_active[3] == 1, "seed=%04x lejos: debe quedar activo", seed);
        cat_x = (int16_t)sx; l7_obj_spawn_slot = 4; spawn_thrown_object();
        CHECK(l7_obj_active[4] == 0 && l7_obj_last_picked == 4, "seed=%04x encima: debe atraparse (activo=%d)", seed, l7_obj_active[4]);
        CHECK(l7_obj_active[3] == 1, "seed=%04x el objeto lejano no cambia", seed);
    }

    if (fails) { printf("test_level7: %d FALLOS\n", fails); return 1; }
    printf("test_level7: OK (%d escenarios de fila/X)\n", n);
    return 0;
}
