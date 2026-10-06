/* score_bar.c — helpers de la barra de bonus de level_objects.asm (T48: L1244-1316; T49 se anade aqui).
 * En un .c aparte de score.c para que handle_level_complete los llame a traves de una frontera de unidad y los tests
 * puedan envolverlos con --wrap (igual que result.c / transition.c). PROGRESS.md §6as. */
#include <stdint.h>
#include "cat_state.h"
#include "cga.h"
#include "score.h"
#include "palette.h"
#include "bios_text.h"
#include "gen/ds_pool.h"

static uint16_t score_ds_word(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

/* T48 (level_objects.asm L1244-1316) */
uint8_t score_tiles[60];   /* DS 0x000e (30 words) */

#define DS_SCORE_TILES_SRC 0x35e0   /* dat_35e0: 30 words de azulejos */
#define DS_L7_TEXT         0x370e   /* dat_370e: "BONUS MULTIPLIER: 12" (20 bytes; los 2 ultimos se pisan con dat_36ec[]) */
#define DS_L7_MULT_TABLE   0x36ec   /* dat_36ec: 16 words ASCII "01","03",... */

/* flash_score_color (L1244-1261): lee el tick, espera retrace (check_vsync = no-op en el port), y pone el color de
 * fondo/borde: 0 si el bit 2 del tick esta activo, si no `bonus_color` (BH=0). Devuelve dx = tick leido al entrar. */
uint16_t flash_score_color(void) {
    uint16_t dx = score_tick();                            /* sub ah,ah / int 0x1a / push dx */
    uint8_t bl = 0x0;                                      /* sub bx,bx */
    if (dx & 0x4) goto lab_3a34;                           /* test dx,4 / jnz */
    bl = bonus_color;                                      /* mov bl,[dat_3699] */
lab_3a34:
    bios_color_select(0x0, bl);                            /* mov ah,0xb / int 0x10 */
    return dx;                                             /* pop dx */
}

/* print_bonus_score (L1262-1281): cursor en (fila = dat_369e >> 3, col 0x12) y 4 digitos del bonus (bonus_bcd[3..6],
 * ASCII, color 3) con el teletype de la BIOS. dat_36a0 es solo el contador del bucle. */
void print_bonus_score(void) {
    uint16_t idx;                                          /* dat_36a0 */
    bios_set_cursor((uint8_t)(bonus_row >> 3), 0x12);      /* mov ah,2 / dh=[369e]>>3 / dl=0x12 / bh=0 */
    idx = 0x3;
lab_3a50:
    bios_teletype((uint8_t)(bonus_bcd[idx] + 0x30), 0x3);  /* al=[bx+dat_368d]+'0'; ah=0xe; bl=3 */
    idx++;
    if (idx < 0x7) goto lab_3a50;                          /* jb (sin signo) */
}

/* print_level7_bonus (L1282-1302): cursor (10,10); dat_3720 = dat_36ec[dat_370c] (2 caracteres ASCII, pisan los 2
 * ultimos del texto); imprime los 20 caracteres de dat_370e con color 3. El buffer es local: nadie lo vuelve a leer. */
void print_level7_bonus(void) {
    uint8_t text[20];
    uint16_t bx;
    bios_set_cursor(0xa, 0xa);                             /* dl=0xa / mov dh,dl */
    for (bx = 0; bx < 0x14; bx++) text[bx] = ds_pool[DS_L7_TEXT + bx];
    text[0x12] = ds_pool[DS_L7_MULT_TABLE + bonus_l7_index];       /* mov ax,[bx+dat_36ec] / mov [dat_3720],ax */
    text[0x13] = ds_pool[DS_L7_MULT_TABLE + bonus_l7_index + 1];   /* dat_3720 = dat_370e + 0x12 */
    bx = 0;                                                /* sub bx,bx */
lab_3a83:
    bios_teletype(text[bx], 0x3);                          /* al=[bx+dat_370e]; bl=3 */
    bx++;
    if (bx < 0x14) goto lab_3a83;                          /* jb */
}

/* mask_score_tiles (L1303-1316): 30 words de dat_35e0 AND dx -> DS:0x000e (score_tiles). */
void mask_score_tiles(uint16_t dx) {
    for (int i = 0; i < 0x1e; i++) {                       /* lodsw / and ax,dx / stosw / loop (cx=0x1e) */
        uint16_t w = (uint16_t)(score_ds_word((uint16_t)(DS_SCORE_TILES_SRC + 2 * i)) & dx);
        score_tiles[2 * i]     = (uint8_t)(w & 0xff);
        score_tiles[2 * i + 1] = (uint8_t)(w >> 8);
    }
}
