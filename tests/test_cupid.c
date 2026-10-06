/* T56 — reset_cupid y update_cupid (ui.asm L571-675). Sin SDL; los helpers de T57 y el gato se envuelven y se registran. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "cupid.h"
#include "input.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static char log_[256]; static int nlog;
static void rec(char c) { if (nlog < 255) log_[nlog++] = c; log_[nlog] = 0; }
static int coll_script[512], ncoll, collpos;      /* valores de CF de check_cupid_collision */
static uint16_t rnd_script[64]; static int nrnd, rndpos;
int  __wrap_check_cupid_collision(void) { rec('?'); return collpos < ncoll ? coll_script[collpos++] : 0; }
void __wrap_erase_cupid(void)           { rec('E'); }
void __wrap_draw_cupid(void)            { rec('D'); }
void __wrap_cupid_toggle_window(void)   { rec('W'); }
void __wrap_restore_alley_buffer(void)  { rec('R'); }
void __wrap_draw_alley_foreground(void) { rec('F'); }
uint16_t __wrap_cga_random(void)        { rec('r'); return rnd_script[rndpos++ % nrnd]; }

static uint16_t now;
static uint16_t tickfn(void) { return now; }
static void fresh(void) {
    nlog = 0; log_[0] = 0; ncoll = collpos = 0; rndpos = 0;
    cupid_active = 0; cupid_anim_tick = 0xffff; now = 0x100;
}
static void step(void) { now++; update_cupid(); }       /* un tick BIOS nuevo por llamada */
static void rnd1(uint16_t v) { rnd_script[0] = v; nrnd = 1; rndpos = 0; }

static const uint8_t T70B0[8] = { 0x00, 0x18, 0x30, 0x48, 0x60, 0x78, 0x90, 0xa8 };

