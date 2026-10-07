/* palette.c — enemy.asm set_palette / set_ega_palette (T44). Ver include/palette.h y PROGRESS.md §6ao.
 * Sin SDL: video.c solo consulta palette_rgb(). */
#include "palette.h"
#include "cat_state.h"
#include "gen/ds_pool.h"

uint8_t cga_color_select = 0x30;   /* valor tras el modo 4 de la BIOS (IBM): paleta 1, intensidad alta; el
                                    * original lo deja en 0x20 en cuanto pasa por set_palette (T40 lo llama) */
uint8_t ega_palette_reg[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf };

/* Tablas verificadas contra /tmp/data_segment.bin (8 bytes cada una, indexadas por level_number SIN escalar:
 * `mov si,[level_number]` y `byte [si+tabla]`). */
#define DS_PCJR_PALETTE_1   0x183b
#define DS_PCJR_PALETTE_2   0x1843
#define DS_PCJR_PALETTE_3   0x184b
#define DS_CGA_PALETTE_TBL  0x1853

void bios_color_select(uint8_t bh, uint8_t bl) {
    if (bh == 0x0) cga_color_select = (uint8_t)((cga_color_select & 0xe0) | (bl & 0x1f));
    else if (bh == 0x1) cga_color_select = (uint8_t)((cga_color_select & 0xdf) | (bl ? 0x20 : 0x00));
}

void set_ega_palette(uint8_t bl, uint8_t bh) {
    ega_palette_reg[bl & 0xf] = (uint8_t)(bh & 0xf);
}

void set_palette(void) {
    uint16_t si;
    if (rom_id == 0xfd) goto lab_1d48;
    si = (uint16_t)level_number;
    bios_color_select(0x1, ds_pool[DS_CGA_PALETTE_TBL + si]);   /* mov ah,0xb / bh=1 / bl=tabla[level] */
    goto lab_1d67;
lab_1d48:
    si = (uint16_t)level_number;
    set_ega_palette(0x1, ds_pool[DS_PCJR_PALETTE_1 + si]);
    set_ega_palette(0x2, ds_pool[DS_PCJR_PALETTE_2 + si]);
    set_ega_palette(0x3, ds_pool[DS_PCJR_PALETTE_3 + si]);
lab_1d67:
    bios_color_select(0x0, 0x0);                                /* sub bx,bx / mov ah,0xb: fondo y borde negros */
}

/* RGBI -> RGB (CGA estandar, el marron es 0xAA5500). */
static uint32_t rgbi(uint8_t c) {
    static const uint32_t t[16] = {
        0xFF000000u, 0xFF0000AAu, 0xFF00AA00u, 0xFF00AAAAu, 0xFFAA0000u, 0xFFAA00AAu, 0xFFAA5500u, 0xFFAAAAAAu,
        0xFF555555u, 0xFF5555FFu, 0xFF55FF55u, 0xFF55FFFFu, 0xFFFF5555u, 0xFFFF55FFu, 0xFFFFFF55u, 0xFFFFFFFFu };
    return t[c & 0xf];
}

uint32_t palette_rgb(unsigned idx) {
    idx &= 0x3;
    if (rom_id == 0xfd) return rgbi(ega_palette_reg[idx]);       /* PCjr: indice -> registro de paleta */
    if (idx == 0) return rgbi((uint8_t)(cga_color_select & 0xf));
    {
        uint8_t base = (cga_color_select & 0x20) ? 3 : 2;          /* paleta 1: cian(3)/magenta(5)/blanco(7); 0: 2/4/6 */
        uint8_t c = (uint8_t)(base + 2 * (idx - 1));
        if (cga_color_select & 0x10) c |= 0x8;
        return rgbi(c);
    }
}

/* Color del borde (overscan), T78: BL & 0xf del ultimo INT 10h AH=0Bh BH=0, o sea cga_color_select & 0xf. En CGA es
 * ademas el color del indice 0 (palette_rgb(0)); en PCjr la BIOS lo escribe en el registro de borde, que NO es el
 * registro de paleta 0, asi que alli el borde cambia pero los pixeles del indice 0 no. */
uint32_t palette_border_rgb(void) {
    return rgbi((uint8_t)(cga_color_select & 0xf));
}
