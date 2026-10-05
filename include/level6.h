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

extern uint8_t  l6_dat_44d9;       /* DS 0x44d9 (byte): 1 = el gato esta cerca de un objeto (lo fija check_l6_proximity); init 0 */
extern uint16_t l6_dat_44d7;       /* DS 0x44d7 (word): ultimo tick BIOS procesado por update_level6_timing; init 0 */
extern uint8_t  l6_dat_44fc;       /* DS 0x44fc (byte): objetos que pasaron a estado 2 en este pase (-> explosion); init 0 */
extern int32_t  l6_tick_override;  /* solo tests: >= 0 sustituye a `int 0x1a` (mismo patron que l4_tick_override); -1 = reloj real */
extern uint16_t l6_dat_44da;       /* DS 0x44da (word): direccion CGA del objeto a limpiar (la fija update_level6_timing, T31); init 0 */

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

/* --- T30: helpers B (level_objects.asm L2741-2816). OJO: estas funciones reciben `slot` (0..11); el `bx` del ASM es
 * 2*slot (offset en bytes en tablas de words), que el codigo C recalcula donde hace falta. --- */
/* prepare_l6_erase (L2741-2749): si dat_44bd != 0 -> erase_l6_tracker + dat_44bd = 0; si no -> restore_alley_buffer. */
void prepare_l6_erase(void);
/* clear_l6_object (L2750-2764): rellena un bloque de 5x13 words con 0xAAAA (fondo) y lo vuelca en la direccion CGA dat_44da.
 * El original rellena el scratch DS:0x000e; aqui se usa un buffer local (como level3_enemy.c) para no pisar ds_pool. */
void clear_l6_object(void);
/* refresh_l6_display (L2765-2775): si dat_44d9 != 0: dat_44bd != 0 -> draw_l6_tracker; si no -> draw_alley_foreground. */
void refresh_l6_display(void);
/* check_l6_proximity (L2776-2799): dat_44d9 = 0; colision (check_rect_collision) entre el rect del objeto
 * (x = dat_43e1[slot]-0x14, y = byte bajo de dat_43f9[slot], 0x28 x 6) y el del gato (0x18 x 0x0e). Si hay choque:
 * dat_44d9 = 1, (check_vsync = no-op) y borra al gato/tracker (dat_44bd != 0 -> erase_l6_tracker; si no -> restore_alley_buffer). */
void check_l6_proximity(uint16_t slot);
/* draw_l6_alert (L2800-2816): blit 1 word x 1 fila desde dat_4100 + 2*l6_obj_state[slot] (+6 si el sprite del objeto no es
 * 0x429c) a l6_obj_x[slot] + 0xa7 (-6 en ese mismo caso). */
void draw_l6_alert(uint16_t slot);

/* update_level6_timing (L2666-2740, T31): cada (difficulty_level -> dat_44dc = {18,16,15,14,13,12,11,10}) ticks BIOS recorre
 * los 12 slots de cx=12 a 1 (slot = cx-1): si el gato esta en la fila del objeto y a <= dat_44ec[dif] pixeles en X, sube
 * l6_obj_state; con state >= 2 limpia el objeto y llama activate_enemy_chase (y termina); si no, con state != 0 baja el estado
 * con probabilidad (dl <= 0x38). Luego dibuja alerta y refresca. Si algun objeto llego a 2, play_explosion_effect al final. */
void update_level6_timing(void);

/* --- T32: update_level6_movement (level_objects.asm L2817-2983) --- */
extern uint16_t l6_dat_44d3;       /* DS 0x44d3 (word): ultimo tick BIOS procesado por update_level6_movement; init 0 */
extern uint16_t l6_dat_44bf;       /* DS 0x44bf (word): mejor distancia X al tile (init 0; la fija la busqueda, parte de 0xffff) */
extern uint16_t l6_dat_44c1;       /* DS 0x44c1 (word): slot del tile mas cercano (>= 0xc = ninguno); init 0 */
extern uint8_t  l6_dat_44c3;       /* DS 0x44c3 (byte): direccion hacia el tile: 0xff (gato a su derecha) o 1; init 0 */
extern uint8_t  l6_dat_44d5;       /* DS 0x44d5 (byte): 1 si el avance del sprite (dat_44d0 += 0x30) dio acarreo -> pisa un tile; init 0 */

/* update_level6_movement (L2817-2983, T32): no hace nada con enemy_active != 0 o si el tick BIOS no cambio. Con auto_walk
 * solo recoge el tracker. Si no, busca (cx=12..1) el tile con tipo >= 1 en la fila cat_y+8 mas cercano en X (empate -> slot mas
 * bajo); sin tile (o con joy_button != 0) retira el tracker; con distancia 4..8 fija scroll_speed=4, y con >= 4 hace que el
 * gato camine hacia el tile (input_horizontal/scroll_direction = dat_44c3); con < 4 dibuja el tracker (sprite dat_44a5)
 * junto al gato y, cuando dat_44d5 != 0, gasta un uso del tile (dat_44c4--, sonido y cat_caught al agotar los 12). */
void update_level6_movement(void);

/* level6_stubs (L3082): es un `ret` desnudo (entry.asm L288 lo llama como "init level 6"); lo que sigue son bytes
 * de relleno decodificados como codigo. No se porta: la llamada de entry.asm es un no-op. */
#endif
