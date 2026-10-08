#include "bios_clock.h"
#include "level2.h"
#include "cat_state.h"
#include "cga.h"
#include "gen/ds_pool.h"
#include "level_collision.h"
#include "sound.h"
#include "alley.h"
#include "palette.h"
#include <time.h>
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

/* ===== T34: check_level_objects (level_objects.asm L690-805) ===== */
#define L2_DAT_3513   0x3513   /* DS: word[2] {8, 0x10}: ancho del rect del objeto (slot < 12 / >= 12) */
#define L2_DAT_3517   0x3517   /* DS: word[2] {6, 2}: alto del rect del objeto (cl; ch = 0xe es el del gato) */
#define L2_HIT_SPRITE 0x3350   /* DS: l1_anim_sprite_c (5 palabras x 0x12 filas) */

uint16_t l2_dat_3511 = 0;                 /* DS 0x3511 (word): slot en curso del barrido */
uint16_t l2_dat_3509 = 0;                 /* DS 0x3509 (word): ultimo tick procesado por update_level2_objects; check_level_objects lo pisa */
uint16_t l2_dat_350b = 0;                 /* DS 0x350b (word): tick de la pasada en curso de update_level2_objects */
uint16_t l2_dat_3413 = 0;                 /* DS 0x3413 (word): fase del sprite de los objetos 12..23 (+8 por vuelta, & 0x18) */
uint8_t  l2_border_color = 0;             /* ultimo color de borde pedido con `int 0x10 ah=0xb` (sin efecto visible en el port) */
uint16_t (*l2_tick_fn)(void) = NULL;      /* solo tests: sustituye a `int 0x1a` */
bool     (*l2_vsync_fn)(void) = NULL;     /* solo tests: sustituye al bit 3 de 0x3da */

static uint16_t l2_ds_word(uint16_t off) { return (uint16_t)(ds_pool[off] | (ds_pool[off + 1] << 8)); }

static uint16_t l2_read_bios_tick(void) {            /* sub ah,ah / int 0x1a -> dx */
    if (l2_tick_fn) return l2_tick_fn();
    return bios_clock_read();
}

/* check_vsync + `jz`: aqui el bucle ESPERA de verdad (el original bloquea ~1 retrace por vuelta, y llama update_noise sin
 * parar mientras espera: ese es el siseo). El retrace se simula con el reloj: 60 Hz, activo el ultimo ~1.4 ms de cada frame. */
static bool l2_vsync_active(void) {
    if (l2_vsync_fn) return l2_vsync_fn();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t us = (uint64_t)ts.tv_sec * 1000000u + (uint64_t)(ts.tv_nsec / 1000);
    return (us % 16667u) >= 15267u;
}

static void l2_set_border(uint8_t color) { l2_border_color = color; bios_color_select(0x0, color); }   /* int 0x10 ah=0xb: ahora se aplica a la paleta */

