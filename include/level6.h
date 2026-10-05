#ifndef LEVEL6_H
#define LEVEL6_H
#include <stdint.h>
#include <stdbool.h>

/* Nivel 6, helpers A (level_objects.asm L2984-3095) — T29, PROGRESS.md §6z.
 * Estado propio del nivel 6 (DS 0x43dc..0x44d6). Los nombres son los del ASM (`dat_XXXX`) salvo los
 * arrays, que se indexan por SLOT (0..11) y no por el offset en bytes `bx = 2*slot` del original. */

/* Objetos (12 slots; word por slot en el original, bx = 2*slot) */
extern uint16_t l6_obj_flag[12];   /* DS 0x4441 (dat_4441): 1 = slot ya ocupado al sembrar; todo 0 al inicio */
extern uint16_t l6_obj_state[12];  /* DS 0x4459 (l6_obj_state): init_level6_objects lo pone a 0 en los slots usados */
/* Tiles (12, byte por tile) */
extern uint8_t  l6_tile_type[12];  /* DS 0x44c4 (dat_44c4): tipo de tile; init_level6_objects lo carga con l6_obj_init_y_tbl[difficulty_level] */

/* Tracker (sprite 3 words x 10 filas; buffer de fondo dat_43a0 = 0x3c bytes = 30 words) */
extern uint16_t l6_dat_43dc;       /* DS 0x43dc (word): direccion CGA donde se dibujara el tracker */
extern uint16_t l6_dat_43de;       /* DS 0x43de (word): direccion CGA del ultimo dibujo (la que borra erase_l6_tracker) */
extern uint8_t  l6_dat_43e0;       /* DS 0x43e0 (byte): 1 = no hay nada dibujado (erase no hace nada) */
extern uint8_t  l6_dat_44d0;       /* DS 0x44d0 (byte): >= 0x80 -> usa la copia espejada del sprite (+0x3c) */
extern uint16_t l6_dat_44d1;       /* DS 0x44d1 (word): offset DS del sprite del tracker (init 0; lo fija update_level6_*) */
extern uint8_t  l6_dat_44bd;       /* DS 0x44bd (byte): init_level6_objects lo pone a 0 */
extern uint8_t  l6_dat_44be;       /* DS 0x44be (byte): init_level6_objects lo pone a 0 */
extern uint8_t  l6_dat_44d6;       /* DS 0x44d6 (byte): init_level6_objects lo pone a 0xc */

/* erase_l6_tracker (L2984-2994): si dat_43e0 == 0, restaura el fondo en dat_43de (blit_to_cga 3x10 desde el buffer). */
void erase_l6_tracker(void);
/* draw_l6_tracker (L2995-3010): dat_43e0=0; dat_43de=dat_43dc; AND-blit (3x10) del sprite dat_44d1 (+0x3c si
 * dat_44d0 >= 0x80) en dat_43dc, guardando el fondo en el buffer. */
void draw_l6_tracker(void);
/* init_level6_objects (L3011-3050): limpia dat_4441[12]; siembra l6_obj_type[difficulty_level] objetos en slots
 * al azar (random() & 0x1e, reintenta si >= 0x18 o ya ocupado) con blit_to_cga 5x13; carga los 12 tiles con
 * l6_obj_init_y_tbl[difficulty_level] y los dibuja; deja dat_44d0=dat_44bd=dat_44be=0, dat_43e0=1, dat_44d6=0xc.
 * El orden y numero de llamadas a random() es el del original. */
void init_level6_objects(void);
/* draw_l6_tile (L3051-3067): dibuja el tile `bx` (0..11): 2x8 desde l5_sprite_table_base + tipo*32. */
void draw_l6_tile(uint16_t bx);
/* calc_l6_addr (L3068-3076): direccion CGA = calc_cga_addr(y = l6_obj_y[bx], x = l6_obj_dims[2*bx]). Ojo: pese
 * al nombre, `l6_obj_dims` es la X en pixeles del tile. No toca bx. */
uint16_t calc_l6_addr(uint16_t bx);

/* level6_stubs (L3082): es un `ret` desnudo (entry.asm L288 lo llama como "init level 6"); lo que sigue son bytes
 * de relleno decodificados como codigo. No se porta: la llamada de entry.asm es un no-op. */
#endif
