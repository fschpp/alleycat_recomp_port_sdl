/* T50 — print_string, set_cursor, wait_for_input, display_text_line, clear_cga (ui.asm). Sin SDL. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "bios_text.h"
#include "font8x8.h"
#include "ui.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static unsigned pix(int x, int y) {
    size_t off = (size_t)(y & 1) * 0x2000 + (size_t)(y >> 1) * 80 + (size_t)(x >> 2);
    return (cga_mem[off] >> (6 - 2 * (x & 3))) & 3;
}
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
static int text_at(int row, int col, const char *s, int color) {
    for (int i = 0; s[i]; i++) if (!cell_is(row, col + i, (unsigned char)s[i], color)) return 0;
    return 1;
}

static int hook_calls, press_after, joy_after;
static void key_hook(void) { if (++hook_calls == press_after) keyboard_counter++; }
static uint8_t joy_val(void) { return (++joy_after >= 4) ? 0xef : 0xff; }   /* boton (bit 4 = 0) a la 4a lectura */

int main(void) {
    /* ---- print_string: color 2, cursor avanza, devuelve puntero tras el 0 */
    memset(cga_mem, 0x55, sizeof cga_mem);
    const uint8_t s[] = "ALLEY CAT\0NEXT";
    bios_set_cursor(3, 5);
    const uint8_t *r = print_string(s);
    CHECK(r == s + 10, "print_string devuelve SI tras el terminador (%d)", (int)(r - s));
    CHECK(text_at(3, 5, "ALLEY CAT", 2), "texto 'ALLEY CAT' color 2");
    CHECK(cell_untouched(3, 4) && cell_untouched(3, 14) && cell_untouched(2, 5) && cell_untouched(4, 5), "vecinos intactos");
    CHECK(bios_cursor_row() == 3 && bios_cursor_col() == 14, "cursor final (%d,%d)", bios_cursor_row(), bios_cursor_col());
    memset(cga_mem, 0x55, sizeof cga_mem);
    const uint8_t e[] = "\0X";
    CHECK(print_string(e) == e + 1, "cadena vacia"); CHECK(cell_untouched(3, 14), "cadena vacia no dibuja");

    /* ---- set_cursor: (dh, 0) */
    bios_set_cursor(9, 9); set_cursor(7);
    CHECK(bios_cursor_row() == 7 && bios_cursor_col() == 0, "set_cursor (%d,%d)", bios_cursor_row(), bios_cursor_col());

    /* ---- wait_for_input por teclado: no vuelve hasta que cambia keyboard_counter */
    use_joystick = 0; keyboard_counter = 0xffff; hook_calls = 0; press_after = 5; ui_wait_hook = key_hook;
    wait_for_input();
    CHECK(hook_calls == 5 && keyboard_counter == 0, "teclado: vueltas %d, contador %u (wrap 16 bits)", hook_calls, keyboard_counter);
    /* ---- por joystick: espera boton (bit 4 a 0); ignora keyboard_counter */
    use_joystick = 1; hook_calls = 0; press_after = 0; joy_after = 0; joy_port_fn = joy_val;
    wait_for_input();
    CHECK(joy_after == 4 && hook_calls == 4, "joystick: lecturas %d, vueltas %d", joy_after, hook_calls);
    ui_wait_hook = NULL; joy_port_fn = NULL; use_joystick = 0;

    /* ---- display_text_line sobre las tablas reales: cursor = byte alto de la tabla, cadena de la tabla de punteros */
    uint16_t ptr0 = (uint16_t)(ds_pool[0x6d37] | (ds_pool[0x6d38] << 8));
    uint16_t cur0 = (uint16_t)(ds_pool[0x6d63] | (ds_pool[0x6d64] << 8));
    CHECK(strncmp((const char *)&ds_pool[ptr0], "Do you want to use a joystick", 29) == 0, "tabla de cadenas en 0x6d37");
    int row0 = cur0 >> 8;
    for (int line = 0; line < 22; line++) {
        uint16_t off = (uint16_t)(2 * line);
        int row = ds_pool[0x6d63 + off + 1];
        const char *str = (const char *)&ds_pool[ds_pool[0x6d37 + off] | (ds_pool[0x6d38 + off] << 8)];
        memset(cga_mem, 0x55, sizeof cga_mem);
        title_joy_offset = off;
        display_text_line();
        CHECK(title_joy_offset == off + 2, "linea %d: offset avanza 2 (%u)", line, title_joy_offset);
        CHECK(text_at(row, 0, str, 2), "linea %d: '%s' en la fila %d", line, str, row);
        int len = (int)strlen(str);   /* una linea de 40 columnas exactas (n.10) deja el cursor en (fila+1, 0), como la BIOS */
        CHECK(bios_cursor_row() == row + len / 40 && bios_cursor_col() == len % 40, "linea %d cursor (%d,%d)", line, bios_cursor_row(), bios_cursor_col());
    }
    (void)row0;

    /* ---- clear_cga: borra los 2 bancos (8000 bytes cada uno) y respeta el hueco entre bancos */
    memset(cga_mem, 0xa5, sizeof cga_mem);
    clear_cga();
    int ok = 1;
    for (int i = 0; i < 8000; i++) ok &= cga_mem[i] == 0 && cga_mem[0x2000 + i] == 0;
    CHECK(ok, "clear_cga pone a 0 ambos bancos");
    CHECK(cga_mem[8000] == 0xa5 && cga_mem[0x2000 + 8000] == 0xa5, "clear_cga no pasa de 8000 bytes por banco");

    if (fails) { printf("test_ui_text: %d FALLOS\n", fails); return 1; }
    printf("test_ui_text: OK\n");
    return 0;
}
