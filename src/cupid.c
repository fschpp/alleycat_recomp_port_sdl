/* cupid.c — T56: reset_cupid y update_cupid (ui.asm L571-675). Ver include/cupid.h y PROGRESS.md §6az. */
#include <stdint.h>
#include "cupid.h"
#include "cat_state.h"
#include "cga.h"
#include "alley.h"
#include "score.h"
#include "gen/ds_pool.h"

uint16_t cupid_prev_x;
uint16_t cupid_anim_tick;
uint16_t cupid_arrow_x;
uint8_t  cupid_active;
uint16_t cupid_x;
uint8_t  cupid_y;
uint8_t  cupid_dir;
uint8_t  cupid_drawn;
uint16_t cupid_erase_addr;
uint16_t cupid_draw_addr;

#define DS_DAT_70B0 0x70b0          /* 8 bytes: filas iniciales (0x00,0x18,...,0xa8) */
#define DS_DAT_70B8 0x70b8          /* 10 words: columnas iniciales (0x00,0x20,...,0x120) */

void reset_cupid(void) {
    cupid_active = 0x0;
}

/* Notas de lectura del ASM:
 *  - `db 0xd0,0xe3` (shl bl,0x0 en el desensamblado) es `shl bl,1`; bl <= 9 aqui, sin acarreo a bh.
 *  - bx = dx & 0x1f deja bh = 0, y `and bl,7` tambien: [bx+tabla] indexa con bl solo.
 *  - cupid_y es un byte: `add byte [cupid_y],2` envuelve y `cmp ... ja` es sin signo.
 *  - cupid_x es un word: `sub word [cupid_x],5 / jb` sale si x < 5; `cmp ... 0x12c / jb` sin signo. */
void update_cupid(void) {
    uint16_t dx, ax, bx;
    uint8_t dl;
    dx = score_tick();                                     /* sub ah,ah / int 0x1a */
    if (dx != cupid_anim_tick) goto lab_6111;              /* cmp dx,[cupid_anim_tick] / jnz */
    return;
lab_6111:
    cupid_anim_tick = dx;
    if (check_cupid_collision() == 0) goto lab_6129;       /* call / jnb: sin choque */
    restore_alley_buffer();
    erase_cupid();
    draw_alley_foreground();
    cupid_active = 0x0;
    return;
lab_6129:
    if (cupid_active != 0x0) goto lab_619e;
lab_6130:
    bx = (uint16_t)(cga_random() & 0x1f);                  /* call random / mov bx,dx / and bx,0x1f */
    if ((uint8_t)bx < 0x10) goto lab_6166;                 /* cmp bl,0x10 / jb */
    bx = (uint8_t)(bx - 0x10);
    if ((uint8_t)bx > 0x9) goto lab_6130;                  /* cmp bl,9 / ja: reintenta */
    dl = 0x1;
    if ((uint8_t)bx < 0x5) goto lab_614f;                  /* cmp bl,5 / jb */
    dl = 0xff;
lab_614f:
    cupid_dir = dl;
    cupid_y = 0x6;
    bx = (uint8_t)(bx << 1);                               /* db 0xd0,0xe3 = shl bl,1 */
    ax = (uint16_t)(ds_pool[DS_DAT_70B8 + bx] | (ds_pool[DS_DAT_70B8 + bx + 1] << 8));   /* mov ax,[bx+dat_70b8] */
    ax = (uint16_t)(ax + 0x4);
    cupid_x = ax;
    goto lab_6188;
lab_6166:
    ax = 0xc;
    dl = 0x1;
    if ((bx & 0x8) == 0) goto lab_6175;                    /* test bl,8 / jz */
    ax = 0x120;
    dl = 0xff;
lab_6175:
    cupid_x = ax;
    cupid_dir = dl;
    bx &= 0x7;
    cupid_y = (uint8_t)(ds_pool[DS_DAT_70B0 + bx] + 0x8);  /* mov al,[bx+dat_70b0] / add al,8 */
lab_6188:
    cupid_active = 0x1;
    cupid_drawn = 0x1;
    cupid_arrow_x = 0x0;
    cupid_prev_x = 0xffff;
lab_619e:
    if (cupid_arrow_x >= 0xa0) goto lab_61ab;              /* cmp ...,0xa0 / jnb */
    cupid_arrow_x = (uint16_t)(cupid_arrow_x + 0x4);
lab_61ab:
    cupid_y = (uint8_t)(cupid_y + 0x2);
    if (cupid_y > 0xbf) goto lab_61d4;                     /* ja */
    if (cupid_dir == 0x1) goto lab_61c7;                   /* cmp byte [cupid_dir],1 / jz */
    if (cupid_x < 0x5) goto lab_61d4;                      /* sub word [cupid_x],5 / jb */
    cupid_x = (uint16_t)(cupid_x - 0x5);
    goto lab_61dd;
lab_61c7:
    cupid_x = (uint16_t)(cupid_x + 0x5);
    if (cupid_x < 0x12c) goto lab_61dd;                    /* cmp word [cupid_x],0x12c / jb */
lab_61d4:
    cupid_active = 0x0;
    erase_cupid();
    return;
lab_61dd:
    {
        uint8_t shift;
        cupid_draw_addr = (uint16_t)calc_cga_addr(cupid_y, cupid_x, &shift);   /* mov cx,[cupid_x] / mov dl,[cupid_y] / call */
    }
    if (check_cupid_collision() != 0) goto lab_61d4;       /* jb */
    cupid_toggle_window();
    erase_cupid();
    draw_cupid();
}
