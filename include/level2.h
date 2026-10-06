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

/* check_level_objects (level_objects.asm L690-805) — T34, PROGRESS.md §6ae. Barre los 24 slots (dat_3511 = 0..23, dat_351b = 0):
 * los activos (l2_obj_active != 0) se saltan; el resto se prueba contra el gato con check_rect_collision (objeto: ancho
 * dat_3513[slot>=12] = {8,0x10}, alto dat_3517 = {6,2}; gato: 0x18 x 0xe).
 *  - Golpe fatal (slot >= 12, !cat_caught, !immune_flag): object_hit = 1, dibuja l1_anim_sprite_c (5x0x12) en (cat_x-8 acotado a
 *    0..0x116, cat_y acotado a <= 0xb4), reset_noise y BLOQUEA 0xd ticks BIOS (~0.7 s) llamando update_noise sin parar y alternando
 *    el borde 1/0xf; devuelve false (CF=0) SIN seguir el barrido.
 *  - Captura (slot < 12 o cat_caught o immune): ++dat_351b, start_tone(0x5dc,0x425), restore_alley_buffer solo la primera vez,
 *    erase_level_object, active = 1; si slot < 12: --dat_3410 y, al llegar a 0 sin immune_flag, cat_caught = 1.
 * Al final del barrido: dat_351b != 0 -> reset_caught_objects y devuelve true (stc); si no, false (clc).
 * NO llama add_score (el ASM no lo hace). check_vsync: el bucle espera un retrace simulado por reloj (60 Hz). */
extern uint16_t l2_dat_3509;                 /* DS 0x3509 (word): ultimo tick procesado; init 0 */
extern uint16_t l2_dat_350b;                 /* DS 0x350b (word): tick de la pasada en curso; init 0 */
extern uint16_t l2_dat_3413;                 /* DS 0x3413 (word): fase del sprite 12..23, +8 por vuelta de 24 slots; init 0 */
extern uint16_t l2_dat_3511;                 /* DS 0x3511 (word): slot en curso */
extern uint8_t  l2_border_color;             /* ultimo `int 0x10 ah=0xb` (sin efecto visible) */
extern uint16_t (*l2_tick_fn)(void);         /* solo tests: sustituye a int 0x1a (NULL = reloj real) */
extern bool     (*l2_vsync_fn)(void);        /* solo tests: sustituye al bit de retrace (NULL = reloj real) */
bool check_level_objects(void);

/* ---- T36: animaciones (level_objects.asm L1017-1099), PROGRESS.md §6ag ---- */
#define L2_BLOCK_COUNT 0x28
extern uint8_t  l2_block_types[L2_BLOCK_COUNT]; /* DS 0x2656 (byte[0x28]): lo llena draw_level2_background (score.asm L180) */
extern uint16_t l2_dat_350d;                 /* DS 0x350d (word): bloque en curso de la ola; init 0 */
extern uint16_t l2_dat_350f;                 /* DS 0x350f (word): tick de arranque de la ola; init 0 */
extern uint16_t l2_dat_35d8;                 /* DS 0x35d8 (word): fase de la entrada (+2 por frame, & 6); init 0 */
extern uint16_t l2_dat_35da;                 /* DS 0x35da (word): ultimo tick de la entrada; init 0 */

/* animate_level2_blocks (L1017-1059): cada vez que pasan >= 8 ticks desde dat_350f avanza UN bloque (dat_350d = 1..0x27; al llegar a 0x28
 * vuelve a 0 y fija dat_350f = tick, asi que la ola es continua y dat_350f solo se refresca al cerrar la vuelta). Si cat_y <= 7 y el gato
 * esta a menos de 4 columnas del bloque (|cat_x/4 + 1 - 2*bloque|, con `not` si hay borrow) no lo anima. Si no: tipo += 8 y dibuja el frame
 * (tipo & 0x18) de level2_bar_sprites (1x4) en 0xa0 + 2*bloque (`shl di,0x0` del listado es `shl di,1`). */
void animate_level2_blocks(void);

/* update_entrance_anim (L1063-1093): cada >= 6 ticks (desde dat_35da) avanza dat_35d8 += 2 y dibuja el frame dat_35d0[dat_35d8 & 6] (2x10)
 * en el offset CGA 0x15c9 (el valor del label enemy_sprite_table_hi usado como direccion). Si el gato toca el rect (0xe4, 0x8a, 0x10 x 0xa)
 * pone el BYTE bajo de level_complete a 1. */
void update_entrance_anim(void);

/* update_level2_objects (level_objects.asm L872-998) — T35, PROGRESS.md §6af. Mueve UN slot por llamada (dat_3415 = 1..23, 0 al
 * envolver) y solo cuando el tick BIOS != dat_3509. Al envolver: dat_3415 = 0, l2_anim_toggle ^= 0xc, dat_3413 += 8 y dat_3509 = tick.
 * En el slot 12 solo marca el tick (dat_3509 = tick) si rom_id != 0xfd o cat_y >= 0x30 (si no, el slot 12 se mueve de todos modos
 * y el tick sigue "pendiente"). Slot activo -> return (sin mover). Si random() <= 0x10: nueva direccion X (dat_3417 = 1/0xff) y Y
 * (dat_342f = 1/0xff), 2 llamadas a random en ese orden. Paso X: 4 (slots < 12) o 2; rebota en 0 / 0x12e. Paso Y: +-1 entre
 * init_y y init_y + 0x18 (rebota). Luego erase_level_object(slot), guarda la direccion CGA, hit = 0 y dibuja: slots >= 12 sprite_b
 * [(slot*8 + dat_3413) & 0x18] 2x2; slots < 12 sprite_a [(toggle ^ (slot par ? 0xc : 0)) + (dir != 1 ? 0x18 : 0)] 1x6.
 * Los dos `shl si,0x0` / `shr cl,0x0` del listado son `shl si,1` / `shr cl,1` (bytes d1 e6 / d0 e9). */
void update_level2_objects(void);
#endif
