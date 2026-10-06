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

/* entry.asm L144-236 (T41, PROGRESS.md §6al): loop del callejon (lab_0155..lab_01b7),
 * manejador de muerte (lab_01b7) y selector de nivel (lab_01e5..lab_022a).
 * game_alley_frame() es UNA pasada del loop; el que lo llama (main.c) hace la parte
 * de plataforma (eventos SDL, input_poll, video_present, retardo) y salta segun el
 * resultado. El original no tiene retardo ni eventos: loop cerrado. */
typedef enum {
    GF_STAY = 0,    /* salto a lab_0155: seguir en el callejon */
    GF_TO_0081,     /* game over -> titulo: game_flow_run(GF_LAB_0081) */
    GF_TO_00A3,     /* timeout -> attract: game_flow_run(GF_LAB_00A3) */
    GF_TO_00AE,     /* restart -> nueva partida: game_flow_run(GF_LAB_00AE) */
    GF_TO_0238      /* la gata murio: level_number ya elegido, saltar al despacho (T42) */
} gf_next_t;

gf_next_t game_alley_frame(void);
/* lab_01b7: guarda tick y posicion, start_in_level=1, y elige level_number
 * (7 si force_level7, si no select_next_level()). Termina donde empieza lab_0238. */
void game_death_handler(void);
/* lab_01e5..lab_022a: elige un nivel con las tablas de dificultad, evita repetir los
 * dos ultimos y desplaza last_level/prev_level. Devuelve el nivel (tambien en level_number). */
uint16_t select_next_level(void);

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
