/* T38 — tick_level_thrown_objects (level_objects.asm L139-208). Sin SDL.
 * Modelo independiente desde el ASM (AABB asimetrico: objeto 0x18x0xf vs gato 0x18x0xe; el objeto mas alto indice gana; last_picked;
 * pantalla modelo con bancos CGA) + registro de llamadas externas (--wrap) + reloj BIOS falso (l7_tick_fn). Ver PROGRESS.md §6ai. */
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

static uint16_t tick_now;
static uint16_t fake_tick(void) { return tick_now; }

#define ERASE 0x2b7a
static uint8_t model[CGA_MEM_SIZE];
static size_t rowoff(uint16_t S, int r) {
    int b0 = (S >> 13) & 1;
    return (size_t)(S & 0x1fff) + (size_t)(((r + b0) >> 1) * 80) + (size_t)(((b0 ^ (r & 1)) & 1) * 0x2000);
}
static uint16_t caddr(uint8_t row, uint16_t x) { return (uint16_t)((row & 1) * 0x2000 + (row >> 1) * 80 + (x >> 2)); }
static void fill(void) { for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + 3); memcpy(model, cga_mem, CGA_MEM_SIZE); }
static void model_erase(uint8_t y, uint16_t x) {
    uint16_t at = caddr(y, x);
    for (int r = 0; r < 15; r++) memcpy(&model[rowoff(at, r)], &ds_pool[ERASE + r * 6], 6);
}
/* A = objeto (x,y, w=0x18, h=0xf), B = gato (x,y, w=0x18, h=0xe); valores chicos: sin acarreos de 8/16 bits */
static bool touch(int ax, int ay, int bx, int by) {
    return (bx - ax) <= 0x18 && (ax - bx) <= 0x18 && (by - ay) <= 0xf && (ay - by) <= 0xe;
}

static uint32_t rs = 12345;
static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }

static void reset(void) {
    memset(l7_obj_x, 0, sizeof l7_obj_x); memset(l7_obj_y, 0, sizeof l7_obj_y); memset(l7_obj_active, 0, sizeof l7_obj_active);
    l7_obj_spawn_slot = 0xffff; l7_obj_last_picked = 0xffff; joy_button = 0; nrev = 0; fill();
}

