/* T55 — show_pause_menu (ui.asm L245-299). Sin SDL. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "score.h"
#include "ui.h"
#include "input.h"
#include "bios_text.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint16_t now;
static uint16_t tickfn(void) { return now; }
static int iter, press_at;
static uint8_t during[sizeof cga_mem]; static int captured;
static void hook(void) {
    iter++; now = (uint16_t)(now + 10);
    if (iter == press_at) { memcpy(during, cga_mem, sizeof during); captured = 1; keyboard_counter++; }
}
static uint8_t joyfn(void) { return iter >= press_at ? 0xef : 0xff; }
static void pattern(void) { for (size_t i = 0; i < sizeof cga_mem; i++) cga_mem[i] = (uint8_t)(i * 7 + (i >> 8) + 1); }

int main(void) {
    game_tick_fn = tickfn; ui_wait_hook = hook;
    static uint8_t before[sizeof cga_mem], ref[sizeof cga_mem];

    /* cadenas del ASM en DS */
    CHECK(strstr((const char *)&ds_pool[0x6d91], "Paws Game") != NULL, "str_paws_game");
    CHECK(strstr((const char *)&ds_pool[0x6db2], "any key") != NULL, "str_key_continue");
    CHECK(strstr((const char *)&ds_pool[0x6dd3], "button") != NULL, "str_button_continue");

    for (int joy = 0; joy < 2; joy++) {
        pattern(); memcpy(before, cga_mem, sizeof before);
        iter = 0; captured = 0; press_at = 6; now = 0x1000; set_bios_tick(now); keyboard_counter = 0x0300; pause_counter = 0; use_joystick = (uint8_t)joy;
        joy_port_fn = joy ? joyfn : NULL;
        show_pause_menu();
        CHECK(captured && iter == 6, "joy=%d: espera hasta la pulsacion (vueltas %d)", joy, iter);
        /* durante la pausa: el cartel esta dibujado y solo cambia la region 0xdca (0x20 words x 0x10 filas) */
        CHECK(memcmp(during, before, sizeof before) != 0, "joy=%d: el cartel se dibuja", joy);
        /* referencia independiente: mismos textos con la API de texto sobre una copia */
        memcpy(cga_mem, before, sizeof before);
        bios_set_cursor(0xb, 5); print_string(&ds_pool[0x6d91]);
        bios_set_cursor(0xc, 5); print_string(&ds_pool[joy ? 0x6dd3 : 0x6db2]);
        memcpy(ref, cga_mem, sizeof ref);
        CHECK(memcmp(during, ref, sizeof ref) == 0, "joy=%d: texto = 'Paws Game' fila 0xb / indicacion fila 0xc, col 5", joy);
        /* tras reanudar: pantalla identica */
        memcpy(cga_mem, before, sizeof before);                 /* (se repite la pausa para comparar el resultado final) */
        iter = 0; captured = 0; now = 0x1000; set_bios_tick(now); keyboard_counter = 0x0300;
        show_pause_menu();
        CHECK(memcmp(cga_mem, before, sizeof before) == 0, "joy=%d: pausa -> reanudar deja cga_mem identico", joy);
        CHECK(pause_counter == keyboard_counter && pause_counter == 0x0301, "joy=%d: pause_counter = keyboard_counter (%04x)", joy, pause_counter);
        /* el tick vuelve al valor de la entrada (0x1000) y sigue avanzando */
        CHECK(title_saved_cx == 0x1000, "joy=%d: tick guardado %04x", joy, title_saved_cx);
        CHECK(score_tick() == 0x1000 + 0, "joy=%d: tick restaurado (%04x, esperado 1000)", joy, score_tick());
        now = (uint16_t)(now + 7); CHECK(score_tick() == 0x1007, "joy=%d: el tick sigue avanzando tras restaurar (%04x)", joy, score_tick());
    }
    /* sin offset residual para el resto: se deja el reloj como estaba */
    set_bios_tick(now);
    CHECK(score_tick() == now, "set_bios_tick(now) deja score_tick == reloj");

    if (!fails) printf("test_pause: OK\n");
    return fails;
}