/* Devuelve el carry: 1 si se atrapo algo y se llamo reset_caught_objects; 0 en otro caso (y tras el golpe fatal). */
bool check_level_objects(void) {
    uint16_t bx, ax, cx, tick0, dx;
    uint8_t dl, di;
    l2_dat_3511 = 0x0;
    l2_dat_351b = 0x0;
lab_34ab:
    bx = l2_dat_3511;
    if (l2_obj_active[bx] == 0x0) goto lab_34b9;
lab_34b6:
    goto lab_35ad;
lab_34b9:
    ax = l2_obj_x[bx];                                 /* mov si,bx / shl si,1 / mov ax,[si+l2_obj_x] */
    dl = l2_obj_y[bx];
    di = 0x0;
    if (bx < 0xc) goto lab_34d0;
    di = 0x2;
lab_34d0:
    /* rect A: x = ax, y = dl, ancho = si, alto = cl; rect B (gato): x = cat_x, y = cat_y, ancho = 0x18, alto = ch = 0xe */
    if (!check_rect_collision((int16_t)ax, dl, l2_ds_word((uint16_t)(L2_DAT_3513 + di)),
                              (uint8_t)l2_ds_word((uint16_t)(L2_DAT_3517 + di)),
                              (uint16_t)cat_x, cat_y, 0x18, 0xe)) goto lab_34b6;   /* jnc */
    bx = l2_dat_3511;
    if (bx < 0xc) goto lab_356f;
    if (cat_caught != 0x0) goto lab_356f;
    if (immune_flag != 0x0) goto lab_356f;
    object_hit = 0x1;
    cx = (uint16_t)cat_x;
    {
        bool borrow = cx < 0x8;                        /* sub cx,8 / jnc */
        cx = (uint16_t)(cx - 0x8);
        if (borrow) cx = 0;                            /* sub cx,cx */
    }
    if (cx >= 0x117) cx = 0x116;
    dl = cat_y;
    if (dl >= 0xb5) dl = 0xb4;
    blit_to_cga(&ds_pool[L2_HIT_SPRITE], calc_cga_addr(dl, cx, NULL), 5, 0x12);   /* cx = 0x1205 */
    reset_noise();
    tick0 = l2_read_bios_tick();
    l2_dat_3509 = tick0;                               /* mov [dat_3509],dx */
    dx = tick0;
    do {                                               /* lab_3543 (push dx) */
        uint16_t pushed = dx;
        do { update_noise(); } while (!l2_vsync_active());   /* lab_3544: call check_vsync / jz */
        update_noise();
        dx = pushed;                                   /* pop dx */
        l2_set_border((dx & 1) ? 0x1 : 0xf);           /* mov bx,1 / test dl,1 / jnz / mov bl,0xf / int 0x10 */
        update_noise();
        dx = (uint16_t)(l2_read_bios_tick() - l2_dat_3509);   /* sub dx,[dat_3509] */
    } while (dx < 0xd);                                /* cmp dx,0xd / jc lab_3543 */
    return false;                                      /* ret con CF=0 (salio por el cmp) */
lab_356f:
    l2_dat_351b = (uint8_t)(l2_dat_351b + 1);
    start_tone(0x5dc, 0x425);
    if (l2_dat_351b != 0x1) goto lab_3586;
    restore_alley_buffer();
lab_3586:
    bx = l2_dat_3511;
    erase_level_object(bx);
    bx = l2_dat_3511;
    l2_obj_active[bx] = 0x1;
    if (bx >= 0xc) goto lab_35ad;
    l2_dat_3410 = (uint8_t)(l2_dat_3410 - 1);
    if (l2_dat_3410 != 0) goto lab_35ad;
    if (immune_flag != 0x0) goto lab_35ad;
    cat_caught = 0x1;
lab_35ad:
    l2_dat_3511 = (uint16_t)(l2_dat_3511 + 1);
    if (l2_dat_3511 >= 0x18) goto lab_35bb;
    goto lab_34ab;
lab_35bb:
    if (l2_dat_351b == 0x0) goto lab_35c7;
    reset_caught_objects();
    return true;                                       /* stc */
lab_35c7:
    return false;                                      /* clc */
}

/* ===== T35: update_level2_objects (level_objects.asm L872-998) ===== */
#define L2_SPRITE_A 0x3300   /* DS: l1_anim_sprite_a (bloques 0..11: 1 palabra x 6 filas, 12 bytes por frame) */
#define L2_SPRITE_B 0x3330   /* DS: l1_anim_sprite_b (objetos 12..23: 2 palabras x 2 filas, 8 bytes por frame) */

