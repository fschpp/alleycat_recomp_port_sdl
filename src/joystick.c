/* joystick.c — poll_joystick y decode_joystick_axis (input.asm L4-89). T60. Ver include/joystick.h y PROGRESS.md §6bc.
 * Sin SDL: el puerto de juegos entra por joy_port_fn / joy_fire_fn y el tiempo por read_pit_timer. */
#include <stdint.h>
#include <stddef.h>
#include "joystick.h"
#include "cat_state.h"
#include "input.h"
#include "score.h"
#include "speaker.h"
#include "ui.h"

void (*joy_fire_fn)(void);
uint16_t joy_timer;
uint8_t  joy_pending;
uint16_t joy_last_tick;

/* decode_joystick_axis (L91-106). read_pit_timer cuenta hacia atras, asi que ax - joy_timer = -(tiempo transcurrido):
 * bx < 0xf5e6 (> 0xa1a cuentas) -> 1; 0xf5e6 <= bx < 0xfafa -> 0; bx >= 0xfafa (<= 0x506 cuentas) -> 0xff. */
uint8_t decode_joystick_axis(void) {
    uint16_t bx = (uint16_t)(read_pit_timer() - joy_timer);   /* push ax / call / sub ax,[joy_timer] / mov bx,ax / pop ax */
    if (bx >= 0xf5e6) goto lab_12b5;                       /* cmp bx,0xf5e6 / jnc */
    return 0x1;                                            /* mov bl,1 */
lab_12b5:
    if (bx >= 0xfafa) goto lab_12be;                       /* cmp bx,0xfafa / jnc */
    return 0x0;                                            /* sub bl,bl */
lab_12be:
    return 0xff;                                           /* mov bl,0xff */
}

/* poll_joystick (L4-89). */
void poll_joystick(void) {
    uint16_t ax, dx, cx;
    uint8_t al;
    dx = score_tick();                                     /* sub ah,ah / int 0x1a */
    ax = (uint16_t)(dx - joy_last_tick);                   /* mov ax,dx / sub ax,[joy_last_tick] */
    if (ax >= 0x2) goto lab_1210;                          /* cmp ax,2 / jnc */
    return;
lab_1210:
    joy_last_tick = dx;
    if (use_joystick != 0x0) goto lab_122e;                /* cmp byte [use_joystick],0 / jnz */
    input_poll();                                          /* call read_keyboard_dirs (port: SDL) */
    dx = read_pit_timer();                                 /* call read_pit_timer / mov dx,ax */
lab_1223:
    ax = (uint16_t)(read_pit_timer() - dx);
    if (ax < 0xf8ed) goto lab_1223;                        /* cmp ax,0xf8ed / jc: espera a que el PIT cambie */
    return;
lab_122e:
    al = joy_port_fn ? joy_port_fn() : 0xff;               /* mov dx,0x201 / in al,dx */
    al &= 0x10;
    joy_button = al;
    joy_pending = 0x3;
    joy_timer = read_pit_timer();
    if (joy_fire_fn) joy_fire_fn();                        /* out dx,al: dispara el monoestable */
    cx = 0x7d0;
lab_1246:
    al = joy_port_fn ? joy_port_fn() : 0xff;               /* in al,dx */
    if ((al & 0x1) != 0) goto lab_125e;                    /* test al,1 / jnz */
    if ((joy_pending & 0x1) == 0) goto lab_125e;           /* test [joy_pending],1 / jz */
    joy_pending &= 0xfe;
    input_horizontal = (int8_t)decode_joystick_axis();     /* call decode / mov [input_horizontal],bl */
lab_125e:
    if ((al & 0x2) != 0) goto lab_1275;                    /* test al,2 / jnz (decode conserva ax) */
    if ((joy_pending & 0x2) == 0) goto lab_1275;
    joy_pending &= 0xfd;
    input_vertical = (int8_t)decode_joystick_axis();
lab_1275:
    if ((joy_pending & 0x3) == 0) goto lab_12a0;           /* test [joy_pending],3 / jz */
    ax = (uint16_t)(read_pit_timer() - joy_timer);
    /* cmp ax,0x1964 / loopnz: ZF viene del cmp (solo igual si ax == 0x1964 exacto); sale tambien al agotar cx */
    cx--;
    if (cx != 0 && ax != 0x1964) goto lab_1246;
    if ((joy_pending & 0x1) == 0) goto lab_1294;           /* eje X sin respuesta: -1 */
    input_horizontal = (int8_t)0xff;
lab_1294:
    if ((joy_pending & 0x2) == 0) goto lab_12a0;
    input_vertical = (int8_t)0xff;
lab_12a0:
    return;
}
