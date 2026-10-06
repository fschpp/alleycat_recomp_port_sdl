#ifndef GAME_FLOW_H
#define GAME_FLOW_H

#include <stdint.h>

/* entry.asm L27-143 (T40, PROGRESS.md §6ak): arranque, titulo, nueva partida y
 * preparacion del callejon, hasta justo antes de lab_0155 (el loop del callejon,
 * que es T41). El original es un unico bloque con saltos; aqui es una funcion con
 * `goto` y varios puntos de entrada, porque el loop (T41) vuelve a esos labels. */
typedef enum {
    GF_ENTRY = 0,   /* entry:    init unica de hardware, luego cae en lab_0081 */
    GF_LAB_0081,    /* titulo / game over */
    GF_LAB_00A3,    /* attract mode, luego cae en lab_00ae */
    GF_LAB_00AE,    /* nueva partida */
    GF_LAB_00F3     /* preparacion del callejon (o del nivel si start_in_level) */
} gf_entry_t;

/* Ejecuta desde `from` hasta llegar a lab_0155 y retorna ahi. */
void game_flow_run(gf_entry_t from);
/* entry: game_flow_run(GF_ENTRY). */
void game_start(void);

/* Hooks de test (NULL = hardware real). */
extern uint16_t (*game_tick_fn)(void);        /* int 0x1a: ticks BIOS (18.2 Hz) */
extern uint16_t (*pit_counter_fn)(void);      /* PIT canal 0 latcheado (read_pit_counter) */

/* read_pit_counter (cga.asm L167-182): rng_seed = contador PIT, o 0xfa59 si es 0. */
void read_pit_counter(void);

/* --- Pendientes de otras tareas: stubs en src/flow_stubs.c. Cada uno se
 * borra de ahi cuando su tarea lo porte de verdad. --- */
void set_palette(void);          /* TODO(T44): enemy.asm L242-274 (CGA palette 1 + out 0x3d9) */
void show_title_screen(void);    /* TODO(T52): ui.asm L55-163 */
void show_attract_mode(void);    /* TODO(T54): ui.asm L307-382 (fija use_joystick y difficulty_counter) */

#endif
