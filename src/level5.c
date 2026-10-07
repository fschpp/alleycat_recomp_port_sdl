/* level5.c — nivel 5, helpers A (T25). Ver include/level5.h y PROGRESS.md §6v. */
#include "bios_clock.h"
#include "level5.h"
#include "level_collision.h"
#include "cat_state.h"
#include "cga.h"
#include "gen/ds_pool.h"
#include "sound.h"
#include "speaker.h"
#include "alley.h"
#include <time.h>

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

bool check_l5_thrown(void) {
    /* ax=dat_40b2, dl=dat_40b4, si=8 | bx=thrown_obj_x, dh=thrown_obj_y, di=0x10, cx=0x1e05 */
    if (!check_rect_collision((int16_t)l5_dat_40b2, l5_dat_40b4, 0x8, 0x5,
                              (uint16_t)thrown_obj_x, thrown_obj_y, 0x10, 0x1e)) return false;
    l5_dat_40b8 = 0xff;                         /* el `mov` no toca CF: sale con CF=1 */
    return true;
}

/* ---- T26: helpers B ---- */
#define L5_PERCH_SPRITE 0x3fbe  /* 3 words x 16 filas = 96 bytes en el DS */

uint16_t l5_dat_40a6 = 0;
/* DS 0x401e..0x40a5 = 0x88 bytes = 68 words. El perch usa 3x16 = 48 words; el sprite de lo que queda
 * al aterrizar (T28, 4 words x 17 filas = 68 words) usa el buffer entero (dat_401e + 0x88 = dat_40a6). */
static uint16_t l5_perch_save[68];

bool check_l5_perch_hit(void) {
    /* ax=dat_40a8, dl=dat_40aa, si=0x18 | bx=cat_x, dh=cat_y, di=si=0x18, cx=0x0e10 (cl=0x10, ch=0x0e) */
    return check_rect_collision((int16_t)l5_dat_40a8, l5_dat_40aa, 0x18, 0x10,
                                (uint16_t)cat_x, cat_y, 0x18, 0x0e);
}

void draw_l5_perch(void) {
    l5_dat_40a6 = l5_dat_40ab;
    blit_masked(&ds_pool[L5_PERCH_SPRITE], l5_dat_40ab, 3, 16, l5_perch_save);
}

