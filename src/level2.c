#include "level2.h"
#include "cat_state.h"
#include "cga.h"
#include "gen/ds_pool.h"
#include <stdint.h>
#include <stdbool.h>

uint8_t  l2_dat_3410 = 0;
uint16_t l2_anim_toggle = 0;
uint16_t l2_dat_3415 = 0;
uint8_t  l2_dat_3417[L2_OBJ_COUNT];
uint8_t  l2_dat_342f[L2_OBJ_COUNT];
uint16_t l2_obj_x[L2_OBJ_COUNT];
uint8_t  l2_obj_y[L2_OBJ_COUNT];
uint8_t  l2_obj_hit[L2_OBJ_COUNT];
uint8_t  l2_obj_active[L2_OBJ_COUNT];
uint16_t l2_obj_cga_addr[L2_OBJ_COUNT];
uint16_t l2_obj_cur_addr = 0;
uint8_t  l2_dat_351b = 0;

#define L2_ERASE_PATTERN 0x3404   /* dat_3404: 12 bytes de 0x55 (6x1) seguidos de 8 (2x2): patron de borrado */

static uint8_t l2_random_dl(void) { return (uint8_t)(cga_random() & 0xFF); }   /* `call random` -> dl */

void init_level2_objects(void) {
    uint16_t cx, bx;
    uint8_t dl, cl;
    l2_anim_toggle = 0x0;
    l2_dat_3415 = 0x0;
    l2_dat_3410 = 0xc;
    cx = 0x18;
lab_35dd:
    bx = (uint16_t)(cx - 1);
    l2_obj_hit[bx] = 0x1;
    l2_obj_active[bx] = 0x0;
    l2_obj_y[bx] = ds_pool[L2_OBJ_INIT_Y + bx];
    l2_dat_342f[bx] = 0x1;
    dl = l2_random_dl();
    dl &= 0x1;
    if (dl != 0) goto lab_3601;
    dl = (uint8_t)~dl;
lab_3601:
    l2_dat_3417[bx] = dl;
    l2_obj_x[bx] = l2_random_dl();                     /* sub dh,dh / mov [bx+l2_obj_x],dx (bx ya duplicado) */
    cx = (uint16_t)(cx - 1);                           /* loop lab_35dd */
    if (cx != 0) goto lab_35dd;
    cl = ds_pool[L2_DAT_351C + difficulty_level];      /* mov bx,[0x8] / mov cl,[bx+dat_351c] / sub ch,ch */
    cx = cl;
lab_361c:
    dl = l2_random_dl();
    dl &= 0xf;
    if (dl >= 0xc) goto lab_361c;
    bx = (uint16_t)(dl + 0xc);
    if (l2_obj_active[bx] != 0x0) {
        /* DESVIACION documentada en level2.h: con los 12 slots ya activos el reintento no termina nunca en el original */
        bool all = true;
        for (int i = 12; i < 24; i++) if (!l2_obj_active[i]) all = false;
        if (all) return;
        goto lab_361c;
    }
    l2_obj_active[bx] = 0x1;
    cx = (uint16_t)(cx - 1);                           /* loop lab_361c (cx=0 inicial da 0xffff vueltas) */
    if (cx != 0) goto lab_361c;
}

void reset_caught_objects(void) {
    uint16_t cx, bx, ax;
    uint8_t dl;
lab_reset:
    cx = 0xc;
lab_3640:
    bx = (uint16_t)(cx + 0xb);
    if (l2_obj_active[bx] == 0x0) goto lab_3672;
    ax = 0;
    dl = 0x1;
    l2_obj_active[bx] = (uint8_t)ax;
    if ((uint16_t)cat_x > 0xa0) goto lab_3661;         /* cmp word [cat_x],0xa0 / ja */
    ax = 0x12e;
    dl = 0xff;
lab_3661:
    l2_dat_3417[bx] = dl;
    l2_obj_x[bx] = ax;                                 /* shl bl,1 / mov [bx+l2_obj_x],ax */
    l2_dat_351b = (uint8_t)(l2_dat_351b - 1);
    if (l2_dat_351b != 0) goto lab_reset;              /* jnz reset_caught_objects */
    return;
lab_3672:
    cx = (uint16_t)(cx - 1);                           /* loop lab_3640 */
    if (cx != 0) goto lab_3640;
}

void erase_level_object(uint16_t slot) {
    if (l2_obj_hit[slot] != 0x0) return;
    uint16_t di = l2_obj_cga_addr[slot];
    if (slot * 2u < 0x18) blit_to_cga(&ds_pool[L2_ERASE_PATTERN], di, 1, 6);   /* cx=0x601 */
    else                  blit_to_cga(&ds_pool[L2_ERASE_PATTERN], di, 2, 2);   /* cx=0x202 */
}
