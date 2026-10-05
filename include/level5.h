#ifndef LEVEL5_H
#define LEVEL5_H
#include <stdint.h>
#include <stdbool.h>

/* Nivel 5, helpers A (level_objects.asm L2362-2437) — T25, PROGRESS.md §6v.
 * Estado propio del nivel 5 (DS 0x40a8..0x40cc). Todo arranca en 0 en el DS original (verificado en
 * /tmp/data_segment.bin). Los nombres son los del ASM (`dat_XXXX`): la semántica es la que se deduce
 * de los usos en estos 5 helpers, no se renombra más hasta portar T27/T28. */
extern uint16_t l5_dat_40a8;  /* DS 0x40a8 (word): X del perch (init: 0x90) */
extern uint8_t  l5_dat_40aa;  /* DS 0x40aa (byte): Y del perch (init: 0x86) */
extern uint16_t l5_dat_40ab;  /* DS 0x40ab (word): dirección CGA del perch (calc_cga_addr(0x86, 0x90)) */
extern uint8_t  l5_dat_40af;  /* DS 0x40af (byte): init = 0 */
extern uint8_t  l5_dat_40b1;  /* DS 0x40b1 (byte): init = 0 */
extern uint16_t l5_dat_40b2;  /* DS 0x40b2 (word): X del objeto móvil (rect de colisión: 8 de ancho, 5 de alto) */
extern uint8_t  l5_dat_40b4;  /* DS 0x40b4 (byte): Y del objeto móvil */
extern uint8_t  l5_dat_40b8;  /* DS 0x40b8 (byte): init = 0; check_l5_thrown lo pone a 0xff */
extern uint8_t  l5_dat_40b9;  /* DS 0x40b9 (byte): init = 1 */
extern uint16_t l5_dat_40c8;  /* DS 0x40c8 (word): init = 0xff */
extern uint8_t  l5_dat_40ca;  /* DS 0x40ca (byte): signo en X (calc_l5_direction): +1 o 0xff */
extern uint8_t  l5_dat_40cb;  /* DS 0x40cb (byte): signo en Y (calc_l5_direction): +1 o 0xff */
extern uint16_t l5_dat_40cc;  /* DS 0x40cc (word): distancia aproximada |dx| + 2*|dy| (con `not`) */

/* check_l5_landing (L2362-2371): CF si in_level_mode == 0 y (cat_y & 0xf8) == 0x88. */
bool check_l5_landing(void);
/* calc_l5_direction (L2372-2395): dat_40ca/dat_40cb = signo de (obj - gato) en X/Y (+1, o 0xff si hubo
 * préstamo); dat_40cc = |dx| + 2*|dy|, donde |.| usa `not` (complemento a 1, no neg) como el original. */
void calc_l5_direction(void);
/* check_l5_cat_catch (L2396-2411): objeto (b2,b4; 8x5) vs. gato (0x18 x 0x0e). Si hay colisión y
 * object_hit == 0 -> cat_caught = 1. Sin colisión no toca nada. */
void check_l5_cat_catch(void);
/* check_l5_thrown (L2412-2422): objeto (b2,b4; 8x5) vs. objeto lanzado (0x10 x 0x1e). Si hay
 * colisión -> dat_40b8 = 0xff. */
void check_l5_thrown(void);
/* init_level5_objects (L2423-2437): perch en (0x90, 0x86), dibuja el perch (draw_l5_perch) y deja los
 * flags en sus valores iniciales. */
void init_level5_objects(void);

/* --- T26: helpers B (level_objects.asm L2603-2665) --- */
extern uint16_t l5_dat_40a6;  /* DS 0x40a6 (word): dirección CGA del último dibujo del perch (la usa erase) */

/* check_l5_perch_hit (L2603-2612): perch (40a8,40aa; 0x18 x 0x10) vs. gato (0x18 x 0x0e). Devuelve CF. */
bool check_l5_perch_hit(void);
/* draw_l5_perch (L2613-2622): guarda dat_40ab en dat_40a6 y hace blit_masked de 3 words x 16 filas de
 * dat_3fbe (DS 0x3fbe, 96 bytes) en dat_40ab, guardando el fondo en dat_401e (DS 0x401e, 96 bytes). */
void draw_l5_perch(void);
/* erase_l5_perch (L2623-2630): blit_to_cga del fondo guardado (dat_401e) en dat_40a6 (3 x 16). */
void erase_l5_perch(void);
/* check_l5_thrown_near (L2631-2645): CF si thrown_obj_y >= 0x66 y perch_x-0x14 <= thrown_x <= perch_x+0x28
 * (comparaciones de 16 bits sin signo; perch_x-0x14 y +0x30 envuelven como en el original). */
bool check_l5_thrown_near(void);
/* check_thrown_near_cat (L2651-2665): objeto lanzado (0x10 x 0x1e) vs. caja alrededor del gato
 * (x = max(cat_x-8, 0) con clamp por préstamo sin signo, y = cat_y+3, 0x28 x 0x0e). CF. También la usa
 * el nivel 6 (T32). */
bool check_thrown_near_cat(void);
#endif
