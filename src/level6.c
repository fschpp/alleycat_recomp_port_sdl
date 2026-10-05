#include "level6.h"
#include "cat_state.h"
#include "cga.h"
#include "gen/ds_pool.h"
#include <string.h>

/* T29 — nivel 6, helpers A (level_objects.asm L2984-3095). Traduccion literal, etiqueta por etiqueta. */

#define L6_OBJ_X          0x4411   /* word[12]: direccion CGA del objeto */
#define L6_OBJ_SPRITE_PTR 0x4429   /* word[12]: offset DS del sprite 5x13 (0x431e / 0x429c, delta 0x82 = 5*2*13) */
#define L6_OBJ_TYPE       0x4471   /* byte[8], indexado por difficulty_level: cantidad de objetos {6,6,7,7,8,8,9,9} */
#define L6_OBJ_INIT_Y_TBL 0x4479   /* byte[8], indexado por difficulty_level: tipo inicial de tile {1,2,3,4,4,4,4,4} */
#define L6_OBJ_DIMS       0x4481   /* word[12]: X en pixeles de cada tile */
#define L6_OBJ_Y          0x4499   /* byte[12]: Y de cada tile */
#define L5_SPRITE_TABLE_BASE 0x41fc

uint16_t l6_obj_flag[12];
uint16_t l6_obj_state[12];
uint8_t  l6_tile_type[12];
uint16_t l6_dat_43dc = 0;
uint16_t l6_dat_43de = 0;
uint8_t  l6_dat_43e0 = 0;
uint8_t  l6_dat_44d0 = 0;
uint16_t l6_dat_44d1 = 0;
uint8_t  l6_dat_44bd = 0;
uint8_t  l6_dat_44be = 0;
uint8_t  l6_dat_44d6 = 0;

static uint16_t l6_tracker_save[30];   /* DS 0x43a0..0x43dc = 0x3c bytes = 3 words x 10 filas */

static uint16_t ds_word(uint16_t off) { return (uint16_t)(ds_pool[off] | (ds_pool[off + 1] << 8)); }

void erase_l6_tracker(void) {
    if (l6_dat_43e0 != 0x0) return;                    /* jnz lab_4b1c */
    blit_to_cga((const uint8_t *)l6_tracker_save, l6_dat_43de, 3, 10);   /* cx=0xa03 */
}

void draw_l6_tracker(void) {
    l6_dat_43e0 = 0x0;
    uint16_t di = l6_dat_43dc;
    l6_dat_43de = di;
    uint16_t si = l6_dat_44d1;
    if (l6_dat_44d0 >= 0x80) si = (uint16_t)(si + 0x3c);   /* jb lab_4b40 salta el add */
    blit_masked(&ds_pool[si], di, 3, 10, l6_tracker_save);  /* bp=dat_43a0, cx=0xa03 */
}

uint16_t calc_l6_addr(uint16_t bx) {
    uint8_t dl = ds_pool[L6_OBJ_Y + bx];               /* mov dl,[bx+l6_obj_y] */
    uint16_t bx2 = (uint16_t)((bx & 0xff00) | (uint8_t)(bx << 1));   /* db 0xd0,0xe3 = shl bl,1 (solo bl) */
    uint16_t cx = ds_word((uint16_t)(L6_OBJ_DIMS + bx2));
    return (uint16_t)calc_cga_addr(dl, cx, NULL);
}

void draw_l6_tile(uint16_t bx) {
    uint16_t di = calc_l6_addr(bx);
    uint16_t ax = l6_tile_type[bx];                    /* mov al,[bx+dat_44c4] / sub ah,ah */
    ax = (uint16_t)(ax << 5);
    uint16_t si = (uint16_t)(ax + L5_SPRITE_TABLE_BASE);
    blit_to_cga(&ds_pool[si], di, 2, 8);               /* cx=0x802 */
}

void init_level6_objects(void) {
    uint16_t bx, cx;
    uint8_t dl;
    memset(l6_obj_flag, 0, sizeof l6_obj_flag);        /* rep stosw: 12 words en dat_4441 */
    bx = difficulty_level;                             /* mov bx,[0x8] */
    cx = ds_pool[L6_OBJ_TYPE + bx];                    /* mov cl,[bx+l6_obj_type] / sub ch,ch */
lab_4b62:
    dl = (uint8_t)(cga_random() & 0xff);               /* call random (dl = byte bajo del seed) */
    bx = (uint16_t)((bx & 0xff00) | dl);               /* mov bl,dl */
    bx &= 0x1e;
    if ((uint8_t)bx >= 0x18) goto lab_4b62;            /* jnb */
    if (l6_obj_flag[bx >> 1] != 0x0) goto lab_4b62;
    l6_obj_state[bx >> 1] = 0x0;
    l6_obj_flag[bx >> 1] = 0x1;
    blit_to_cga(&ds_pool[ds_word((uint16_t)(L6_OBJ_SPRITE_PTR + bx))],
                ds_word((uint16_t)(L6_OBJ_X + bx)), 5, 13);   /* push cx / cx=0xd05 / pop cx */
    cx = (uint16_t)(cx - 1);
    if (cx != 0) goto lab_4b62;                        /* loop */
    cx = 0xc;
lab_4b98:
    bx = (uint16_t)(cx - 1);
    dl = ds_pool[L6_OBJ_INIT_Y_TBL + difficulty_level];   /* mov si,[0x8] / mov dl,[si+...] */
    l6_tile_type[bx] = dl;
    draw_l6_tile(bx);                                  /* push cx / call / pop cx */
    cx = (uint16_t)(cx - 1);
    if (cx != 0) goto lab_4b98;                        /* loop */
    l6_dat_44d0 = 0x0;
    l6_dat_44bd = 0x0;
    l6_dat_43e0 = 0x1;
    l6_dat_44d6 = 0xc;
    l6_dat_44be = 0x0;
}