void update_level2_objects(void) {
    uint16_t bx, ax, cx, si, dx;
    uint8_t al, dl;
    dx = l2_read_bios_tick();                          /* sub ah,ah / int 0x1a */
    if (dx != l2_dat_3509) goto lab_3680;
lab_367f:
    return;
lab_3680:
    l2_dat_350b = dx;
    l2_dat_3415 = (uint16_t)(l2_dat_3415 + 1);
    bx = l2_dat_3415;
    if (bx < 0x18) goto lab_36a4;
    bx = 0;                                            /* sub bx,bx */
    l2_dat_3415 = bx;
    l2_anim_toggle = (uint16_t)(l2_anim_toggle ^ 0xc);
    l2_dat_3413 = (uint16_t)(l2_dat_3413 + 0x8);
    goto lab_36b7;
lab_36a4:
    if (bx != 0xc) goto lab_36bd;
    if (rom_id != 0xfd) goto lab_36b7;                 /* cmp byte [0x697],0xfd (rom_id) / jnz */
    if (cat_y < 0x30) goto lab_36bd;
lab_36b7:
    l2_dat_3509 = l2_dat_350b;
lab_36bd:
    si = (uint16_t)(bx << 1);                          /* db 0xd1,0xe6 = shl si,1 (el listado dice shl si,0x0) */
    if (l2_obj_active[bx] != 0x0) goto lab_367f;
    dl = l2_random_dl();
    if (dl > 0x10) goto lab_36e9;                      /* cmp dl,0x10 / ja (sin signo) */
    dl &= 0x1;
    if (dl != 0) goto lab_36d7;
    dl = (uint8_t)~dl;
lab_36d7:
    l2_dat_3417[bx] = dl;
    dl = l2_random_dl();
    dl &= 0x1;
    if (dl != 0) goto lab_36e5;
    dl = (uint8_t)~dl;
lab_36e5:
    l2_dat_342f[bx] = dl;
lab_36e9:
    cx = 0x4;
    if (bx < 0xc) goto lab_36f3;
    cx = (uint16_t)(cx & 0xff00) | (uint8_t)((uint8_t)cx >> 1);   /* db 0xd0,0xe9 = shr cl,1 (el listado dice shr cl,0x0) */
lab_36f3:
    ax = l2_obj_x[bx];                                 /* [si + l2_obj_x] */
    if (l2_dat_3417[bx] == 0x1) goto lab_370b;
    {
        bool borrow = ax < cx;                         /* sub ax,cx / jnb */
        ax = (uint16_t)(ax - cx);
        if (!borrow) goto lab_371a;
    }
    ax = 0;                                            /* sub ax,ax */
    l2_dat_3417[bx] = 0x1;
    goto lab_371a;
lab_370b:
    ax = (uint16_t)(ax + cx);
    if (ax < 0x12f) goto lab_371a;                     /* jb (sin signo) */
    ax = 0x12e;
    l2_dat_3417[bx] = 0xff;
lab_371a:
    l2_obj_x[bx] = ax;
    al = l2_obj_y[bx];
    if (l2_dat_342f[bx] == 0x1) goto lab_373c;
    al = (uint8_t)(al - 1);
    if (al >= ds_pool[L2_OBJ_INIT_Y + bx]) goto lab_3750;   /* cmp al,[init_y] / jnb */
    al = ds_pool[L2_OBJ_INIT_Y + bx];
    l2_dat_342f[bx] = 0x1;
    goto lab_3750;
lab_373c:
    al = (uint8_t)(al + 1);
    dl = (uint8_t)(ds_pool[L2_OBJ_INIT_Y + bx] + 0x18);
    if (al <= dl) goto lab_3750;                       /* cmp al,dl / jbe */
    al = dl;
    l2_dat_342f[bx] = 0xff;
lab_3750:
    l2_obj_y[bx] = al;
    dl = al;
    cx = l2_obj_x[bx];
    ax = (uint16_t)calc_cga_addr(dl, cx, NULL);
    l2_obj_cur_addr = ax;
    bx = l2_dat_3415;
    erase_level_object(bx);
    bx = l2_dat_3415;
    si = (uint16_t)(bx << 1);
    l2_obj_cga_addr[bx] = l2_obj_cur_addr;             /* mov di,[cur_addr] / mov [si+l2_obj_cga_addr],di */
    l2_obj_hit[bx] = 0x0;
    if (bx < 0xc) goto lab_379f;
    si = (uint16_t)(bx << 3);                          /* mov cl,3 / shl si,cl */
    si = (uint16_t)(si + l2_dat_3413);
    si = (uint16_t)(si & 0x18);
    si = (uint16_t)(si + L2_SPRITE_B);
    blit_to_cga(&ds_pool[si], l2_obj_cur_addr, 2, 2);  /* cx = 0x202 */
    return;
lab_379f:
    si = l2_anim_toggle;
    if (bx & 0x1) goto lab_37ac;                       /* test bl,1 / jnz */
    si = (uint16_t)(si ^ 0xc);
lab_37ac:
    if (l2_dat_3417[bx] == 0x1) goto lab_37b6;
    si = (uint16_t)(si + 0x18);
lab_37b6:
    si = (uint16_t)(si + L2_SPRITE_A);
    blit_to_cga(&ds_pool[si], l2_obj_cur_addr, 1, 6);  /* cx = 0x601 */
    return;
}