int main(void) {
    l7_tick_fn = fake_tick;

    /* 1) mismo tick BIOS: no hace nada */
    reset(); tick_now = 100; l7_obj_last_tick = 100; l7_obj_spawn_slot = 3; l7_obj_last_picked = 5;
    l7_obj_x[1] = 0x50; l7_obj_y[1] = 104; l7_obj_active[1] = 1; cat_x = 0x50; cat_y = 104;
    tick_level_thrown_objects();
    CHECK(nrev == 0 && l7_obj_last_picked == 5 && l7_obj_active[1] == 1 && l7_obj_spawn_slot == 3, "mismo tick no debe tocar nada");

    /* 2) spawn pendiente (slot < 8): actualiza last_tick, last_picked=0xffff y no escanea */
    tick_now = 101; tick_level_thrown_objects();
    CHECK(l7_obj_last_tick == 101 && l7_obj_last_picked == 0xffff && nrev == 0 && l7_obj_active[1] == 1 && l7_obj_spawn_slot == 3,
          "spawn pendiente: last_tick=%u last_picked=%x", l7_obj_last_tick, l7_obj_last_picked);

    /* 3) limites del solape (asimetrico en Y: gato por debajo hasta 0xf, por encima hasta 0xe) */
    for (int dx = -0x1c; dx <= 0x1c; dx += 4) for (int dy = -0x12; dy <= 0x12; dy++) {
        reset(); tick_now = (uint16_t)(tick_now + 1); l7_obj_last_tick = (uint16_t)(tick_now - 1);
        cat_x = 0x60; cat_y = 100; l7_obj_x[4] = (int16_t)(0x60 + dx); l7_obj_y[4] = (uint8_t)(100 + dy); l7_obj_active[4] = 1;
        tick_level_thrown_objects();
        bool t = touch(0x60 + dx, 100 + dy, 0x60, 100);
        CHECK((l7_obj_active[4] == 0) == t, "dx=%d dy=%d modelo=%d activo=%d", dx, dy, t, l7_obj_active[4]);
        CHECK(t ? (l7_obj_spawn_slot == 4 && nrev == 5) : (l7_obj_spawn_slot == 0xffff && nrev == 0 && l7_obj_last_picked == 0xffff), "dx=%d dy=%d efectos", dx, dy);
    }

    /* 4) aleatorio contra el modelo: varios objetos, gana el de MAYOR indice; el ultimo recogido se ignora (sale sin tocar nada) */
    int picks = 0, ignored = 0, nones = 0;
    for (int it = 0; it < 30000; it++) {
        reset();
        int cx = 0x20 + (int)(rnd() % 0xa0), cy = 0x30 + (int)(rnd() % 0x60);   /* y <= 0xa8: el sprite de 15 filas cabe en la pantalla CGA */
        cat_x = (int16_t)cx; cat_y = (uint8_t)cy;
        uint8_t act[8]; int ox[8], oy[8];
        for (int k = 0; k < 8; k++) {
            act[k] = (rnd() % 3) != 0;
            ox[k] = cx + (int)(rnd() % 0x50) - 0x28; oy[k] = cy + (int)(rnd() % 0x30) - 0x18;
            l7_obj_x[k] = (int16_t)ox[k]; l7_obj_y[k] = (uint8_t)oy[k]; l7_obj_active[k] = act[k];
        }
        uint16_t lp = (rnd() % 3 == 0) ? (uint16_t)(rnd() % 8) : 0xffff;
        l7_obj_last_picked = lp;
        tick_now = (uint16_t)(1000 + it); l7_obj_last_tick = (uint16_t)(tick_now - 1);
        l7_obj_spawn_slot = (rnd() % 5 == 0) ? (uint16_t)(rnd() % 8) : (uint16_t)(rnd() % 2 ? 8 : 0xffff);
        uint16_t sp0 = l7_obj_spawn_slot;
        uint8_t act0[8]; memcpy(act0, act, 8);
        tick_level_thrown_objects();

        if (sp0 < 8) {                                    /* spawn pendiente */
            CHECK(l7_obj_last_picked == 0xffff && nrev == 0 && memcmp(l7_obj_active, act0, 8) == 0, "it=%d pendiente", it);
            continue;
        }
        int hit = -1;
        for (int k = 7; k >= 0; k--) if (act[k] && touch(ox[k], oy[k], cx, cy)) { hit = k; break; }
        if (hit < 0) {
            nones++;
            CHECK(l7_obj_last_picked == 0xffff && nrev == 0 && memcmp(l7_obj_active, act0, 8) == 0 && l7_obj_spawn_slot == sp0, "it=%d sin choque", it);
        } else if (hit == lp) {
            ignored++;
            CHECK(l7_obj_last_picked == lp && nrev == 0 && memcmp(l7_obj_active, act0, 8) == 0 && l7_obj_spawn_slot == sp0, "it=%d ignorado", it);
        } else {
            picks++;
            act0[hit] = 0; model_erase((uint8_t)oy[hit], (uint16_t)ox[hit]);
            CHECK(memcmp(l7_obj_active, act0, 8) == 0 && l7_obj_spawn_slot == hit && l7_obj_last_picked == lp, "it=%d recogida %d", it, hit);
            CHECK(nrev == 5 && rev[0] == E_RESTORE && rev[1] == E_FG && rev[2] == E_TONE && rev[3] == 0x3e8 && rev[4] == 0x349, "it=%d eventos", it);
            CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) == 0, "it=%d pantalla", it);
        }
    }

    /* 5) ciclo completo con spawn_thrown_object: recoger -> reponer en el mismo slot -> no se re-recoge encima -> al alejarse se rearma */
    reset(); cat_y = 104; cat_x = 0x54;
    l7_obj_x[2] = 0x54; l7_obj_y[2] = 104; l7_obj_active[2] = 1;
    tick_now = 5000; l7_obj_last_tick = 4999; tick_level_thrown_objects();
    CHECK(l7_obj_active[2] == 0 && l7_obj_spawn_slot == 2, "ciclo: recogido");
    spawn_thrown_object();
    CHECK(l7_obj_active[2] == 1 && l7_obj_last_picked == 2 && l7_obj_spawn_slot == 0xffff, "ciclo: repuesto en el mismo slot");
    tick_now = 5001; nrev = 0; tick_level_thrown_objects();
    CHECK(l7_obj_active[2] == 1 && l7_obj_last_picked == 2 && nrev == 0, "ciclo: no se re-recoge encima (last_picked)");
    cat_x = 0x100; tick_now = 5002; tick_level_thrown_objects();
    CHECK(l7_obj_last_picked == 0xffff && l7_obj_active[2] == 1, "ciclo: al alejarse last_picked se rearma");
    cat_x = 0x54; tick_now = 5003; tick_level_thrown_objects();
    CHECK(l7_obj_active[2] == 0 && l7_obj_spawn_slot == 2, "ciclo: al volver se recoge");

    if (fails) { printf("test_level7b: %d FALLOS\n", fails); return 1; }
    printf("test_level7b: OK (%d recogidas, %d ignoradas por last_picked, %d sin choque)\n", picks, ignored, nones);
    return 0;
}
