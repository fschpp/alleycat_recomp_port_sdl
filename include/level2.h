#ifndef LEVEL2_H
#define LEVEL2_H
#include <stdint.h>
#include <stdbool.h>

/* Nivel 2, helpers y datos (level_objects.asm L806-871 y L1001-1016) — T33, PROGRESS.md §6ad.
 * Todos los arrays tienen 24 entradas (slot 0..23; 0..11 = bloques, 12..23 = objetos que caen/se recogen);
 * los de tipo word ocupan 48 bytes en el DS (se indexan con slot*2). Todo arranca en 0 en el DS original
 * (verificado en /tmp/data_segment.bin). */
#define L2_OBJ_COUNT 24
extern uint8_t  l2_dat_3410;                 /* DS 0x3410 (byte): init_level2_objects lo pone a 0xc */
extern uint16_t l2_anim_toggle;              /* DS 0x3411 (word): init 0 */
extern uint16_t l2_dat_3415;                 /* DS 0x3415 (word): init 0 */
extern uint8_t  l2_dat_3417[L2_OBJ_COUNT];   /* DS 0x3417: direccion X por slot (1 o 0xff) */
extern uint8_t  l2_dat_342f[L2_OBJ_COUNT];   /* DS 0x342f: init 1 por slot */
extern uint16_t l2_obj_x[L2_OBJ_COUNT];      /* DS 0x3447 (word[24]) */
extern uint8_t  l2_obj_y[L2_OBJ_COUNT];      /* DS 0x3477 (copia de l2_obj_init_y al iniciar) */
extern uint8_t  l2_obj_hit[L2_OBJ_COUNT];    /* DS 0x348f: 1 = ya atrapado/borrado (nada que borrar) */
extern uint8_t  l2_obj_active[L2_OBJ_COUNT]; /* DS 0x34a7: solo los slots 12..23 se activan */
extern uint16_t l2_obj_cga_addr[L2_OBJ_COUNT]; /* DS 0x34bf (word[24]): direccion CGA dibujada; la fijan T35/T36 */
extern uint16_t l2_obj_cur_addr;             /* DS 0x34ef (word) */
extern uint8_t  l2_dat_351b;                 /* DS 0x351b (byte): contador de objetos atrapados; init 0 */

#define L2_OBJ_INIT_Y 0x34f1                 /* DS: 24 bytes de solo lectura (Y inicial por slot) */
#define L2_DAT_351C   0x351c                 /* DS: cuantos objetos se activan por dificultad: {10,8,6,4,3,2,2,1,0,0,...} */

/* init_level2_objects (L806-845): l2_anim_toggle = dat_3415 = 0, dat_3410 = 0xc. Para slot 23..0: hit = 1, active = 0,
 * y = init_y, dat_342f = 1, dat_3417 = (random() & 1) ? 1 : 0xff, x = random() (byte bajo, 0..255); orden de random():
 * primero la direccion y luego la X, por slot. Despues activa `dat_351c[difficulty_level]` slots distintos de 12..23,
 * eligiendo (random() & 0xf) con reintento si >= 12 o si el slot ya esta activo.
 * DESVIACION (unica): con difficulty_level >= 8 la tabla da 0 -> el `loop` con cx=0 en el original da 65536 vueltas y,
 * al llenarse los 12 slots, el reintento no termina nunca (cuelgue). Aqui se sale cuando los 12 estan activos.
 * No ocurre con difficulty_level 0..7. */
void init_level2_objects(void);

/* reset_caught_objects (L848-871): recorre cx=12..1 (slot = cx+11, es decir 23..12) buscando uno activo; al encontrarlo:
 * active = 0, dat_3417 = 1 y x = 0 si cat_x > 0xa0 (sin signo) o dat_3417 = 0xff y x = 0x12e si no; --dat_351b (byte,
 * envuelve) y, si no es 0, REINICIA el barrido desde cx = 12 (el `jnz reset_caught_objects`); si es 0 o no queda ninguno, ret. */
void reset_caught_objects(void);

/* erase_level_object (L1001-1016): si l2_obj_hit[slot] != 0 no hace nada. Si no, copia el patron de borrado (DS 0x3404) en
 * l2_obj_cga_addr[slot]: 1 palabra x 6 filas (0x601) para los slots 0..11, 2 palabras x 2 filas (0x202) para 12..23. */
void erase_level_object(uint16_t slot);
#endif
