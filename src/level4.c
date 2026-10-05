/* level4.c — nivel 4, helpers A (T21). Ver include/level4.h y PROGRESS.md §6r. */
#include "level4.h"
#include "level45_state.h"
#include "level_collision.h"
#include "cat_state.h"
#include "cga.h"
#include "gen/ds_pool.h"

#define L4_PLATFORM_OFFSET 0x1050  /* byte[16] */
#define L4_OBJ_X_TABLE     0x1137  /* word[16] */

bool check_l4_thrown_collision(void) {
    /* ax=thrown_obj_x, dl=thrown_obj_y, si=0x10 | bx=cat_x, dh=cat_y, di=0x18, cx=0x0e1e */
    return check_rect_collision(thrown_obj_x, thrown_obj_y, 0x10, 0x1e,
                                (uint16_t)cat_x, cat_y, 0x18, 0x0e);
}

void calc_l4_obj_pos(uint16_t bx) {
    uint16_t di = l5_obj_frame[bx];
    uint8_t al = ds_pool[L4_PLATFORM_OFFSET + di];
    uint8_t dl = 0xa;
    if (di < 0x3) dl = 0;                       /* cmp di,3 / jnb: sin signo */
    al = (uint8_t)(al - dl);
    al = (uint8_t)(al + 0x3);
    l5_obj_y_pos[bx] = al;
    di = (uint16_t)(di << 1);
    uint16_t ax = (uint16_t)(ds_pool[L4_OBJ_X_TABLE + di] | (ds_pool[L4_OBJ_X_TABLE + di + 1] << 8));
    l5_obj_dims[bx] = (uint16_t)(ax + 0x8);
}

bool check_l4_proximity(uint16_t bx, uint16_t bp) {
    uint16_t ax = l5_obj_dims[bx];
    bool borrow = ax < (uint16_t)cat_x;
    ax = (uint16_t)(ax - (uint16_t)cat_x);
    if (borrow) ax = (uint16_t)~ax;             /* jnb salta el `not` si NO hubo préstamo */
    uint8_t dl = l5_obj_y_pos[bx];
    borrow = dl < cat_y;
    dl = (uint8_t)(dl - cat_y);
    if (borrow) dl = (uint8_t)~dl;
    ax = (uint16_t)(ax + dl);                   /* dh = 0 */
    return ax < bp;                             /* jb => stc */
}

void randomize_l4_pos(uint16_t bx) {
    l5_anim_delay = 0x20;
    for (;;) {                                  /* lab_427c */
        uint16_t dx = (uint16_t)(cga_random() & 0xf);
        bool retry = false;
        for (uint16_t di = 0; di < 0x8; di = (uint16_t)(di + 2)) {
            if (di == (uint16_t)(bx * 2)) continue;            /* cmp di,si / jz lab_428f */
            if (l5_obj_frame[di >> 1] == dx) { retry = true; break; }
        }
        if (retry) continue;
        l5_obj_frame[bx] = dx;
        calc_l4_obj_pos(bx);
        if (l5_anim_delay == 0) return;
        if (check_l4_proximity(bx, 0x32)) {
            l5_anim_delay--;
            continue;
        }
        return;
    }
}

void init_level4_objects(void) {
    for (uint16_t cx = 4; cx != 0; cx--) {
        uint16_t bx = (uint16_t)(cx - 1);
        l5_obj_active[bx] = 0x1;
        l5_obj_hit[bx] = 0x0;
        randomize_l4_pos(bx);
        uint8_t dl = (uint8_t)(cga_random() & 0xf);
        l5_obj_anim[bx] = (uint8_t)(dl + 0x14);
    }
    l5_obj_index = 0x0;
    l5_obj_count = 0x4;
}

void erase_level4_sprite(void) {
    uint16_t bx = l5_obj_index;
    if (l5_obj_active[bx] != 0) return;
    blit_to_cga(l5_obj_save_buf[bx], l5_obj_cga_addr[bx], 2, 12);
}

/* ---- T22: helpers B ---- */

bool check_l4_obj_cat(uint16_t idx) {
    /* ax=dims[si], dl=y_pos[bx], si=0x10 | bx=cat_x, dh=cat_y, di=0x18, cx=0x0e0c */
    return check_rect_collision((int16_t)l5_obj_dims[idx], l5_obj_y_pos[idx], 0x10, 0x0c,
                                (uint16_t)cat_x, cat_y, 0x18, 0x0e);
}

bool check_l4_obj_thrown(uint16_t idx) {
    /* ax=dims[si], dl=y_pos[bx], si=0x10 | bx=thrown_obj_x, dh=thrown_obj_y, di=si=0x10, cx=0x1e0c */
    return check_rect_collision((int16_t)l5_obj_dims[idx], l5_obj_y_pos[idx], 0x10, 0x0c,
                                (uint16_t)thrown_obj_x, thrown_obj_y, 0x10, 0x1e);
}

/* ---- T22: cola de init_level4_bg ---- */

#define L4_DAT_3CC3 0x3cc3  /* 4 grupos x 8 bytes = 32 (hasta dat_3ce3) */

uint8_t l4_dat_3ce3[16];    /* DS 0x3ce3: valores iniciales = 0 (verificado en /tmp/data_segment.bin) */
uint8_t l4_dat_3cf3[16];    /* DS 0x3cf3 */

void init_level4_bg_tail(void) {
    /* mov bx,[0x8] / mov cl,3 / and bl,cl / shl bl,cl: solo bl se toca (8 bits); bh queda como el
     * byte alto de difficulty_level (0 en la práctica). */
    uint8_t bl = (uint8_t)(difficulty_level & 0xff);
    bl = (uint8_t)(bl & 0x3);
    bl = (uint8_t)(bl << 3);
    uint16_t bx = (uint16_t)((difficulty_level & 0xff00) | bl);
    for (uint16_t si = 0; si < 0x10; si = (uint16_t)(si + 2)) {   /* lab_403c */
        uint8_t al = ds_pool[L4_DAT_3CC3 + bx];
        uint8_t ah = al;
        al = (uint8_t)(al >> 4);
        l4_dat_3ce3[si] = al;
        l4_dat_3cf3[si] = 0x0;
        ah = (uint8_t)(ah & 0xf);
        l4_dat_3ce3[si + 1] = ah;          /* dat_3ce4 = dat_3ce3 + 1 */
        l4_dat_3cf3[si + 1] = 0x0;         /* dat_3cf4 = dat_3cf3 + 1 */
        bx = (uint16_t)(bx + 1);
    }
}
