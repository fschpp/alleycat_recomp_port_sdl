#include "level6.h"
#include "cat_state.h"
#include "cga.h"
#include "alley.h"
#include "level_collision.h"
#include "enemy.h"
#include "sound.h"
#include "level_objects.h"
#include <time.h>
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
#define L6_DAT_43E1       0x43e1   /* word[12]: X del objeto (dato fijo del DS; solo lectura) */
#define L6_DAT_43F9       0x43f9   /* word[12]: Y del objeto (byte bajo; dato fijo del DS; solo lectura) */
#define L6_DAT_4100       0x4100   /* words: sprite de 1 word del indicador de alerta */
#define L6_DAT_44DC       0x44dc   /* word[8] por dificultad: ticks entre pases {18,16,15,14,13,12,11,10} (solo lectura) */
#define L6_DAT_44EC       0x44ec   /* word[8] por dificultad: distancia X maxima {40,50,60,70,85,80,85,90} (solo lectura) */
#define L6_SPRITE_B       0x429c   /* offset DS del sprite de objeto "B" (el otro es 0x431e) */

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
uint8_t  l6_dat_44d9 = 0;
uint16_t l6_dat_44d7 = 0;
uint8_t  l6_dat_44fc = 0;
int32_t  l6_tick_override = -1;
uint16_t l6_dat_44da = 0;

static uint16_t l6_tracker_save[30];   /* DS 0x43a0..0x43dc = 0x3c bytes = 3 words x 10 filas */

/* `sub ah,ah / int 0x1a` -> dx (mismo sustituto que level4.c, 55 ms por tick). */
static uint16_t read_bios_tick(void) {
    if (l6_tick_override >= 0) return (uint16_t)l6_tick_override;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms / 55);
}

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

/* ---- T30: helpers B (level_objects.asm L2741-2816). `slot` = bx/2 del ASM. ---- */

void prepare_l6_erase(void) {
    if (l6_dat_44bd == 0x0) goto lab_489d;             /* cmp byte [dat_44bd],0 / jz */
    erase_l6_tracker();
    l6_dat_44bd = 0x0;
    return;
lab_489d:
    restore_alley_buffer();
}

void clear_l6_object(void) {
    uint8_t scratch[0x41 * 2];                         /* rep stosw: cx=0x41 words de 0xaaaa en DS:0xe */
    memset(scratch, 0xaa, sizeof scratch);
    blit_to_cga(scratch, l6_dat_44da, 5, 13);          /* cx=0xd05 (5 words x 13 filas) = 0x41 words */
}

void refresh_l6_display(void) {
    if (l6_dat_44d9 == 0x0) return;                    /* jz lab_48d2 */
    if (l6_dat_44bd == 0x0) goto lab_48d3;
    draw_l6_tracker();
    return;                                            /* lab_48d2: ret */
lab_48d3:
    draw_alley_foreground();
}

void check_l6_proximity(uint16_t slot) {
    uint16_t bx = (uint16_t)(slot * 2);                /* el ASM recibe bx = 2*slot */
    l6_dat_44d9 = 0x0;
    uint16_t ax = ds_word((uint16_t)(L6_DAT_43E1 + bx));
    uint16_t dx = ds_word((uint16_t)(L6_DAT_43F9 + bx));   /* dl = Y del objeto; dh se pisa con cat_y */
    ax = (uint16_t)(ax - 0x14);                        /* db 0x2d,0x14,0x00 = sub ax,0x14 */
    /* si=0x28, bx=cat_x, dh=cat_y, cx=0x0e06 (cl=6 alto A, ch=0xe alto B), di=0x18 */
    if (!check_rect_collision((int16_t)ax, (uint8_t)dx, 0x28, 0x06,
                              (uint16_t)cat_x, cat_y, 0x18, 0x0e)) return;   /* jnb lab_4915 */
    l6_dat_44d9 = 0x1;
    /* lab_4902: `call check_vsync / jz lab_4902` espera el retrace; en el port es un no-op (no espera). */
    if (l6_dat_44bd == 0x0) goto lab_4912;
    erase_l6_tracker();
    return;
lab_4912:
    restore_alley_buffer();
}