int main(void) {
    game_tick_fn = tickfn;

    /* tablas de DS que usa el spawn */
    CHECK(!memcmp(&ds_pool[0x70b0], T70B0, 8), "dat_70b0");
    for (int i = 0; i < 10; i++) CHECK((ds_pool[0x70b8 + 2*i] | (ds_pool[0x70b9 + 2*i] << 8)) == 0x20 * i, "dat_70b8[%d]", i);

    /* reset_cupid */
    cupid_active = 1; reset_cupid(); CHECK(cupid_active == 0, "reset_cupid");

    /* una actualizacion por tick: el mismo tick no hace nada */
    fresh(); rnd1(0); now = 0x200; cupid_anim_tick = 0x200; update_cupid();
    CHECK(nlog == 0 && cupid_active == 0, "mismo tick: sin llamadas (log '%s')", log_);

    /* spawn: los 32 valores de (dx & 0x1f). Tras el spawn, la misma llamada ya mueve (arrow +4, y +2, x +-5). */
    for (unsigned v = 0; v < 0x20; v++) {
        if (v >= 0x1a) continue;                         /* b > 9: reintenta (probado abajo con una secuencia de valores) */
        fresh(); rnd1((uint16_t)(v | 0xe0));             /* bits altos ignorados por 'and bx,0x1f' */
        step();
        int exp_dir, exp_y, exp_x;
        if (v < 0x10) {
            exp_dir = (v & 8) ? -1 : 1; int x0 = (v & 8) ? 0x120 : 0xc; exp_y = T70B0[v & 7] + 8 + 2; exp_x = x0 + 5 * exp_dir;
        } else {
            unsigned b = v - 0x10; exp_dir = b < 5 ? 1 : -1; exp_y = 6 + 2; exp_x = 0x20 * (int)b + 4 + 5 * exp_dir;
        }
        CHECK(cupid_active == 1, "v=%02x: activo", v);
        CHECK((int8_t)cupid_dir == exp_dir, "v=%02x: dir %d (esperado %d)", v, (int8_t)cupid_dir, exp_dir);
        CHECK(cupid_y == exp_y, "v=%02x: y %02x (esperado %02x)", v, cupid_y, exp_y);
        CHECK(cupid_x == exp_x, "v=%02x: x %04x (esperado %04x)", v, cupid_x, exp_x);
        CHECK(cupid_arrow_x == 4 && cupid_drawn == 1 && cupid_prev_x == 0xffff, "v=%02x: arrow %u, drawn %u, prev %04x", v, cupid_arrow_x, cupid_drawn, cupid_prev_x);
        CHECK(!strcmp(log_, "?r?WED"), "v=%02x: orden de llamadas '%s' (esperado ?r?WED)", v, log_);
        uint8_t sh; CHECK(cupid_draw_addr == (uint16_t)calc_cga_addr(cupid_y, cupid_x, &sh), "v=%02x: draw_addr", v);
    }

    /* b > 9 (v = 0x1a..0x1f): reintenta con otro valor hasta uno valido (0x1d tambien lo es; 0x03 -> borde izquierdo, fila 3) */
    for (unsigned v = 0x1a; v < 0x20; v++) {
        fresh(); rnd_script[0] = (uint16_t)v; rnd_script[1] = 0x1d; rnd_script[2] = 0x03; nrnd = 3; rndpos = 0;
        step();
        CHECK(!strcmp(log_, "?rrr?WED"), "v=%02x: reintentos hasta un valor valido (log '%s')", v, log_);
        CHECK(cupid_active == 1 && (int8_t)cupid_dir == 1 && cupid_x == 0xc + 5 && cupid_y == 0x48 + 8 + 2,
              "v=%02x: queda el 3 (x=%04x y=%02x dir=%d)", v, cupid_x, cupid_y, (int8_t)cupid_dir);
    }

    /* recorrido desde el borde izquierdo (v=0): x=0xc+5n, y=8+2n; sale por la derecha al 58o tick (x >= 0x12c) */
    fresh(); rnd1(0); coll_script[0] = 0; ncoll = 0;
    int ended = 0, ticks = 0, arrow_ok = 1;
    for (ticks = 1; ticks <= 100; ticks++) {
        nlog = 0; log_[0] = 0; step();
        if (!cupid_active) { ended = ticks; break; }
        if (cupid_x != 0xc + 5 * ticks || cupid_y != 8 + 2 * ticks) { CHECK(0, "tick %d: x=%04x y=%02x", ticks, cupid_x, cupid_y); break; }
        unsigned ea = 4u * (unsigned)ticks > 0xa0 ? 0xa0 : 4u * (unsigned)ticks;
        if (cupid_arrow_x != ea) arrow_ok = 0;
        rnd1(0);
    }
    CHECK(ended == 58, "izquierda->derecha: termina en el tick %d (58)", ended);
    CHECK(arrow_ok, "arrow_x sube de 4 en 4 y se queda en 0xa0");
    CHECK(cupid_arrow_x == 0xa0, "arrow_x final 0xa0 (%x)", cupid_arrow_x);
    CHECK(!strcmp(log_, "?E"), "al salir por el borde: solo la colision inicial y erase_cupid, sin toggle ni draw (log '%s')", log_);
    CHECK(cupid_x == 0xc + 5 * 58, "x final %04x", cupid_x);

    /* borde derecho (v=8: x=0x120, dir -1): sale por la izquierda cuando x < 5, tambien al tick 58 */
    fresh(); rnd1(8); ended = 0;
    for (ticks = 1; ticks <= 100; ticks++) { step(); if (!cupid_active) { ended = ticks; break; } rnd1(8); }
    CHECK(ended == 58, "derecha->izquierda: termina en el tick %d (58)", ended);

    /* fila mas baja (v=7: y=0xa8+8=0xb0): sale por abajo (y > 0xbf) al 8o tick */
    fresh(); rnd1(7); ended = 0;
    for (ticks = 1; ticks <= 100; ticks++) { step(); if (!cupid_active) { ended = ticks; break; } }
    CHECK(ended == 8 && cupid_y == 0xc0, "fila baja: sale por abajo en el tick %d, y=%02x (8, c0)", ended, cupid_y);

    /* choque al comienzo del tick: restaura fondo, borra, redibuja el gato y desactiva (no mueve nada) */
    fresh(); rnd1(0); step();                      /* activo */
    nlog = 0; log_[0] = 0; coll_script[0] = 1; ncoll = 1; collpos = 0;
    uint16_t x_before = cupid_x;
    step();
    CHECK(!strcmp(log_, "?REF") && cupid_active == 0 && cupid_x == x_before, "choque inicial: '%s' (?REF), x sin cambio", log_);

    /* choque tras el movimiento: sale por lab_61d4 (erase_cupid, sin toggle ni draw) */
    fresh(); rnd1(0); step();
    nlog = 0; log_[0] = 0; coll_script[0] = 0; coll_script[1] = 1; ncoll = 2; collpos = 0;
    step();
    CHECK(!strcmp(log_, "??E") && cupid_active == 0, "choque tras mover: '%s' (??E)", log_);

    /* con cupid activo no consume numeros aleatorios y el orden normal es ? W E D */
    fresh(); rnd1(0); step();
    nlog = 0; log_[0] = 0; step();
    CHECK(!strcmp(log_, "??WED"), "activo: sin random, orden '%s' (??WED)", log_);

    if (!fails) printf("test_cupid: OK\n");
    return fails;
}
