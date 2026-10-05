#include "level45_state.h"

/* Valores iniciales = bytes del DS original (0x3ea6..0x3edb, ver /tmp/data_segment.bin): todo en 0
 * salvo los punteros l5_obj_save_buf. */
static _Alignas(2) uint8_t save_buf_0[48], save_buf_1[48], save_buf_2[48], save_buf_3[48];

uint16_t l5_obj_cga_addr[4];
uint8_t  l5_obj_active[4];
uint8_t  l5_obj_hit[4];
uint8_t  l5_obj_anim[4];
uint16_t l5_obj_frame[4];
uint8_t *l5_obj_save_buf[4] = { save_buf_0, save_buf_1, save_buf_2, save_buf_3 };
uint16_t l5_obj_dims[4];
uint8_t  l5_obj_y_pos[4];
uint8_t  l5_obj_count;
uint8_t  l5_anim_delay;
uint16_t l5_obj_index;
uint16_t l5_last_tick;        /* DS 0x3edc (word), inicial 0 */
uint16_t l5_obj_sprite_ptr;   /* DS 0x3eca (word): offset DS del sprite elegido (0 = no dibujar), inicial 0 */

/* T24: valores iniciales = 0 (verificado en /tmp/data_segment.bin: 0x39e1..0x39e9 y 0x3d03..0x3d19 son 0) */
uint16_t l3_door_cga_1, l3_door_cga_2, l3_door_cga_3, l3_door_sprite_base;
uint16_t l4_obj_cur_x;
uint8_t  l4_obj_cur_y;
uint16_t l4_last_tick;
uint16_t l4_dat_3d18;
