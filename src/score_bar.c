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

/* ---- T49 (level_objects.asm L1317-1367) ---- */
#include "sound.h"

#define DS_SCORE_TILE_LIST 0x361c   /* dat_361c: dims 0x0c05 (12 filas x 5 bytes) + 4 destinos + 0xffff */
#define DS_BCD_POW_TABLE   0x3684   /* dat_3684: BCD de 2^12; las potencias menores estan 7 bytes antes cada una */
#define SCORE_BAR_START    0x1b80   /* ax inicial: offset CGA de la primera posicion de la barra */

static uint16_t bar_limit;     /* DS 0x369a */
static uint16_t bar_base;      /* DS 0x369c */
static uint16_t bcd_src_bin;   /* DS 0x368b: copia del valor binario (solo scratch de binary_to_bcd) */

/* draw_block_list con la lista dat_361c: el origen de los 5 bloques es DS:0x000e, que el port modela como
 * `score_tiles` (lo deja mask_score_tiles), no como ds_pool (const). Mismo recorrido que level_background.c. */
static void draw_score_tiles(uint16_t base) {
    uint16_t list = DS_SCORE_TILE_LIST;
    uint8_t cols = ds_pool[list], rows = ds_pool[list + 1];
    list += 2;
    for (;;) {
        uint16_t src = score_ds_word(list);
        if (src == 0xffff) return;
        uint16_t dst = (uint16_t)(base + score_ds_word((uint16_t)(list + 2)));
        blit_bytes_to_cga(src == 0x000e ? score_tiles : &ds_pool[src], dst, cols, rows);
        list += 4;
    }
}

/* animate_score_bar (L1317-1347): dibuja los azulejos enmascarados de la barra de bonus desde la posicion 0x1b80
 * hacia atras en pasos de 0x280 (una fila de texto de 8 px) mientras la posicion siga >= el limite `ax`. Con
 * bonus_bar_flag != 0 suena un paso de melodia y espera 2 ticks BIOS (bloqueante, como el original) por paso. */
void animate_score_bar(uint16_t ax) {
    uint16_t dx;
    bar_limit = ax;                                        /* mov [dat_369a],ax */
    init_level_melody();
    ax = SCORE_BAR_START;
lab_3aba:
    bar_base = ax;                                         /* mov [dat_369c],ax */
    draw_score_tiles(ax);                                  /* bx=dat_361c / call draw_block_list */
    if (bonus_bar_flag == 0) goto lab_3ae2;                /* cmp byte [dat_369f],0 / jz */
    play_melody_step();
    bonus_tick_start = score_tick();                       /* sub ah,ah / int 0x1a / mov [dat_3695],dx */
lab_3ad5:
    dx = (uint16_t)(score_tick() - bonus_tick_start);      /* int 0x1a / sub dx,[dat_3695] */
    if (dx < 0x2) goto lab_3ad5;                           /* jb (sin signo) */
lab_3ae2:
    if (bar_base < 0x280) goto lab_3af0;                   /* sub ax,0x280 / jb */
    ax = (uint16_t)(bar_base - 0x280);
    if (ax >= bar_limit) goto lab_3aba;                    /* cmp ax,[dat_369a] / jnb */
lab_3af0:
    silence_speaker();
}

/* binary_to_bcd (L1348-1367): suma en bonus_bcd (limpio) el BCD de cada potencia de 2 activa en `ax`. Recorre solo
 * los bits 12..0 (dx = 0x1000 desplazado a la derecha hasta sacar el 1 final): los bits 13-15 se ignoran. */
void binary_to_bcd(uint16_t ax) {
    uint16_t bx, dx;
    bcd_src_bin = ax;                                      /* mov [dat_368b],ax */
    for (int i = 0; i < 8; i++) bonus_bcd[i] = 0;          /* dat_368d/368f/3691/3693 = 0 (4 words) */
    bx = DS_BCD_POW_TABLE;
    dx = 0x1000;
lab_3b0b:
    if ((bcd_src_bin & dx) == 0) goto lab_3b19;            /* test [dat_368b],dx / jz */
    add_bcd_scores(bonus_bcd, &ds_pool[bx]);               /* si=bx / di=dat_368d */
lab_3b19:
    bx = (uint16_t)(bx - 0x7);
    {
        uint8_t cf = (uint8_t)(dx & 1);                    /* shr dx,1 */
        dx >>= 1;
        if (!cf) goto lab_3b0b;                            /* jnb */
    }
}
