/* T48 — mask_score_tiles, print_bonus_score, print_level7_bonus, flash_score_color (level_objects.asm L1244-1316).
 * Sin SDL. El texto de la BIOS es un modelo (include/bios_text.h): se comprueba contra la fuente, no contra la ROM. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "palette.h"
#include "score.h"
#include "bios_text.h"
#include "font8x8.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static unsigned pix(int x, int y) {
    size_t off = (size_t)(y & 1) * 0x2000 + (size_t)(y >> 1) * 80 + (size_t)(x >> 2);
    return (cga_mem[off] >> (6 - 2 * (x & 3))) & 3;
}
/* la celda (row,col) contiene el glifo `ch` en color `color` (fondo 0) */
static int cell_is(int row, int col, int ch, int color) {
    for (int r = 0; r < 8; r++) for (int p = 0; p < 8; p++) {
        unsigned want = (font8x8[ch][r] >> p) & 1 ? (unsigned)color : 0u;
        if (pix(col * 8 + p, row * 8 + r) != want) return 0;
    }
    return 1;
}
static int cell_untouched(int row, int col) {
    for (int r = 0; r < 8; r++) for (int p = 0; p < 8; p++) if (pix(col * 8 + p, row * 8 + r) != 1) return 0;
    return 1;
}
static uint16_t fake_tick; static uint16_t tickfn(void) { return fake_tick; }

/* dat_35e0 (60 bytes), extraido del ASM con tools/resolve_data_segment.py */
static const char *SRC_HEX = "0f000000f00ff0ff0ff00ffffffff00ffffffff03f003f003cffcaffcafffffffffffffffff0ffff3ff0fff0fc0fff000ff000ffffff000000ff0000";

int main(void) {
    uint8_t src[60];
    for (int i = 0; i < 60; i++) { unsigned v; sscanf(SRC_HEX + 2 * i, "%2x", &v); src[i] = (uint8_t)v; }

    /* ---- mask_score_tiles: and por word, little endian */
    uint16_t masks[] = { 0xffff, 0xaaaa, 0x5555, 0x0000, 0x0ff0 };
    for (unsigned m = 0; m < sizeof masks / sizeof masks[0]; m++) {
        memset(score_tiles, 0xee, sizeof score_tiles);
        mask_score_tiles(masks[m]);
        int ok = 1;
        for (int i = 0; i < 30; i++) {
            uint16_t w = (uint16_t)((src[2 * i] | (src[2 * i + 1] << 8)) & masks[m]);
            ok &= score_tiles[2 * i] == (w & 0xff) && score_tiles[2 * i + 1] == (w >> 8);
        }
        CHECK(ok, "mask_score_tiles(0x%04x)", masks[m]);
    }

    /* ---- print_bonus_score: 4 digitos (bonus_bcd[3..6]) en la fila bonus_row>>3, columna 0x12, color 3 */
    memset(cga_mem, 0x55, sizeof cga_mem);
    uint8_t d[8] = { 9, 9, 9, 1, 2, 3, 4, 0 };
    memcpy(bonus_bcd, d, 8); bonus_row = 0x50;
    print_bonus_score();
    for (int i = 0; i < 4; i++) CHECK(cell_is(10, 0x12 + i, '1' + i, 3), "digito %d", i);
    CHECK(cell_untouched(10, 0x11) && cell_untouched(10, 0x16) && cell_untouched(9, 0x12) && cell_untouched(11, 0x12), "vecinos intactos");
    CHECK(bios_cursor_row() == 10 && bios_cursor_col() == 0x16, "cursor final (%d,%d)", bios_cursor_row(), bios_cursor_col());
    /* bonus_row no multiplo de 8: 0x3a >> 3 = 7 */
    memset(cga_mem, 0x55, sizeof cga_mem); bonus_row = 0x3a;
    print_bonus_score();
    CHECK(cell_is(7, 0x12, '1', 3), "fila 0x3a -> celda 7");

    /* ---- print_level7_bonus: "BONUS MULTIPLIER: " + 2 caracteres de dat_36ec[bonus_l7_index] en (10,10) */
    struct { uint16_t idx; const char *tail; } cases[] = { { 0, "01" }, { 4, "05" }, { 0x12, "06" }, { 0x1e, "30" } };
    for (unsigned c = 0; c < 4; c++) {
        memset(cga_mem, 0x55, sizeof cga_mem);
        bonus_l7_index = cases[c].idx;
        print_level7_bonus();
        char want[21]; snprintf(want, sizeof want, "BONUS MULTIPLIER: %s", cases[c].tail);
        int ok = 1;
        for (int i = 0; i < 20; i++) ok &= cell_is(10, 10 + i, (unsigned char)want[i], 3);
        CHECK(ok, "texto nivel 7 idx %u = \"%s\"", cases[c].idx, want);
        CHECK(cell_untouched(10, 9) && cell_untouched(10, 30) && cell_untouched(9, 10) && cell_untouched(11, 10), "n7 vecinos intactos (idx %u)", cases[c].idx);
    }

    /* ---- flash_score_color: devuelve el tick; borde 0 si tick&4, si no bonus_color */
    game_tick_fn = tickfn;
    for (int bc = 1; bc <= 2; bc++) {
        bonus_color = (uint8_t)bc;
        uint16_t ticks[] = { 0, 3, 4, 7, 8, 12, 0xfffc, 0xffff };
        for (unsigned i = 0; i < sizeof ticks / sizeof ticks[0]; i++) {
            fake_tick = ticks[i];
            cga_color_select = 0x2f;
            uint16_t r = flash_score_color();
            unsigned want = (ticks[i] & 4) ? 0u : (unsigned)bc;
            CHECK(r == ticks[i], "flash devuelve tick %u (%u)", ticks[i], r);
            CHECK((cga_color_select & 0x1f) == want, "flash tick %u color %u: borde %u (esperado %u)", ticks[i], bc, cga_color_select & 0x1f, want);
            CHECK((cga_color_select & 0x20) == 0x20, "flash conserva paleta (bit 5)");
        }
    }

    if (fails) { printf("test_score_bar_a: %d FALLOS\n", fails); return 1; }
    printf("test_score_bar_a: OK\n");
    return 0;
}
