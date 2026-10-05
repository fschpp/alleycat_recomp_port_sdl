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
 * colisión -> dat_40b8 = 0xff. Devuelve CF (el `mov` no toca flags: sale con CF=1 si hubo colisión);
 * update_level5_anim lo consume con `jb` (T27). */
bool check_l5_thrown(void);
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

/* --- T27: update_level5_anim (level_objects.asm L2198-2361) --- */
extern uint16_t l5_dat_40b5;  /* DS 0x40b5 (word): último tick BIOS procesado */
extern uint8_t  l5_dat_40b7;  /* DS 0x40b7 (byte): dirección X del objeto: 0, 1 (der.) o 0xff (izq.) */
extern uint16_t l5_dat_40ba;  /* DS 0x40ba (word): dirección CGA del último dibujo del objeto (para borrarlo) */
extern uint16_t l5_dat_40bc;  /* DS 0x40bc (word): dirección CGA calculada para el dibujo de este tick */
extern uint16_t l5_dat_40be;  /* DS 0x40be (word): contador de frame de animación (+2 por dibujo; usa bits 1-2) */
extern uint8_t  l5_dat_40ff;  /* DS 0x40ff (byte): contador de ticks procesados */
extern uint16_t l5_dat_3f2c[5]; /* DS 0x3f2c: fondo guardado bajo el objeto (1 word x 5 filas = 10 bytes) */

/* Hook de pruebas del tick BIOS (`int 0x1a`): -1 = reloj real (~18.2 Hz); >= 0 = ese valor. */
extern int32_t l5_tick_override;

/* update_level5_anim: un paso del objeto móvil del nivel 5 por tick BIOS. Solo actúa con el perch
 * bajado (dat_40aa >= 0xa4). Elige dirección (persigue al gato con probabilidad por dificultad, o va
 * hacia uno de 11 puntos objetivo dat_40de/dat_40f4, o dirección aleatoria), mueve (+-2 en Y entre
 * 0x30 y 0xa7, +-4 en X entre 0 y 0x135), borra el dibujo anterior y dibuja el frame siguiente
 * (3 sprites de 1 word x 5 filas en dat_40c0[], espejados +0x1e si va a la izquierda). El orden y el
 * número de llamadas a random() es el del original. */
void update_level5_anim(void);

/* --- T28: update_level5_objects (level_objects.asm L2438-2602) --- */
extern uint16_t l5_dat_40ad;  /* DS 0x40ad (word): ultimo tick BIOS procesado */
extern uint8_t  l5_dat_40b0;  /* DS 0x40b0 (byte): direccion del empujon al gato (1 = izq., 0xff = der.) */
extern int32_t  l5_tick_advance; /* hook de pruebas: se suma a l5_tick_override tras cada lectura (0 = fijo) */

/* update_level5_objects: un paso por tick BIOS del perch del nivel 5. (a) Un objeto lanzado cerca del
 * perch y el gato aterrizado -> fin de nivel (in_level_mode=1, transition_timer=0x10). (b) El gato
 * pisa el perch -> lo empuja hasta 0x20 pasos de 8 px. (c) Si el perch fue empujado, se desplaza y
 * suena un tono. (d) Si no, lo baja de 5 en 5 filas hasta 0xa4 (espera bloqueante de una vez, un paso
 * por tick, con tono de pitch variable) y al llegar deja el sprite aterrizado (dat_3f36, 4x17) y
 * activa al objeto movil de update_level5_anim. */
void update_level5_objects(void);
#endif
