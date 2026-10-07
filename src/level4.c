/* level4.c — nivel 4, helpers A (T21). Ver include/level4.h y PROGRESS.md §6r. */
#include "bios_clock.h"
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

/* ---- T23: update_level4_anim ---- */

#include "alley.h"
#include "sound.h"
#include <time.h>

int32_t l4_tick_override = -1;

/* `sub ah,ah / int 0x1a` -> dx (mismo sustituto que en el resto del port). */
static uint16_t read_bios_tick(void) {
    if (l4_tick_override >= 0) return (uint16_t)l4_tick_override;
    return bios_clock_read();
}

/* Offsets DS (resueltos con tools/asm_label.sh; son CONSTANTES inmediatas del tipo `mov ax,label`,
 * no datos, salvo las dos tablas que se leen con corchetes). */
#define L4_SPRITE_CAUGHT_ICON 0x3d20  /* dat_3d20: 2 words x 12 filas = 48 bytes (icono de bonus Y 2º frame) */
#define L4_SPRITE_A           0x3d80  /* 48 bytes */
#define L4_SPRITE_B           0x3db0  /* 48 bytes */
#define L4_SPRITE_FRAMES      0x3de0  /* dat_3de0: word[2] = {0x3d50, 0x3d20} (punteros a sprites) */
#define L4_PROX_THRESHOLD     0x3ede  /* "l5_save_buf_ptrs" (nombre engañoso): word[8] por dificultad
                                       * {20,80,100,120,120,140,140,140} = umbral bp de check_l4_proximity */

uint16_t l4_dat_3de4;                 /* DS 0x3de4 (word): dirección CGA calculada para el dibujo (global solo para los tests) */

