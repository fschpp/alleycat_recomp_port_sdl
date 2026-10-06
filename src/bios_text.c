/* bios_text.c — texto de la BIOS en modo 4 (ver include/bios_text.h). Usa la fuente de src/font8x8.c. */
#include "bios_text.h"
#include "cga.h"
#include "font8x8.h"

static uint8_t cur_row, cur_col;

void bios_set_cursor(uint8_t row, uint8_t col) { cur_row = row; cur_col = col; }
uint8_t bios_cursor_row(void) { return cur_row; }
uint8_t bios_cursor_col(void) { return cur_col; }

void bios_teletype(uint8_t ch, uint8_t color) {
    const uint8_t *g = font8x8[ch & 0x7f];
    color &= 3;
    if (cur_row < 25 && cur_col < 40) {
        for (int r = 0; r < 8; r++) {
            unsigned y = (unsigned)cur_row * 8 + (unsigned)r;
            size_t off = (size_t)(y & 1) * CGA_BANK_SIZE + (size_t)(y >> 1) * CGA_BYTES_PER_ROW + (size_t)cur_col * 2;
            uint16_t w = 0;                                   /* pixel 0 = bits 15..14 (el byte bajo de la direccion es el izquierdo) */
            for (int p = 0; p < 8; p++)
                if (g[r] & (1u << p)) w |= (uint16_t)(color << (14 - 2 * p));
            cga_mem[off]     = (uint8_t)(w >> 8);
            cga_mem[off + 1] = (uint8_t)(w & 0xff);
        }
    }
    cur_col++;
    if (cur_col >= 40) { cur_col = 0; cur_row++; }            /* sin scroll */
}
