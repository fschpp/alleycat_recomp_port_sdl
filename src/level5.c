/* level5.c — nivel 5, helpers A (T25). Ver include/level5.h y PROGRESS.md §6v. */
#include "level5.h"
#include "level_collision.h"
#include "cat_state.h"
#include "cga.h"
#include "gen/ds_pool.h"

uint16_t l5_dat_40a8 = 0;
uint8_t  l5_dat_40aa = 0;
uint16_t l5_dat_40ab = 0;
uint8_t  l5_dat_40af = 0;
uint8_t  l5_dat_40b1 = 0;
uint16_t l5_dat_40b2 = 0;
uint8_t  l5_dat_40b4 = 0;
uint8_t  l5_dat_40b8 = 0;
uint8_t  l5_dat_40b9 = 0;
uint16_t l5_dat_40c8 = 0;
uint8_t  l5_dat_40ca = 0;
uint8_t  l5_dat_40cb = 0;
uint16_t l5_dat_40cc = 0;

bool check_l5_landing(void) {
    /* cmp byte [in_level_mode],0 / jnz lab_44f9 / al = cat_y & 0xf8 / cmp al,0x88 / jnz lab_44f9 */
    if (in_level_mode != 0) return false;
    return (uint8_t)(cat_y & 0xf8) == 0x88;
}

void calc_l5_direction(void) {
    /* ax = dat_40b2 - cat_x (16 bits sin signo); jnb salta el `not` si NO hubo préstamo */
    uint16_t ax = l5_dat_40b2;
    uint8_t dl = 0x1;
    bool borrow = ax < (uint16_t)cat_x;
    ax = (uint16_t)(ax - (uint16_t)cat_x);
    if (borrow) { ax = (uint16_t)~ax; dl = 0xff; }
    l5_dat_40ca = dl;
    l5_dat_40cc = ax;
    /* al = dat_40b4 - cat_y (8 bits) */
    uint8_t al = l5_dat_40b4;
    dl = 0x1;
    borrow = al < cat_y;
    al = (uint8_t)(al - cat_y);
    if (borrow) { al = (uint8_t)~al; dl = 0xff; }
    l5_dat_40cb = dl;
    /* db 0x2a,0xe4 = sub ah,ah ; db 0xd1,0xe0 = shl ax,1 (el desensamblador lo muestra como `shl ax,0x0`) */
    ax = (uint16_t)al;
    ax = (uint16_t)(ax << 1);
    l5_dat_40cc = (uint16_t)(l5_dat_40cc + ax);
}

void check_l5_cat_catch(void) {
    /* ax=dat_40b2, dl=dat_40b4, si=8 | bx=cat_x, dh=cat_y, di=0x18, cx=0x0e05 (cl=5 alto A, ch=0x0e alto B) */
    if (!check_rect_collision((int16_t)l5_dat_40b2, l5_dat_40b4, 0x8, 0x5,
                              (uint16_t)cat_x, cat_y, 0x18, 0x0e)) return;     /* jnb lab_4556 */
    if (object_hit != 0) return;                                              /* cmp [0x552],0 / jnz */
    cat_caught = 0x1;                                                         /* mov byte [0x553],1 */
}

void check_l5_thrown(void) {
    /* ax=dat_40b2, dl=dat_40b4, si=8 | bx=thrown_obj_x, dh=thrown_obj_y, di=0x10, cx=0x1e05 */
    if (!check_rect_collision((int16_t)l5_dat_40b2, l5_dat_40b4, 0x8, 0x5,
                              (uint16_t)thrown_obj_x, thrown_obj_y, 0x10, 0x1e)) return;
    l5_dat_40b8 = 0xff;
}

/* ---- T26: helpers B ---- */
#define L5_PERCH_SPRITE 0x3fbe  /* 3 words x 16 filas = 96 bytes en el DS */

uint16_t l5_dat_40a6 = 0;
static uint8_t l5_perch_save[96];   /* DS 0x401e: fondo guardado (3 words x 16 filas) */

bool check_l5_perch_hit(void) {
    /* ax=dat_40a8, dl=dat_40aa, si=0x18 | bx=cat_x, dh=cat_y, di=si=0x18, cx=0x0e10 (cl=0x10, ch=0x0e) */
    return check_rect_collision((int16_t)l5_dat_40a8, l5_dat_40aa, 0x18, 0x10,
                                (uint16_t)cat_x, cat_y, 0x18, 0x0e);
}

void draw_l5_perch(void) {
    l5_dat_40a6 = l5_dat_40ab;
    blit_masked(&ds_pool[L5_PERCH_SPRITE], l5_dat_40ab, 3, 16, (uint16_t *)(void *)l5_perch_save);
}

void erase_l5_perch(void) {
    blit_to_cga(l5_perch_save, l5_dat_40a6, 3, 16);
}

bool check_l5_thrown_near(void) {
    if (thrown_obj_y < 0x66) return false;                         /* jb lab_47a4 */
    uint16_t ax = (uint16_t)(l5_dat_40a8 - 0x14);
    if (ax > (uint16_t)thrown_obj_x) return false;                 /* ja */
    ax = (uint16_t)(ax + 0x30);
    if (ax < (uint16_t)thrown_obj_x) return false;                 /* jb */
    return true;
}

bool check_thrown_near_cat(void) {
    uint16_t bx = (uint16_t)cat_x;
    bool borrow = bx < 0x8;
    bx = (uint16_t)(bx - 0x8);
    if (borrow) bx = 0;                                            /* jnb salta el `sub bx,bx` */
    uint8_t dh = (uint8_t)(cat_y + 0x3);
    /* ax=thrown_obj_x, dl=thrown_obj_y, si=0x10 | bx, dh, di=0x28, cx=0x0e1e (cl=0x1e, ch=0x0e) */
    return check_rect_collision(thrown_obj_x, thrown_obj_y, 0x10, 0x1e, bx, dh, 0x28, 0x0e);
}

void init_level5_objects(void) {
    uint16_t cx = 0x90;
    uint8_t dl = 0x86;
    l5_dat_40a8 = cx;
    l5_dat_40aa = dl;
    l5_dat_40ab = (uint16_t)calc_cga_addr(dl, cx, NULL);
    draw_l5_perch();
    l5_dat_40af = 0x0;
    l5_dat_40b1 = 0x0;
    l5_dat_40b9 = 0x1;
    l5_dat_40b8 = 0x0;
    l5_dat_40c8 = 0xff;
}
