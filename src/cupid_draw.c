/* cupid_draw.c — T57: dibujo, borrado, ventanas y colision del cupido (ui.asm L676-791). Ver include/cupid.h y PROGRESS.md §6ba.
 * Esta en un .c aparte de cupid.c (T56) para que los tests de update_cupid puedan envolver estas funciones con --wrap. */
#include <stdint.h>
#include "cupid.h"
#include "cat_state.h"
#include "cga.h"
#include "gen/ds_pool.h"
#include "level_collision.h"
#include "level_background.h"
#include "sound.h"

#define DS_DAT_6F30 0x6f30          /* 12 frames de 2 words x 8 filas (0x20 bytes): 6 hacia la izquierda, +0xc0 hacia la derecha */
#define DS_WINDOW_ROW_Y_TABLE      0x2bd4   /* 7 bytes: b0 98 80 68 50 38 20 */
#define DS_WINDOW_ROW_COL_OFFSET   0x2bdb   /* 7 bytes: 0,18,...,108 (fila * 18) */
#define DS_CUPID_SPRITE_OFFSET     0x70fc   /* 16 words: x del tile de cada columna de ventana (0x20..0x110); el nombre del ASM engana */
#define DS_CUPID_SPRITE_END        0x7120   /* 7 bytes: y del tile de cada fila (bf a7 8f 77 5f 47 2f) */

/* DS 0x70cc (dat_70cc, 16 words = 2 words x 8 filas): fondo que blit_transparent guarda al dibujar al cupido. */
static uint16_t cupid_bg_save[16];

/* --- T57 (ui.asm L676-791): dibujo, borrado, ventanas y colision. PROGRESS.md §6ba. --- */

/* draw_cupid (L676-697): sprite = dat_6f30 + (cupid_arrow_x & 0x1e0) (+0xc0 si dir != 0xff), a cupid_draw_addr, 2 words x
 * 8 filas (cx=0x802) con blit_transparent, que guarda el fondo en dat_70cc (bp). cupid_erase_addr = cupid_draw_addr. */
void draw_cupid(void) {
    uint16_t ax, si, di;
    cupid_drawn = 0x0;
    ax = (uint16_t)(cupid_arrow_x & 0x1e0);
    ax = (uint16_t)(ax + DS_DAT_6F30);
    if (cupid_dir == 0xff) goto lab_6217;                  /* cmp byte [cupid_dir],0xff / jz */
    ax = (uint16_t)(ax + 0xc0);
lab_6217:
    si = ax;                                               /* db 0x8b,0xf0 = mov si,ax */
    di = cupid_draw_addr;
    cupid_erase_addr = di;
    blit_transparent(&ds_pool[si], di, 0x02, 0x08, cupid_bg_save);   /* mov bp,dat_70cc / mov cx,0x802 */
}

/* erase_cupid (L699-714): solo si cupid_drawn == 0 (dibujado): devuelve el fondo guardado a cupid_erase_addr. */
void erase_cupid(void) {
    if (cupid_drawn != 0x0) goto lab_6244;                 /* cmp byte [cupid_drawn],0 / jnz */
    blit_to_cga((const uint8_t *)cupid_bg_save, cupid_erase_addr, 0x02, 0x08);   /* si=dat_70cc / cx=0x802 */
lab_6244:
    return;
}

/* cupid_toggle_window (L716-764). Busca la fila de ventana cuyo y == (cupid_y - 8) & 0xf8 en window_row_y_table (7
 * filas, de la 6 a la 0), la columna es (cupid_x >> 4) - 2 (0..15), y el indice window_row_col_offset[fila] + columna.
 * Si es distinto de cupid_prev_x (ultima ventana alternada), lo guarda y hace xor 2 en window_open_state[indice];
 * borra el cupido, redibuja el tile (x = cupid_sprite_offset[col], y = cupid_sprite_end[fila], bx = estado nuevo 0/2
 * como offset en l7_bg_tile_ptrs) y vuelve a dibujar al cupido. `db 0xd1,0xe7` = shl di,1. */
void cupid_toggle_window(void) {
    uint8_t al;
    uint16_t cx, bx, ax, di, dx, si, state;
    al = (uint8_t)(cupid_y - 0x8);
    al = (uint8_t)(al & 0xf8);
    cx = 0x7;
lab_624f:
    bx = (uint16_t)(cx - 1);                               /* mov bx,cx / dec bx */
    if (al == ds_pool[DS_WINDOW_ROW_Y_TABLE + bx]) goto lab_625b;   /* cmp al,[bx+window_row_y_table] / jz */
    cx = (uint16_t)(cx - 1);                               /* loop */
    if (cx != 0) goto lab_624f;
lab_625a:
    return;
lab_625b:
    ax = (uint16_t)(cupid_x >> 4);                         /* mov cl,4 / shr ax,cl */
    if (ax < 0x2) goto lab_625a;                           /* sub ax,2 / jb */
    ax = (uint16_t)(ax - 0x2);
    if (ax >= 0x10) goto lab_625a;                         /* cmp ax,0x10 / jnb */
    di = ax;
    dx = ds_pool[DS_WINDOW_ROW_COL_OFFSET + bx];           /* mov dl,[bx+window_row_col_offset] / sub dh,dh */
    ax = (uint16_t)(ax + dx);
    if (ax == cupid_prev_x) goto lab_625a;                 /* cmp ax,[cupid_prev_x] / jz */
    cupid_prev_x = ax;
    si = ax;
    window_open_state[si] ^= 0x2;                          /* xor byte [si+window_open_state],2 */
    state = window_open_state[si];                         /* mov al,[si+window_open_state] / sub ah,ah */
    di = (uint16_t)(di << 1);                              /* shl di,1 */
    cx = (uint16_t)(ds_pool[DS_CUPID_SPRITE_OFFSET + di] | (ds_pool[DS_CUPID_SPRITE_OFFSET + di + 1] << 8));
    dx = ds_pool[DS_CUPID_SPRITE_END + bx];
    erase_cupid();
    draw_bg_tile(cx, (uint8_t)dx, state);                  /* pop dx / pop cx / pop bx (= state) / call draw_bg_tile */
    draw_cupid();
}

/* check_cupid_collision (L766-791). Devuelve CF. Sin cupido activo: CF=0. Rect del cupido (x, y, ancho 0x10, alto 8)
 * contra el del gato (cat_x, cat_y, ancho 0x18, alto 0x0e) (cx=0xe08: ch = alto del gato, cl = alto del cupido). Si
 * choca: in_level_mode=1, anim_counter=2, anim_step=0x20, transition_timer=8, start_tone(0x91d, 0xce4), CF=1. (El
 * comentario del ASM habla de cat_died, pero el codigo no lo toca.) */
int check_cupid_collision(void) {
    if (cupid_active != 0x0) goto lab_62af;
    return 0;                                              /* clc / ret */
lab_62af:
    if (!check_rect_collision((int16_t)cupid_x, cupid_y, 0x10, 0x08, (uint16_t)cat_x, cat_y, 0x18, 0x0e)) goto lab_62ea;   /* jnb */
    in_level_mode = 0x1;
    anim_counter = 0x2;
    anim_step = 0x20;
    transition_timer = 0x8;
    start_tone(0x91d, 0xce4);
    return 1;                                              /* stc */
lab_62ea:
    return 0;
}
