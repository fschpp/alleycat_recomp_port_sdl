#ifndef LEVEL45_STATE_H
#define LEVEL45_STATE_H
#include <stdint.h>

/* Estado compartido de los niveles 4 y 5 (T21, PROGRESS.md §6r): el nivel 4 REUTILIZA las variables
 * `l5_obj_*` del nivel 5 (level_objects.asm). 4 objetos; los offsets DS están verificados con
 * /tmp/data_segment_labels.txt (coinciden con tareas.md) y las tablas indexadas por `bx` son byte[4],
 * las indexadas por `si = 2*bx` son word[4]. */
extern uint16_t l5_obj_cga_addr[4];  /* DS 0x3ea6 (words): dirección CGA donde se dibujó el objeto */
extern uint8_t  l5_obj_active[4];    /* DS 0x3eae */
extern uint8_t  l5_obj_hit[4];       /* DS 0x3eb2 */
extern uint8_t  l5_obj_anim[4];      /* DS 0x3eb6 */
extern uint16_t l5_obj_frame[4];     /* DS 0x3eba (words): en el nivel 4 = índice de plataforma 0..15 */
extern uint8_t *l5_obj_save_buf[4];  /* DS 0x3ec2 (words = punteros): 4 buffers de 48 bytes (2 words x 12 filas),
                                      * en el DS 0x3de6/0x3e16/0x3e46/0x3e76 */
extern uint16_t l5_obj_dims[4];      /* DS 0x3ecc (words): X del objeto (nivel 4: x_table + 8) */
extern uint8_t  l5_obj_y_pos[4];     /* DS 0x3ed4 */
extern uint8_t  l5_obj_count;        /* DS 0x3ed8 */
extern uint8_t  l5_anim_delay;       /* DS 0x3ed9: reintentos restantes de randomize_l4_pos */
extern uint16_t l5_obj_index;        /* DS 0x3eda (word) */
extern uint16_t l5_last_tick;       /* DS 0x3edc: último tick BIOS procesado (T23) */
extern uint16_t l5_obj_sprite_ptr;  /* DS 0x3eca: OFFSET en el DS del sprite (no un dato), 0 = no dibujar (T23) */
#endif
