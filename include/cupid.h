#ifndef CUPID_H
#define CUPID_H
#include <stdint.h>
#include <stddef.h>

/* T56: enemigo volador del nivel 7 (ui.asm L571-675: reset_cupid, update_cupid). PROGRESS.md §6az.
 * Estado en DS 0x70ec..0x70fb (tipos del original: byte = uint8_t, word = uint16_t). */
extern uint16_t cupid_prev_x;       /* DS 0x70ec: 0xffff al aparecer */
extern uint16_t cupid_anim_tick;    /* DS 0x70ee: ultimo tick procesado (una actualizacion por tick BIOS) */
extern uint16_t cupid_arrow_x;      /* DS 0x70f0: 0..0xa0 en pasos de 4; elige el frame del sprite (T57) */
extern uint8_t  cupid_active;       /* DS 0x70f2: 0 = libre */
extern uint16_t cupid_x;            /* DS 0x70f3 (word) */
extern uint8_t  cupid_y;            /* DS 0x70f5 (byte) */
extern uint8_t  cupid_dir;          /* DS 0x70f6: 0x01 derecha, 0xff izquierda */
extern uint8_t  cupid_drawn;        /* DS 0x70f7 */
extern uint16_t cupid_erase_addr;   /* DS 0x70f8: lo fija draw_cupid (T57) */
extern uint16_t cupid_draw_addr;    /* DS 0x70fa: calc_cga_addr(cupid_y, cupid_x) */

void reset_cupid(void);             /* cupid_active = 0 */
void update_cupid(void);            /* una vez por tick: colision, aparicion aleatoria, movimiento, ventanas */

/* --- T57 (ui.asm L676-791). check_cupid_collision devuelve el flag CF del original (1 = choque con el gato). --- */
void draw_cupid(void);
void erase_cupid(void);
void cupid_toggle_window(void);
int  check_cupid_collision(void);
#endif