void draw_l6_alert(uint16_t slot) {
    uint16_t bx = (uint16_t)(slot * 2);
    uint16_t ax = ds_word((uint16_t)(L6_OBJ_X + bx));
    uint16_t si = l6_obj_state[slot];
    si = (uint16_t)(si << 1);                          /* db 0xd1,0xe6 = shl si,1 (listing: shl si,0x0) */
    si = (uint16_t)(si + L6_DAT_4100);
    ax = (uint16_t)(ax + 0xa7);
    if (ds_word((uint16_t)(L6_OBJ_SPRITE_PTR + bx)) == L6_SPRITE_B) goto lab_4935;   /* cmp word,0x429c / jz */
    ax = (uint16_t)(ax - 0x6);                         /* db 0x2d,0x06,0x00 = sub ax,6 */
    si = (uint16_t)(si + 0x6);
lab_4935:
    blit_to_cga(&ds_pool[si], ax, 1, 1);               /* cx=0x101: 1 word x 1 fila */
}

/* ---- T31: update_level6_timing (level_objects.asm L2666-2740). ---- */

void update_level6_timing(void) {
    uint16_t cx, bx, ax, slot;
    uint16_t dx = read_bios_tick();                    /* sub ah,ah / int 0x1a */
    ax = dx;                                           /* mov ax,dx */
    ax = (uint16_t)(ax - l6_dat_44d7);
    uint16_t si = (uint16_t)(difficulty_level << 1);   /* db 0xd1,0xe6 = shl si,1 (listing: shl si,0x0) */
    if (ax > ds_word((uint16_t)(L6_DAT_44DC + si))) goto lab_47ed;   /* ja (sin signo): el resto del tick se pasa de largo */
lab_47ec:
    return;
lab_47ed:
    l6_dat_44d7 = dx;
    if (enemy_active != 0x0) goto lab_47ec;            /* jnz: ya hay perro activo, solo se actualiza el tick */
    l6_dat_44fc = 0x0;
    cx = 0xc;
lab_4800:
    bx = (uint16_t)(cx - 1);                           /* mov bx,cx / dec bx */
    bx = (uint16_t)((bx & 0xff00) | (uint8_t)(bx << 1));   /* db 0xd0,0xe3 = shl bl,1 (solo bl; bx <= 22) */
    slot = (uint16_t)(bx >> 1);
    if (l6_obj_flag[slot] == 0x0) goto lab_487d;       /* cmp word [bx+dat_4441],0 / jz */
    ax = ds_word((uint16_t)(L6_DAT_43F9 + bx));        /* al = Y del objeto */
    if ((uint8_t)ax != cat_y) goto lab_485d;           /* cmp al,[cat_y] / jnz */
    ax = ds_word((uint16_t)(L6_DAT_43E1 + bx));
    {
        bool borrow = ax < (uint16_t)cat_x;            /* sub ax,[cat_x] / jnb */
        ax = (uint16_t)(ax - (uint16_t)cat_x);
        if (borrow) ax = (uint16_t)~ax;                /* not ax: |dx| aprox. (|d|-1) */
    }
    si = (uint16_t)(difficulty_level << 1);
    if (ax > ds_word((uint16_t)(L6_DAT_44EC + si))) goto lab_485d;   /* ja (sin signo) */
    if (l6_obj_state[slot] < 0x2) goto lab_484c;       /* cmp word [bx+l6_obj_state],2 / jb */
    l6_dat_44da = ds_word((uint16_t)(L6_OBJ_X + bx));
    prepare_l6_erase();
    clear_l6_object();
    draw_l1_thrown();                                  /* call draw_thrown_sprite */
    draw_alley_foreground();
    activate_enemy_chase();
    return;                                            /* sin explosion ni mas slots */
lab_484c:
    l6_obj_state[slot] = (uint16_t)(l6_obj_state[slot] + 1);
    if (l6_obj_state[slot] < 0x2) goto lab_4870;       /* jb */
    l6_dat_44fc = (uint8_t)(l6_dat_44fc + 1);
    goto lab_4870;
lab_485d:
    if (l6_obj_state[slot] == 0x0) goto lab_487d;      /* jz: slot inactivo en este pase */
    {
        uint8_t dl = (uint8_t)(cga_random() & 0xff);   /* call random (dl = byte bajo del seed) */
        if (dl > 0x38) goto lab_4870;                  /* ja (sin signo) */
    }
    l6_obj_state[slot] = (uint16_t)(l6_obj_state[slot] - 1);
lab_4870:
    /* push cx / push bx / call check_l6_proximity / pop bx / call draw_l6_alert / call refresh_l6_display / pop cx */
    check_l6_proximity(slot);
    draw_l6_alert(slot);
    refresh_l6_display();
lab_487d:
    cx = (uint16_t)(cx - 1);                           /* loop lab_488a -> jmp near lab_4800 */
    if (cx != 0) goto lab_4800;
    if (l6_dat_44fc == 0x0) goto lab_4889;
    play_explosion_effect();
lab_4889:
    return;
}
