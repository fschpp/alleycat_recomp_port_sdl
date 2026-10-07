/* T78 — borde de color en play_explosion_effect (sound.asm L285-325). Sin SDL.
 * ASM: `mov ah,0xb / mov bx,4 / int 0x10` al entrar y `mov ah,0xb / sub bx,bx / int 0x10` al salir.
 * Esperados a mano: BH=0 -> bits 0-4 del registro 0x3D9 = BL (borra la intensidad); rojo CGA (RGBI 4) = 0xAA0000. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "cga.h"
#include "cat_state.h"
#include "palette.h"
#include "sound.h"
#include "game_flow.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static int hook_calls; static uint8_t hook_reg; static uint32_t hook_border, hook_idx0;
static void hook(void) { hook_calls++; hook_reg = cga_color_select; hook_border = palette_border_rgb(); hook_idx0 = palette_rgb(0); }

static void run(uint8_t start_reg, uint8_t exp_during, uint8_t exp_after, const char *tag) {
    cga_color_select = start_reg; hook_calls = 0;
    play_explosion_effect();
    CHECK(hook_calls == 1, "%s: presenta una vez durante la explosion (%d)", tag, hook_calls);
    CHECK(hook_reg == exp_during, "%s: durante: reg 0x%02x, esperado 0x%02x", tag, hook_reg, exp_during);
    CHECK(hook_border == 0xFFAA0000u, "%s: durante: borde 0x%08x, esperado rojo", tag, hook_border);
    CHECK(hook_idx0 == 0xFFAA0000u, "%s: durante: indice 0 (fondo) 0x%08x, esperado rojo", tag, hook_idx0);
    CHECK(cga_color_select == exp_after, "%s: despues: reg 0x%02x, esperado 0x%02x", tag, cga_color_select, exp_after);
    CHECK(palette_border_rgb() == 0xFF000000u, "%s: despues: borde negro", tag);
}

int main(void) {
    rom_id = 0xff; sound_enabled = 0;
    game_present_hook = hook;
    run(0x20, 0x24, 0x20, "paleta 1, intensidad baja");   /* estado normal tras set_palette */
    run(0x30, 0x24, 0x20, "intensidad alta sucia");        /* BH=0 borra el bit 4 y el BX=0 final lo deja en 0 */
    run(0x00, 0x04, 0x00, "paleta 0");                     /* el bit 5 (paleta) no se toca */
    /* con sonido: mismo borde (la rama del altavoz no debe cambiar el registro de color) */
    sound_enabled = 1;
    run(0x20, 0x24, 0x20, "con sonido");
    /* sin hook no revienta */
    game_present_hook = NULL; sound_enabled = 0; cga_color_select = 0x20;
    play_explosion_effect();
    CHECK(cga_color_select == 0x20, "sin hook: queda en negro (0x%02x)", cga_color_select);

    if (fails) { printf("test_explosion_border: %d FALLOS\n", fails); return 1; }
    printf("test_explosion_border: OK\n");
    return 0;
}
