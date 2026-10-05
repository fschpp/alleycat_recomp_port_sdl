#include "level45_state.h"

/* Valores iniciales = bytes del DS original (0x3ea6..0x3edb, ver /tmp/data_segment.bin): todo en 0
 * salvo los punteros l5_obj_save_buf. */
static uint8_t save_buf_0[48], save_buf_1[48], save_buf_2[48], save_buf_3[48];

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