/* ===== T36: animate_level2_blocks / update_entrance_anim (level_objects.asm L1017-1099) ===== */
#define L2_BAR_SPRITES   0x2020   /* DS: level2_bar_sprites, 4 frames de 1 palabra x 4 filas (8 bytes), indice = tipo & 0x18 */
#define L2_ENTRANCE_PTRS 0x35d0   /* DS: dat_35d0, word[4] = {0x3530,0x3558,0x3580,0x35a8}: frames 2 palabras x 10 filas (0x28 bytes) */
#define L2_ENTRANCE_DST  0x15c9   /* `mov di,enemy_sprite_table_hi`: la DIRECCION del label (0x15c9) usada como offset CGA */

uint8_t  l2_block_types[L2_BLOCK_COUNT];  /* DS 0x2656 (byte[0x28]): tipo/fase de cada bloque de la franja; lo llena draw_level2_background */
uint16_t l2_dat_350d = 0;                 /* DS 0x350d (word): bloque en curso de la ola de animacion */
uint16_t l2_dat_350f = 0;                 /* DS 0x350f (word): tick de arranque de la ola */
uint16_t l2_dat_35d8 = 0;                 /* DS 0x35d8 (word): fase de la animacion de la entrada (+2 por frame) */
uint16_t l2_dat_35da = 0;                 /* DS 0x35da (word): ultimo tick de la animacion de la entrada */

void animate_level2_blocks(void) {
    uint16_t dx, ax, bx, di;
    dx = l2_read_bios_tick();                          /* sub ah,ah / int 0x1a */
    ax = dx;
    ax = (uint16_t)(ax - l2_dat_350f);
    if (ax < 0x8) goto lab_384a;                       /* cmp ax,8 / jb */
    l2_dat_350d = (uint16_t)(l2_dat_350d + 1);
    bx = l2_dat_350d;
    if (bx < 0x28) goto lab_380b;
    bx = 0;                                            /* sub bx,bx */
    l2_dat_350d = bx;
    l2_dat_350f = dx;
lab_380b:
    di = (uint16_t)(bx << 1);                          /* db 0xd1,0xe7 = shl di,1 (el listado dice shl di,0x0) */
    if (cat_y > 0x7) goto lab_3829;                    /* cmp byte [cat_y],7 / ja (sin signo) */
    ax = (uint16_t)((uint16_t)cat_x >> 2);
    ax = (uint16_t)(ax + 1);
    {
        bool borrow = ax < di;                         /* sub ax,di / jnb */
        ax = (uint16_t)(ax - di);
        if (borrow) ax = (uint16_t)~ax;                /* not ax */
    }
    if (ax < 0x4) goto lab_384a;                       /* gato cerca de este bloque: no se anima */
lab_3829:
    di = (uint16_t)(di + 0xa0);
    {
        uint8_t al = l2_block_types[bx];
        al = (uint8_t)(al + 0x8);
        l2_block_types[bx] = al;
        ax = (uint16_t)(al & 0x18);                    /* and ax,0x18 (ah no cuenta) */
    }
    ax = (uint16_t)(ax + L2_BAR_SPRITES);
    blit_to_cga(&ds_pool[ax], di, 1, 4);               /* cx = 0x401 */
lab_384a:
    return;
}

void update_entrance_anim(void) {
    uint16_t dx, ax, bx, si;
    dx = l2_read_bios_tick();
    ax = dx;
    ax = (uint16_t)(ax - l2_dat_35da);
    if (ax >= 0x6) goto lab_3860;                      /* cmp ax,6 / jnb */
    return;
lab_3860:
    l2_dat_35da = dx;
    l2_dat_35d8 = (uint16_t)(l2_dat_35d8 + 0x2);
    bx = l2_dat_35d8;
    bx = (uint16_t)(bx & 0x6);
    si = l2_ds_word((uint16_t)(L2_ENTRANCE_PTRS + bx));
    blit_to_cga(&ds_pool[si], L2_ENTRANCE_DST, 2, 10); /* cx = 0xa02 */
    /* rect A: x = 0xe4, y = 0x8a, ancho 0x10, alto cl = 0xa; rect B (gato): cat_x, cat_y, 0x18, ch = 0xe */
    if (!check_rect_collision(0xe4, 0x8a, 0x10, 0x0a, (uint16_t)cat_x, cat_y, 0x18, 0xe)) goto lab_38a3;   /* jnb */
    level_complete = (uint16_t)((level_complete & 0xff00) | 0x1);   /* mov byte [level_complete],1 */
lab_38a3:
    return;
}
