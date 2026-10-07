/* T60 — poll_joystick / decode_joystick_axis (input.asm L4-89). Sin SDL: read_pit_timer se envuelve con un reloj virtual. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cat_state.h"
#include "game_flow.h"
#include "score.h"
#include "ui.h"
#include "input.h"
#include "joystick.h"
#include "speaker.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* reloj virtual del PIT: cada lectura avanza `step` cuentas; el contador cuenta hacia atras (0 - vt) */
static uint32_t vt, step, fire_vt; static int pit_reads, keyb_calls;
uint16_t __wrap_read_pit_timer(void) { pit_reads++; vt += step; return (uint16_t)(0u - vt); }
void input_poll(void) { keyb_calls++; }                    /* gana a la definicion debil de flow_stubs.c */

/* puerto de juegos simulado: ejes en 1 hasta ex/ey cuentas despues del disparo; boton pulsado = bit 4 a 0 */
static uint32_t ex, ey; static int button_down, fired;
static uint8_t port(void) {
    uint8_t al = 0xe0; uint32_t e = vt - fire_vt;
    if (!fired || e < ex) al |= 1;
    if (!fired || e < ey) al |= 2;
    if (!button_down) al |= 0x10;
    return al;
}
static void fire(void) { fired = 1; fire_vt = vt; }

static uint16_t now;
static uint16_t tickfn(void) { return now; }

static void setup(void) {
    vt = 0; step = 20; pit_reads = 0; keyb_calls = 0; fired = 0; button_down = 0;
    input_horizontal = 0x55; input_vertical = 0x55; joy_pending = 0; joy_button = 0xee;
    joy_port_fn = port; joy_fire_fn = fire; use_joystick = 1;
    now = 100; joy_last_tick = 0;
}

static int8_t exp_dir(uint32_t e) { return e <= 0x506 ? -1 : e <= 0xa1a ? 0 : 1; }

int main(void) {
    game_tick_fn = tickfn; set_bios_tick(0);

    /* decode_joystick_axis: umbrales exactos (joy_timer = 0, vt = tiempo transcurrido, step = 0 para fijar la lectura) */
    {
        uint32_t es[] = { 0, 1, 0x505, 0x506, 0x507, 0xa19, 0xa1a, 0xa1b, 0x2000 };
        int8_t   ex_[] = { 1 /* 0 cuentas: bx=0 < 0xf5e6 (aritmetica modulo 2^16, caso degenerado del original) */, -1, -1, -1, 0, 0, 0, 1, 1 };
        for (unsigned i = 0; i < sizeof es / sizeof *es; i++) {
            joy_timer = 0; step = 0; vt = es[i];
            CHECK((int8_t)decode_joystick_axis() == ex_[i], "decode: %#x cuentas -> %d (esperado %d)", es[i], (int8_t)decode_joystick_axis(), ex_[i]);
        }
    }

    /* limite de 2 ticks: con diferencia < 2 no hace nada, ni lee el PIT */
    setup(); joy_last_tick = 99; now = 100;
    poll_joystick();
    CHECK(pit_reads == 0 && keyb_calls == 0 && !fired && input_horizontal == 0x55, "1 tick desde el ultimo sondeo: no hace nada");
    now = 101; poll_joystick();
    CHECK(pit_reads > 0 && joy_last_tick == 101, "2 ticks: sondea (joy_last_tick=%u)", joy_last_tick);
    /* con wrap de 16 bits */
    setup(); joy_last_tick = 0xffff; now = 0x0001; poll_joystick();
    CHECK(joy_last_tick == 0x0001, "diferencia sin signo con wrap (0xffff -> 1)");

    /* sin joystick: teclado + espera del PIT */
    setup(); use_joystick = 0; step = 7; poll_joystick();
    CHECK(keyb_calls == 1 && !fired && input_horizontal == 0x55 && joy_button == 0xee, "use_joystick=0: solo input_poll una vez");
    CHECK(pit_reads == 2, "use_joystick=0: sale en cuanto cambia el PIT (%d lecturas)", pit_reads);

    /* con joystick: todas las combinaciones de respuesta de ejes (lejos de los umbrales) y del boton */
    uint32_t times[] = { 100, 500, 1500, 2300, 3500, 20000 };
    for (int b = 0; b < 2; b++)
    for (unsigned i = 0; i < 6; i++) for (unsigned j = 0; j < 6; j++) {
        setup(); button_down = b; ex = times[i]; ey = times[j];
        poll_joystick();
        CHECK(fired && joy_button == (b ? 0x00 : 0x10), "boton b=%d: joy_button=%02x", b, joy_button);
        /* el tiempo medido incluye la lectura de decode (step): se compara con margen en ex + step */
        int8_t hx = exp_dir(ex + step), hy = exp_dir(ey + step);
        CHECK(input_horizontal == hx, "ex=%u: horizontal %d (esperado %d)", ex, input_horizontal, hx);
        CHECK(input_vertical == hy, "ey=%u: vertical %d (esperado %d)", ey, input_vertical, hy);
        CHECK(joy_pending == 0, "ex=%u ey=%u: joy_pending=%d", ex, ey, joy_pending);
    }

    /* eje que no responde: 2000 vueltas y queda en 0xff; el otro se decodifica */
    setup(); ex = 1500; ey = 0xfffffff; step = 20; poll_joystick();
    CHECK(input_horizontal == 0 && input_vertical == -1 && joy_pending == 2, "Y mudo: X=0, Y=-1 (pending=%d)", joy_pending);
    CHECK(pit_reads == 1 + 2000 + 1, "Y mudo: 1 + 2000 vueltas + 1 decode (%d lecturas)", pit_reads);
    setup(); ex = 0xfffffff; ey = 600; poll_joystick();
    CHECK(input_horizontal == -1 && input_vertical == -1 && joy_pending == 1, "X mudo (ey=600): Y=-1, X=-1 (%d,%d)", input_horizontal, input_vertical);
    setup(); ex = ey = 0xfffffff; poll_joystick();
    CHECK(input_horizontal == -1 && input_vertical == -1 && joy_pending == 3 && pit_reads == 1 + 2000, "ningun eje responde: (-1,-1), 2000 vueltas (%d)", pit_reads);

    /* loopnz: sale antes si ax == 0x1964 exacto (tiempo transcurrido 0xe69c = 59036 = 2 * 29518) */
    setup(); ex = ey = 0xfffffff; step = 29518; poll_joystick();
    CHECK(pit_reads == 1 + 2 && input_horizontal == -1 && input_vertical == -1, "ax == 0x1964 corta el loopnz (%d lecturas)", pit_reads);

    /* sin hook de puerto: 0xff (sin joystick, nunca responde) */
    setup(); joy_port_fn = NULL; joy_fire_fn = NULL; poll_joystick();
    CHECK(joy_button == 0x10 && input_horizontal == -1 && input_vertical == -1, "sin puerto: boton suelto, ejes -1");

    if (!fails) printf("test_joystick: OK\n");
    return fails;
}
