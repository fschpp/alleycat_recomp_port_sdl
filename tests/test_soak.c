/* T74: soak headless del flujo real con update_animation() real (sin wraps de logica). Entrada aleatoria con semilla
 * fija, reloj BIOS simulado (avanza solo), sin SDL. Comprueba: no crashea, no se cuelga, y recorre varios niveles. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "animation_entry.h"
#include "score.h"
#include "bios_clock.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

/* los retardos de sonido del original (bucles de ciclos) esperan en tiempo real: se anulan para que el soak sea rapido */
void __wrap_speaker_spin_cycles(uint32_t cycles) { (void)cycles; }

/* read_bios_tick avanza una unidad por llamada: las esperas de "N ticks" (play_crash_sound, ...) terminan al instante */
uint16_t __wrap_read_bios_tick(void) { static uint16_t t; return t++; }

static uint16_t fake_tick;
static uint16_t tick_fn(void) { return fake_tick++; }
/* Reloj compartido (bios_clock): un tick cada ~3 frames, mas un avance por cada 16 lecturas dentro del mismo frame, para que
 * las esperas bloqueantes ("espera N ticks") terminen. Antes enemy.c & co. leian el reloj real y el perro casi no avanzaba. */
static long soak_frame, soak_reads;
static uint16_t soak_clock(void) { return (uint16_t)(soak_frame / 3 + soak_reads++ / 16); }
static uint16_t pit_fn(void)  { return (uint16_t)(rand() & 0xffff); }
static unsigned seen_levels;

/* T74: cobertura de niveles. Para cada nivel 0..7: partida nueva, se fuerza level_number y se entra por game_level_enter()
 * (lo que hace lab_0238 tras morir), y se corre el loop del nivel con entrada aleatoria hasta GL_EXIT o `cap` frames.
 * Informa por nivel: frames corridos, si salio, y vidas. */
static int run_levels(long cap) {
    int bad = 0;
    for (int lv = 0; lv < 8; lv++) {
        srand(740 + lv);
        game_flow_run(GF_LAB_00AE);
        game_death_handler();                         /* start_in_level=1, saved_cat_x/y, elige un nivel... */
        level_number = lv;                            /* ...que se pisa por el forzado */
        game_level_enter();
        long f; int hold = 0; int8_t hk = 0, vk = 0; int exited = 0;
        for (f = 0; f < cap; f++) {
            soak_frame = f; soak_reads = 0;
            ua_tick_override = -1;
            if (hold-- <= 0) { hold = 10 + rand() % 120; hk = (int8_t)(rand() % 3 - 1); vk = (int8_t)(rand() % 3 - 1); }
            input_horizontal = hk; input_vertical = vk; input_fire = (rand() % 8) == 0;
            restart_game = false; show_attract = false;
            if (game_level_frame() == GL_EXIT) { exited = 1; f++; break; }
        }
        printf("nivel %d: %ld frames, %s, level_number=%u lives=%u\n", lv, f, exited ? "SALIO" : "sigue", (unsigned)level_number, (unsigned)lives_count);
        if (exited) (void)game_level_exit(); else bad++;
    }
    printf(bad ? "soak-levels: FALLO (%d niveles sin salir)\n" : "soak-levels: OK (8 niveles entran y salen)\n", bad);
    return bad != 0;
}

int main(int argc, char **argv) {
    long frames = (argc > 1) ? atol(argv[1]) : 200000;
    srand(74);
    cga_init();
    game_tick_fn = tick_fn;
    bios_clock_hook = soak_clock;
    pit_counter_fn = pit_fn;
    if (argc > 2 && argv[2][0] == 'l') return run_levels(frames);   /* ./test_soak <cap> levels */
    game_flow_run(GF_LAB_00AE);                     /* nueva partida -> callejon listo (lab_0155) */
    int in_level = 0; long exits = 0, deaths = 0, enters = 0;
    for (long f = 0; f < frames; f++) {
        soak_frame = f; soak_reads = 0;
        ua_tick_override = -1;                      /* ahora lo cubre bios_clock_hook */
        static int hold = 0; static int8_t hk, vk;
        if (hold-- <= 0) {                          /* pulsaciones largas, como un jugador: camina y de vez en cuando sube */
            hold = 20 + rand() % 280;
            hk = (int8_t)(rand() % 3 - 1);
            vk = (rand() % 6 == 0) ? (int8_t)-1 : 0;
        }
        input_horizontal = hk; input_vertical = vk; /* el juego los resetea cada frame (read_keyboard_dirs): se re-fijan */
        restart_game = false; show_attract = false;
        gf_next_t next = GF_STAY;
        if (in_level) {
            if (game_level_frame() == GL_EXIT) { next = game_level_exit(); exits++; }
        } else {
            next = game_alley_frame();
        }
        if (getenv("SOAK_TRACE") && f % 100000 == 0)
            printf("f=%ld cat_x=%u cat_y=%u lives=%u level=%u walk=%u scroll=%d hor=%d ver=%d\n", f, (unsigned)cat_x, (unsigned)cat_y, (unsigned)lives_count, (unsigned)level_number, (unsigned)auto_walk, (int)scroll_direction, (int)input_horizontal, (int)input_vertical);
        switch (next) {
        case GF_STAY: break;
        case GF_TO_0238: in_level = game_level_enter(); enters++; seen_levels |= 1u << (level_number & 7); break;
        case GF_TO_00F3: in_level = 0; game_flow_run(GF_LAB_00F3); break;
        case GF_TO_0081: in_level = 0; deaths++; game_flow_run(GF_LAB_00AE); break;   /* game over: nueva partida (sin titulo) */
        default: in_level = 0; game_flow_run(GF_LAB_00AE); break;
        }
    }
    printf("soak: %ld frames, %ld entradas a nivel, %ld salidas, %ld game over, niveles vistos=0x%02x\n",
           frames, enters, exits, deaths, seen_levels);
    return 0;
}
