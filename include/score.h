#ifndef SCORE_H
#define SCORE_H

#include <stdint.h>

/* Ported from score.asm — see src/score.c and PROGRESS.md §5v.
 * NOTE: function names here follow what each function actually DOES
 * (content is self-consistent with update_high_score's clear comment),
 * not the original disassembly's labels, which turned out to be swapped
 * relative to their content — see §5v. */

void clear_score(void);
void clear_high_score(void);

/* add_score — adds a BCD digit (0-9) to the ones place of current_score,
 * propagating carries leftward. */
void add_score(unsigned char bcd_digit);

/* add_bcd_scores — adds the 7-byte BCD buffer at `src` into `dst`,
 * right-to-left with carry propagation (both MSB-first, matching
 * current_score/high_score's layout). */
void add_bcd_scores(unsigned char *dst, const unsigned char *src);

void update_high_score(void);

/* draw_lives — redraws the lives counter only if it changed since the
 * last call (matches the original's cached-compare-then-blit pattern). */
void draw_lives(void);

/* draw_current_score / draw_high_score_display — render the 7-digit BCD
 * buffers to their respective fixed screen positions. Named by content,
 * not by the original's (swapped) labels — see §5v. */
void draw_current_score(void);
void draw_high_score_display(void);


/* --- Barra de bonus / fin de nivel (level_objects.asm L1100-1375; T47 en score.c, T48/T49 pendientes) ---
 * Estado compartido entre handle_level_complete (T47) y los helpers de T48/T49. Offsets DS verificados con
 * /tmp/data_segment_labels.txt. Las variables mutables son C propias (no ds_pool); las tablas de solo lectura
 * (dat_36cc, dat_36dc) se leen de ds_pool. */
extern uint16_t bonus_binary;      /* DS 0x3697: bonus en binario (lo calcula handle_level_complete) */
extern uint8_t  bonus_bcd[8];      /* DS 0x368d..0x3694: bonus en BCD, 7 digitos MSB-first (+1 byte de relleno que binary_to_bcd pone a 0) */
extern uint16_t bonus_tick_start;  /* DS 0x3695: tick de inicio del parpadeo final */
extern uint8_t  bonus_color;       /* DS 0x3699: color del borde en flash_score_color (1 = nivel 7, 2 = resto) */
extern uint8_t  bonus_row;         /* DS 0x369e: fila de texto (pixel y) donde print_bonus_score imprime */
extern uint8_t  bonus_bar_flag;    /* DS 0x369f: 0 = animate_score_bar sin melodia, 1 = con melodia */
extern uint16_t bonus_l7_index;    /* DS 0x370c: indice (en bytes) a la tabla de textos dat_36ec de print_level7_bonus */
extern uint16_t bonus_duration;    /* DS 0x3722: duracion del parpadeo final, en ticks BIOS */

/* Regiones de pantalla que save_score_regions guarda en DS:0x000e (4 words x 8 filas = 64 bytes) y DS:0x004e
 * (20 words x 8 filas = 320 bytes) y que el final del nivel 7 devuelve a CGA 0x8e4 / 0xc94. */
extern uint8_t  score_save_a[64];
extern uint8_t  score_save_b[320];

/* int 0x1a (dx): ticks BIOS a 18.2 Hz; usa game_tick_fn si esta puesto (hook de tests), si no el reloj monotono. */
uint16_t score_tick(void);

/* handle_level_complete (T47, level_objects.asm L1100-1227): declarada en game_flow.h. */

/* --- Helpers de T48/T49: STUBS en src/flow_stubs.c hasta que se porten. Las firmas siguen los registros del ASM:
 * mask_score_tiles(dx), animate_score_bar(ax), binary_to_bcd(ax) y flash_score_color() devuelve dx (el tick
 * leido al entrar: el bucle de handle_level_complete lo usa en `sub dx,[dat_3695]` sin releerlo). --- */
void     mask_score_tiles(uint16_t dx);      /* TODO(T48) */
void     animate_score_bar(uint16_t ax);     /* TODO(T49) */
void     binary_to_bcd(uint16_t ax);         /* TODO(T49) */
void     print_bonus_score(void);            /* TODO(T48) */
void     print_level7_bonus(void);           /* TODO(T48) */
uint16_t flash_score_color(void);            /* TODO(T48) */
void     save_score_regions(void);           /* T47: portada aqui (8 lineas de ASM, la necesita el final del nivel 7) */

#endif
