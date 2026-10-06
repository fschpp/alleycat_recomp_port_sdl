/* T44 — set_palette / set_ega_palette / INT 10h AH=0Bh (enemy.asm L242-274, entry.asm L49-62). Sin SDL.
 * Valores esperados escritos a mano desde cat.asm / data/result_sprites.asm. Ver PROGRESS.md §6ao. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "cga.h"
#include "cat_state.h"
#include "palette.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* cga_palette_table (data/result_sprites.asm) y pcjr_palette_1/2/3 (cat.asm L603-608), 8 entradas */
static const uint8_t CGA_TBL[8] = { 1, 1, 1, 1, 0, 0, 1, 0 };
static const uint8_t PCJR1[8]   = { 10, 9, 11, 11, 14, 9, 6, 10 };
static const uint8_t PCJR2[8]   = { 9, 14, 4, 6, 12, 10, 13, 4 };
static const uint8_t PCJR3[8]   = { 15, 15, 15, 15, 15, 15, 15, 15 };

int main(void) {
    /* ---- INT 10h AH=0Bh ---- */
    cga_color_select = 0x30;
    bios_color_select(0x1, 0x0);  CHECK(cga_color_select == 0x10, "BH=1,BL=0 quita solo el bit de paleta (0x%02x)", cga_color_select);
    bios_color_select(0x1, 0x1);  CHECK(cga_color_select == 0x30, "BH=1,BL=1 pone el bit de paleta");
    bios_color_select(0x1, 0x7);  CHECK(cga_color_select == 0x30, "BH=1: cualquier BL != 0 es paleta 1");
    bios_color_select(0x0, 0xf);  CHECK(cga_color_select == 0x2f, "BH=0,BL=0xf: fondo blanco, intensidad borrada (0x%02x)", cga_color_select);
    bios_color_select(0x0, 0x00); CHECK(cga_color_select == 0x20, "BH=0,BL=0: fondo negro y SIN intensidad (0x%02x)", cga_color_select);
    bios_color_select(0x2, 0x5);  CHECK(cga_color_select == 0x20, "BH desconocido: sin efecto");

    /* ---- set_palette CGA por nivel: paleta de la tabla, fondo negro, intensidad baja ---- */
    rom_id = 0xff;
    for (int n = 0; n < 8; n++) {
        cga_color_select = 0x3f;           /* sucio: intensidad y fondo */
        level_number = (int16_t)n;
        set_palette();
        uint8_t exp = (uint8_t)(CGA_TBL[n] ? 0x20 : 0x00);
        CHECK(cga_color_select == exp, "nivel %d: puerto 0x3D9 = 0x%02x, esperado 0x%02x", n, cga_color_select, exp);
    }
    /* colores resultantes: paleta 1 baja = negro/cian/magenta/gris; paleta 0 baja = negro/verde/rojo/marron */
    cga_color_select = 0x20;
    CHECK(palette_rgb(0) == 0xFF000000u && palette_rgb(1) == 0xFF00AAAAu && palette_rgb(2) == 0xFFAA00AAu && palette_rgb(3) == 0xFFAAAAAAu, "paleta 1 baja");
    cga_color_select = 0x00;
    CHECK(palette_rgb(1) == 0xFF00AA00u && palette_rgb(2) == 0xFFAA0000u && palette_rgb(3) == 0xFFAA5500u, "paleta 0 baja");
    cga_color_select = 0x30;
    CHECK(palette_rgb(1) == 0xFF55FFFFu && palette_rgb(3) == 0xFFFFFFFFu, "paleta 1 alta");
    cga_color_select = 0x1f;
    CHECK(palette_rgb(0) == 0xFFFFFFFFu, "fondo 0xf = blanco brillante");

    /* ---- set_palette PCjr (rom_id 0xfd): registros 1,2,3 por nivel; el 0 no se toca ---- */
    rom_id = 0xfd;
    for (int n = 0; n < 8; n++) {
        for (int r = 0; r < 16; r++) ega_palette_reg[r] = (uint8_t)r;
        cga_color_select = 0x3f;
        level_number = (int16_t)n;
        set_palette();
        CHECK(ega_palette_reg[1] == PCJR1[n] && ega_palette_reg[2] == PCJR2[n] && ega_palette_reg[3] == PCJR3[n],
              "PCjr nivel %d: regs %u,%u,%u", n, ega_palette_reg[1], ega_palette_reg[2], ega_palette_reg[3]);
        CHECK(ega_palette_reg[0] == 0 && ega_palette_reg[4] == 4, "PCjr nivel %d: no toca otros registros", n);
        CHECK(cga_color_select == 0x20, "PCjr nivel %d: tambien hace BH=0,BL=0 (0x%02x)", n, cga_color_select);
        CHECK(palette_rgb(1) == palette_rgb(1) && palette_rgb(3) == 0xFFFFFFFFu, "PCjr: el indice 3 es el registro 3 (blanco)");
    }
    set_ega_palette(0x13, 0x1b);   /* mascaras: registro & 0xf, valor & 0xf */
    CHECK(ega_palette_reg[3] == 0xb, "set_ega_palette enmascara a 4 bits");
    rom_id = 0xff;

    if (fails == 0) printf("test_palette: OK\n");
    return fails ? 1 : 0;
}