static uint16_t ds_word(uint32_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

void update_level4_anim(void) {
    uint16_t dx = read_bios_tick();                    /* sub ah,ah / int 0x1a */
    if (dx == l5_last_tick) return;                    /* cmp dx,[l5_last_tick] / jnz lab_40cd */

    /* lab_40cd */
    l5_obj_index = (uint16_t)(l5_obj_index + 1);
    uint16_t bx = l5_obj_index;
    if (bx <= 0x2) goto lab_40e5;                      /* jbe (sin signo) */
    if (bx < 0x4) goto lab_40e9;                       /* bx == 3: NO actualiza l5_last_tick */
    bx = 0; l5_obj_index = 0;
lab_40e5:
    l5_last_tick = dx;
lab_40e9:
    /* mov si,bx / `db d1 e6` = shl si,1: si = 2*bx, implícito en los helpers que reciben el índice */
    if (l5_obj_hit[bx] != 0) return;                   /* jnz lab_40cc (ret) */
    if (check_l4_obj_cat(bx)) goto lab_4124;
    /* lab_40fc */
    if (check_l4_obj_thrown(bx)) return;               /* jb lab_40cc */
    if (l5_obj_anim[bx] == 0) {
        randomize_l4_pos(bx);
        uint8_t dl = (uint8_t)(cga_random() & 0x7);
        dl = (uint8_t)(dl + 0x14);
        l5_obj_anim[bx] = dl;
    }
    /* lab_4118 */
    l5_obj_anim[bx] = (uint8_t)(l5_obj_anim[bx] - 1);
    if (check_l4_obj_cat(bx)) goto lab_4124;
    goto lab_4181;

lab_4124:                                              /* el gato toca al objeto */
    if (l5_obj_active[bx] != 0) return;                /* jnz lab_4132 (ret) */
    if (l5_obj_anim[bx] < 0x14) goto lab_4133;         /* jb */
    return;                                            /* lab_4132 */

lab_4133: {
    restore_alley_buffer();
    erase_level4_sprite();                             /* usa l5_obj_index, no bx */
    bx = l5_obj_index;
    l5_obj_hit[bx] = 0x1;
    save_alley_buffer();
    at_platform = 0x0;                                 /* mov byte [0x55c],0 */
    l5_obj_count = (uint8_t)(l5_obj_count - 1);
    if (l5_obj_count == 0) cat_caught = 0x1;           /* dec / jnz lab_4155 ; mov byte [0x553],1 */
    /* lab_4155: icono de bonus en x = 0x51 + 4*(4 - count). bp=0xe (scratch DS:0xe) no lo consume
     * nada: mask_save = NULL, igual que el blit de init_level4_bg. */
    uint8_t al = (uint8_t)(4 - l5_obj_count);
    al = (uint8_t)(al << 2);                           /* shl al,cl con cl=2 */
    uint16_t di = (uint16_t)(al + 0x51);               /* ah = 0 */
    blit_masked(&ds_pool[L4_SPRITE_CAUGHT_ICON], di, 2, 12, NULL);
    start_tone(0x3e8, 0x2ee);
    return;
}

lab_4181: {
    calc_l4_obj_pos(bx);
    uint16_t di = (uint16_t)(difficulty_level << 1);   /* `db d1 e7` = shl di,1 */
    uint16_t bp = ds_word((uint32_t)((L4_PROX_THRESHOLD + di) & 0xffff));
    if (check_l4_proximity(bx, bp)) {                  /* jnb lab_41b0 salta si NO hay CF */
        if (l5_obj_anim[bx] >= 0x2) {                  /* cmp 2 / jb lab_41b0 */
            uint8_t al = 0x1;
            if (l5_obj_anim[bx] <= 0x11) goto lab_41ac;      /* jbe */
            if (l5_obj_anim[bx] >= 0x14) goto lab_41b0;      /* jnb */
            al = (uint8_t)(al - 1);                          /* dec al -> 0 */
lab_41ac:
            l5_obj_anim[bx] = al;
        }
    }
}
lab_41b0: {
    uint8_t al = l5_obj_anim[bx];
    uint16_t ax;
    if (al <= 0x1) goto lab_41d8;                      /* jbe */
    if (al < 0x12) goto lab_41f8;                      /* jb */
    al = 0x1;
    if (!(l5_obj_frame[bx] >= 0x3)) al = 0x3;          /* cmp word,3 / jnb: sin signo */
    l5_obj_y_pos[bx] = (uint8_t)(l5_obj_y_pos[bx] + al);
    if (l5_obj_anim[bx] < 0x13) goto lab_41f3;         /* jb */
    if (l5_obj_anim[bx] == 0x13) goto lab_41ee;        /* jz */
    ax = 0;                                            /* anim >= 0x14: no se dibuja */
    goto lab_4204;
lab_41d8:
    al = 0x1;
    if (!(l5_obj_frame[bx] >= 0x3)) al = 0x3;
    l5_obj_y_pos[bx] = (uint8_t)(l5_obj_y_pos[bx] + al);
    if (l5_obj_anim[bx] >= 0x1) goto lab_41f3;         /* jnb */
lab_41ee:
    ax = L4_SPRITE_B;
    goto lab_4204;
lab_41f3:
    ax = L4_SPRITE_A;
    goto lab_4204;
lab_41f8: {
    /* `db d0 e0` = shl al,1; mov di,ax; and di,2 -> solo importa el bit 0 de anim */
    uint16_t d = (uint16_t)(((uint8_t)(al << 1)) & 0x2);
    ax = ds_word((uint32_t)(L4_SPRITE_FRAMES + d));
}
lab_4204:
    l5_obj_sprite_ptr = ax;
    uint8_t dl = l5_obj_y_pos[bx];
    uint16_t cx = l5_obj_dims[bx];
    l4_dat_3de4 = (uint16_t)calc_cga_addr(dl, cx, NULL);
}
    erase_level4_sprite();
    bx = l5_obj_index;                                 /* mov bx,[l5_obj_index] / mov si,bx / shl si,1 */
    if (check_l4_obj_thrown(bx)) return;               /* jnb lab_4226 ; ret */
    if (l5_obj_sprite_ptr == 0) {                      /* lab_4226 */
        l5_obj_active[bx] = 0x1;
        return;
    }
    /* lab_4233 */
    l5_obj_active[bx] = 0x0;
    l5_obj_cga_addr[bx] = l4_dat_3de4;
    blit_masked(&ds_pool[l5_obj_sprite_ptr], l4_dat_3de4, 2, 12, (uint16_t *)(void *)l5_obj_save_buf[bx]);
}


/* ---- T24: update_level4_state (level_objects.asm L1720-1823) ---- */

#include "level_objects.h"

#define L4_DAT_3C5A 0x3c5a  /* DS 0x3c5a: word[2*n] = {dir. CGA, 0x3b92/0x3b12 (no se usa aquí)} por puerta, 4 bytes c/u */
#define L4_ANIM_OFFSET_TABLE 0x3d06  /* word[8], indexado por l3_door_anim_frame (byte par 0..0xe) */

void update_level4_state(void) {
    uint16_t dx, ax, di, si, bx;
    uint8_t bl;

    if (l3_door_anim_frame == 0x0) goto lab_3ea4;
    dx = read_bios_tick();                              /* sub ah,ah / int 0x1a */
    if (dx == l4_last_tick) return;                     /* jz lab_3eb9 (ret) */
    goto lab_3f35;

lab_3ea4:
    if (auto_walk != 0x0) return;                       /* cmp byte [0x584],0 / jnz lab_3eb9 */
    if (joy_button != 0x0) return;                      /* cmp byte [0x69a],0 / jnz lab_3eb9 */
    if (l3_platform_id != 0x0) goto lab_3eba;           /* jnz lab_3eba */
    return;                                             /* lab_3eb9 */

lab_3eba:
    if (check_l4_thrown_collision()) return;            /* jb lab_3eb9 */
    dx = read_bios_tick();
    ax = (uint16_t)(dx - l4_dat_3d18);
    if (ax < 0xc) return;                               /* cmp ax,0xc / jb (sin signo) */
    l4_dat_3d18 = dx;
    at_platform = 0x0;                                  /* mov byte [0x55c],0 */
    bl = (uint8_t)(l3_platform_id - 1);                 /* dec bl (8 bits); bh = 0 */
    bx = bl;
    si = (uint16_t)(bx << 2);                           /* mov cl,2 / shl si,cl */
    l3_door_cga_1 = ds_word(L4_DAT_3C5A + si);
    ax = 0;
    if (bl < 0x3) ax = 0x80;                            /* cmp bl,3 / jnb: al = 0x80 solo si bl < 3 */
    l3_door_cga_2 = ax;
    bl = l4_dat_3ce3[bx];                               /* mov bl,[bx+dat_3ce3]  (bh sigue en 0) */
    bx = bl;
    si = (uint16_t)(bx << 2);
    l3_door_cga_3 = ds_word(L4_DAT_3C5A + si);
    ax = 0;
    if (bl < 0x3) ax = 0x80;
    l3_door_sprite_base = ax;
    l4_obj_cur_y = ds_pool[L4_PLATFORM_OFFSET + bx];    /* mov al,[bx+l4_platform_offset] */
    bl = (uint8_t)(bl << 1);                            /* `db d0 e3` = shl bl,1 (no shl bl,0) */
    bx = bl;
    l4_obj_cur_x = (uint16_t)(ds_word(L4_OBJ_X_TABLE + bx) + 0x8);
    restore_alley_buffer();
    l3_door_anim_frame = 0xe;
    joy_button = 0x10;                                  /* mov byte [0x69a],0x10 */

lab_3f35:
    if (enemy_chasing == 0x0) erase_l1_thrown();        /* erase_thrown_sprite */
    l3_door_anim_frame = (uint8_t)(l3_door_anim_frame - 0x2);
    bx = l3_door_anim_frame;                            /* sub bh,bh / mov bl,[...] */
    if (bx < 0x8) goto lab_3f58;                        /* jb */
    di = l3_door_cga_1;
    ax = l3_door_cga_2;
    goto lab_3f70;

lab_3f58:
    di = l3_door_cga_3;
    cat_y = l4_obj_cur_y;
    cat_y_bottom = (uint8_t)(l4_obj_cur_y + 0x32);      /* mov [0x57c],al */
    cat_x = (int16_t)l4_obj_cur_x;
    ax = l3_door_sprite_base;

lab_3f70:
    ax = (uint16_t)(ax + ds_word(L4_ANIM_OFFSET_TABLE + bx));
    blit_to_cga(&ds_pool[ax], di, 2, 0x10);             /* mov si,ax / cx=0x1002 */
    if (enemy_chasing == 0x0) draw_l1_thrown();         /* draw_thrown_sprite */
    l4_last_tick = read_bios_tick();                    /* sub ah,ah / int 0x1a / mov [l4_last_tick],dx */
    if (l3_door_anim_frame == 0x0) save_cat_background();
}