void erase_l5_perch(void) {
    blit_to_cga((const uint8_t *)l5_perch_save, l5_dat_40a6, 3, 16);
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

/* ---- T27: update_level5_anim ---- */

uint16_t l5_dat_40b5 = 0;
uint8_t  l5_dat_40b7 = 0;
uint16_t l5_dat_40ba = 0;
uint16_t l5_dat_40bc = 0;
uint16_t l5_dat_40be = 0;
uint8_t  l5_dat_40ff = 0;
uint16_t l5_dat_3f2c[5];
int32_t  l5_tick_override = -1;

/* `sub ah,ah / int 0x1a` -> dx (mismo sustituto que en el resto del port). */
static uint16_t read_bios_tick(void) {
    if (l5_tick_override >= 0) {
        uint16_t t = (uint16_t)l5_tick_override;
        l5_tick_override += l5_tick_advance;
        return t;
    }
    return bios_clock_read();
}

/* Offsets DS (tools/asm_label.sh). Extensiones confirmadas por cómo se indexan (no por el label):
 *   dat_40ce word[8] (si = 2*difficulty)   {46,92,140,140,160,170,180,180} = umbral de persecución
 *   dat_40de word[11] (si = 2*bx, bx<=0xa)  X objetivo;   dat_40f4 byte[11] (bx<=0xa)  Y objetivo
 *   dat_40c0 word[4] (bx = dat_40be & 6)    punteros a sprites 1x5 (0x3ef0,0x3efa,0x3f04,0x3efa); +0x1e = espejo
 * Verificado: los punteros distan 10 bytes (1 word x 5 filas) y 0x3f04+0x1e+10 = 0x3f2c = dat_3f2c. */
#define L5_THRESH_TABLE 0x40ce
#define L5_TARGET_X     0x40de
#define L5_TARGET_Y     0x40f4
#define L5_FRAME_PTRS   0x40c0

static uint16_t ds_word(uint32_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

/* `call random` deja el resultado en dx; el código solo mira dl salvo `and dx,0xff` (= dl). */
static uint8_t rnd_dl(void) { return (uint8_t)(cga_random() & 0xff); }

void update_level5_anim(void) {
    uint16_t dx = read_bios_tick();                    /* sub ah,ah / int 0x1a */
    uint8_t dl, al;
    uint16_t ax, bx, si;
    if (dx == l5_dat_40b5) return;                     /* lab_434a */
    l5_dat_40ff = (uint8_t)(l5_dat_40ff + 1);
    if ((l5_dat_40ff & 0x3) != 0) l5_dat_40b5 = dx;    /* test/jz: cada 4.º tick NO guarda el tick */
    if (l5_dat_40aa < 0xa4) return;                    /* lab_435a: jb lab_434a */
    check_l5_cat_catch();
    if (check_l5_thrown()) return;                     /* jb lab_434a (CF=1 si colisión) */
    dl = rnd_dl();
    if (dl > 0x30) goto lab_439c;                      /* ja */
    calc_l5_direction();
    si = (uint16_t)(difficulty_level << 1);            /* db 0xd1,0xe6 = shl si,1 */
    ax = ds_word(L5_THRESH_TABLE + si);
    if (l5_dat_40cc > ax) goto lab_439c;               /* ja (sin signo) */
    play_random_chirp();
    l5_dat_40c8 = 0xff;
    l5_dat_40b7 = l5_dat_40ca;
    l5_dat_40b8 = l5_dat_40cb;
    goto lab_442e;

lab_439c:
    if (l5_dat_40c8 > 0xa) goto lab_4402;              /* ja lab_43b1 -> jmp lab_4402 */
    dl = rnd_dl();
    if (dl > 0x6) goto lab_43b4;                       /* ja */
    l5_dat_40c8 = 0xff;
    goto lab_4402;                                     /* lab_43b1 */

lab_43b4:
    bx = l5_dat_40c8;
    si = (uint16_t)(bx << 1);
    dl = 0;
    ax = (uint16_t)(l5_dat_40b2 & 0xffc);
    {
        uint16_t t = ds_word(L5_TARGET_X + si);
        if (ax != t) { dl = 0x1; if (!(ax < t)) dl = 0xff; }   /* jz / inc dl (no toca CF) / jb */
    }
    l5_dat_40b7 = dl;
    dl = 0;
    al = (uint8_t)(l5_dat_40b4 & 0xfe);
    {
        uint8_t t = ds_pool[L5_TARGET_Y + bx];
        if (al != t) { dl = 0x1; if (!(al < t)) dl = 0xff; }
    }
    l5_dat_40b8 = dl;
    if ((uint8_t)(dl | l5_dat_40b7) != 0) goto lab_442e;       /* or dl,[dat_40b7] / jnz */
    dl = rnd_dl();
    if (dl > 0x10) goto lab_442e;                      /* ja */
    l5_dat_40c8 = 0xff;
    play_random_chirp();
    /* cae a lab_4402 */

lab_4402:
    dl = rnd_dl();
    if (dl > 0x30) goto lab_4423;                      /* ja */
    dl = (uint8_t)(dl & 0x1);
    if (dl == 0) dl = 0xff;                            /* jnz lab_4411 */
    l5_dat_40b7 = dl;
    dl = rnd_dl();
    dl = (uint8_t)(dl & 0x1);
    if (dl == 0) dl = 0xff;                            /* jnz lab_441f */
    l5_dat_40b8 = dl;

lab_4423:
    l5_dat_40c8 = (uint16_t)(cga_random() & 0xff);     /* call random / and dx,0xff */

lab_442e:
    al = l5_dat_40b4;
    if (l5_dat_40b8 < 0x1) goto lab_4459;              /* jb */
    if (l5_dat_40b8 != 0x1) goto lab_4449;             /* jnz */
    al = (uint8_t)(al + 0x2);
    if (al < 0xa8) goto lab_4456;                      /* jb */
    al = 0xa7;
    l5_dat_40b8 = 0xff;
    goto lab_4456;
lab_4449:
    al = (uint8_t)(al - 0x2);
    if (al >= 0x30) goto lab_4456;                     /* jnb */
    al = 0x30;
    l5_dat_40b8 = 0x1;
lab_4456:
    l5_dat_40b4 = al;

lab_4459:
    ax = l5_dat_40b2;
    if (l5_dat_40b7 < 0x1) goto lab_4486;              /* jb */
    if (l5_dat_40b7 != 0x1) goto lab_4477;             /* jnz */
    ax = (uint16_t)(ax + 0x4);
    if (ax < 0x136) goto lab_4483;                     /* jb */
    ax = 0x135;
    l5_dat_40b7 = 0xff;
    goto lab_4483;
lab_4477:
    {
        bool borrow = ax < 0x4;
        ax = (uint16_t)(ax - 0x4);
        if (!borrow) goto lab_4483;                    /* jnb */
    }
    ax = 0;
    l5_dat_40b7 = 0x1;
lab_4483:
    l5_dat_40b2 = ax;

lab_4486:
    check_l5_cat_catch();
    l5_dat_40bc = (uint16_t)calc_cga_addr(l5_dat_40b4, l5_dat_40b2, NULL);
    if (l5_dat_40b9 == 0)                              /* borra el dibujo anterior */
        blit_to_cga((const uint8_t *)l5_dat_3f2c, l5_dat_40ba, 1, 5);
    if (check_l5_thrown()) return;                     /* lab_44b0: jb lab_44e6 */
    l5_dat_40b9 = 0x0;
    l5_dat_40be = (uint16_t)(l5_dat_40be + 0x2);
    bx = (uint16_t)(l5_dat_40be & 0x6);
    si = ds_word(L5_FRAME_PTRS + bx);
    if (l5_dat_40b7 == 0xff) si = (uint16_t)(si + 0x1e);
    l5_dat_40ba = l5_dat_40bc;
    blit_transparent(&ds_pool[si], l5_dat_40bc, 1, 5, l5_dat_3f2c);
}

/* ---- T28: update_level5_objects (level_objects.asm L2438-2602) ---- */

uint16_t l5_dat_40ad = 0;   /* DS 0x40ad (word): ultimo tick BIOS procesado por update_level5_objects */
uint8_t  l5_dat_40b0 = 0;   /* DS 0x40b0 (byte): direccion del empujon al gato (1 = izq., 0xff = der.) */
int32_t  l5_tick_advance = 0; /* hook de pruebas: se suma a l5_tick_override tras cada lectura */

#define L5_LANDED_SPRITE 0x3f36   /* 4 words x 17 filas = 0x88 bytes (dat_3f36..dat_3fbe) */

void update_level5_objects(void) {
    uint16_t dx = read_bios_tick();                    /* sub ah,ah / int 0x1a */
    uint16_t ax;
    uint8_t al, dl;
    uint16_t cx;
    if (dx != l5_dat_40ad) goto lab_45b6;
lab_45b5:
    return;
lab_45b6:
    l5_dat_40ad = dx;
    if (l5_dat_40aa >= 0xa4) goto lab_45b5;            /* jnb */
    if (!check_l5_thrown_near()) goto lab_45d6;        /* jnb */
    if (!check_l5_landing()) goto lab_45b5;            /* jnb */
    in_level_mode = 0x1;
    transition_timer = 0x10;
    return;
lab_45d6:
    if (!check_l5_perch_hit()) goto lab_4649;          /* jnb */
    if (l5_dat_40af != 0x0) goto lab_45fa;
    al = (uint8_t)scroll_direction;                    /* mov al,[0x56e] */
    if (al != 0x0) goto lab_45f7;
    al++;
    if (l5_dat_40a8 > (uint16_t)cat_x) goto lab_45f7;  /* ja (sin signo) */
    al = 0xff;
lab_45f7:
    l5_dat_40b0 = al;
lab_45fa:
    l5_dat_40af = 0x1;
    cx = 0x20;
lab_4602:
    ax = (uint16_t)cat_x;
    dl = 0x1;
    if (l5_dat_40b0 != 0x1) goto lab_4615;
    ax = (uint16_t)(ax - 0x8);                         /* db 0x2d,0x08,0x00 = sub ax,8 */
    dl = 0xff;
    goto lab_4618;
lab_4615:
    ax = (uint16_t)(ax + 0x8);                         /* db 0x05,0x08,0x00 = add ax,8 */
lab_4618:
    cat_x = (int16_t)ax;
    scroll_direction = (int8_t)dl;                     /* mov [0x56e],dl */
    al = cat_y;
    if ((uint8_t)in_level_mode < 0x1) goto lab_4639;   /* jb */
    if ((uint8_t)in_level_mode != 0x1) goto lab_462f;  /* jnz (jb no toca flags) */
    al = (uint8_t)(al - 0x3);
    goto lab_4631;
lab_462f:
    al = (uint8_t)(al + 0x3);
lab_4631:
    cat_y = al;
    al = (uint8_t)(al + 0x32);
    cat_y_bottom = al;                                 /* mov [0x57c],al */
lab_4639:
    if (!check_l5_perch_hit()) goto lab_4642;          /* push cx / call / pop cx / jnb */
    if (--cx != 0) goto lab_4602;                      /* loop */
lab_4642:
    restore_alley_buffer();
    save_cat_background();
lab_4648:
    return;
lab_4649:
    if (l5_dat_40b1 != 0x0) goto lab_46a2;
    if (l5_dat_40af == 0x0) goto lab_4648;
    ax = l5_dat_40a8;
    if (l5_dat_40b0 != 0x1) goto lab_4666;
    ax = (uint16_t)(ax + 0x8);                         /* db 0x05,0x08,0x00 */
    goto lab_4669;
lab_4666:
    ax = (uint16_t)(ax - 0x8);                         /* db 0x2d,0x08,0x00 */
lab_4669:
    l5_dat_40a8 = ax;
    if (check_l5_perch_hit()) return;                  /* jnb lab_4672 / ret */
    start_tone(0xc00, 0xb54);
    l5_dat_40af = 0x0;
    l5_dat_40ab = (uint16_t)calc_cga_addr(l5_dat_40aa, l5_dat_40a8, NULL);
    erase_l5_perch();
    draw_l5_perch();
    ax = l5_dat_40a8;
    if (ax < 0x78) goto lab_46a2;                      /* db 0x3d,0x78,0x00 = cmp ax,0x78 / jb */
    if (ax > 0xa8) goto lab_46a2;                      /* ja */
    return;
lab_46a2:
    l5_dat_40b1 = 0x1;
    if (enemy_chasing == 0x0) goto lab_46be;
    if (check_l5_landing()) {                          /* jnb lab_46bd */
        in_level_mode = 0x1;
        transition_timer = 0x10;
    }
    return;
lab_46be:
    /* Espera bloqueante (cutscene de una vez): baja el perch de a 5 filas por tick BIOS. */
    dx = read_bios_tick();
    if (dx == l5_dat_40ad) goto lab_46a2;              /* jz (vuelve a lab_46a2, como el original) */
    l5_dat_40ad = dx;
    if (sound_enabled == 0x0) goto lab_46ec;           /* cmp byte [0x0],0 */
    pit_out_43(0xb6);
    ax = (uint16_t)l5_dat_40aa;                        /* mov al,[40aa] / sub ah,ah */
    ax = (uint16_t)(ax << 1);                          /* db 0xd1,0xe0 = shl ax,1 (x2) */
    ax = (uint16_t)(ax << 1);
    pit_ch2_out((uint8_t)(ax & 0xff));
    pit_ch2_out((uint8_t)(ax >> 8));                   /* mov al,ah / out 0x42 */
    port61_out((uint8_t)(port61_in() | 0x3));
lab_46ec:
    dl = l5_dat_40aa;
    if (dl >= 0xa4) goto lab_470e;                     /* jnb */
    dl = (uint8_t)(dl + 0x5);
    l5_dat_40aa = dl;
    l5_dat_40ab = (uint16_t)calc_cga_addr(dl, l5_dat_40a8, NULL);
    erase_l5_perch();
    draw_l5_perch();
    goto lab_46be;
lab_470e:
    silence_speaker();
    erase_l5_perch();
    l5_dat_40a6 = (uint16_t)(l5_dat_40a6 - 1);         /* dec word [40a6] */
    blit_masked(&ds_pool[L5_LANDED_SPRITE], l5_dat_40a6, 4, 17, l5_perch_save);  /* bp=dat_401e, cx=0x1104 */
    l5_dat_40b2 = l5_dat_40a8;
    l5_dat_40b4 = l5_dat_40aa;
    calc_l5_direction();
    l5_dat_40b7 = l5_dat_40ca;
}
