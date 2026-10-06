/* T47 — handle_level_complete (level_objects.asm L1100-1227). Sin SDL. Valores esperados de un modelo Python
 * independiente del ASM (ver PROGRESS.md §6ar). Los helpers de T48/T49 se envuelven con --wrap para registrar
 * sus argumentos; binary_to_bcd se reemplaza por una version real minima para comprobar el BCD resultante. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "level7_epilogue.h"
#include "palette.h"
#include "score.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

enum { E_MASK = 1, E_BAR, E_BCD, E_SAVE, E_PBONUS, E_PL7, E_FLASH, E_VNOTE, E_LNOTE, E_SIL };
static int tr[4000], ntr;
static uint16_t a_bcd, flash_calls;
static uint16_t masks[8], nmask, bars[8]; static uint8_t bar_row[8], bar_flag[8]; static int nbar;
static void ev(int e) { if (ntr < 4000) tr[ntr++] = e; }

void __wrap_mask_score_tiles(uint16_t dx)  { ev(E_MASK); if (nmask < 8) masks[nmask] = dx; nmask++; }
void __wrap_animate_score_bar(uint16_t ax) { ev(E_BAR); if (nbar < 8) { bars[nbar] = ax; bar_row[nbar] = bonus_row; bar_flag[nbar] = bonus_bar_flag; } nbar++; }
void __wrap_binary_to_bcd(uint16_t ax) {
    ev(E_BCD); a_bcd = ax;
    /* version de referencia: 4 digitos BCD en los 4 ultimos bytes del buffer de 7 */
    memset(bonus_bcd, 0, sizeof bonus_bcd);
    for (int i = 6; i >= 3; i--) { bonus_bcd[i] = (uint8_t)(ax % 10); ax /= 10; }
}
void __wrap_print_bonus_score(void)  { ev(E_PBONUS); }
static int scribble;   /* simula el texto del bonus del nivel 7 pisando las dos regiones guardadas */
void __wrap_print_level7_bonus(void) {
    ev(E_PL7);
    if (scribble) for (int r = 0; r < 8; r++) {
        memset(&cga_mem[0x8e4 + (size_t)(r & 1) * 0x2000 + (size_t)(r >> 1) * 80], 0xff, 8);
        memset(&cga_mem[0xc94 + (size_t)(r & 1) * 0x2000 + (size_t)(r >> 1) * 80], 0xff, 40);
    }
}
void __wrap_play_victory_note(void)  { ev(E_VNOTE); }
void __wrap_play_level_note(void)    { ev(E_LNOTE); }
void __wrap_silence_speaker(void)    { ev(E_SIL); }
uint16_t __wrap_flash_score_color(void) { ev(E_FLASH); flash_calls++; return game_tick_fn(); }

static uint16_t fake_tick, tick_step;
static uint16_t tickfn(void) { uint16_t t = fake_tick; fake_tick = (uint16_t)(fake_tick + tick_step); return t; }

static unsigned bcd_val(const uint8_t *b) { unsigned v = 0; for (int i = 0; i < 7; i++) v = v * 10 + b[i]; return v; }
static void set_score(unsigned v) { for (int i = 6; i >= 0; i--) { current_score[i] = (uint8_t)(v % 10); v /= 10; } }
static int count(int e) { int n = 0; for (int i = 0; i < ntr; i++) n += tr[i] == e; return n; }

/* Valores esperados: modelo Python independiente (no leen ds_pool). Constantes BCD por nivel 0..7:
 * 3000,3000,3000,2000,1500,3500,2500,4000.
 * now = tick que devuelve int 0x1a la PRIMERA vez (entrada de lab_38d3); luego avanza `step` por llamada. */
#define SETUP(lvl, diff, now, step) do { \
    ntr = 0; nmask = nbar = 0; flash_calls = 0; a_bcd = 0xdead; \
    fake_tick = (now); tick_step = (step); game_tick_fn = tickfn; \
    level_state = (lvl); difficulty_level = (diff); \
    memset(bonus_bcd, 0xee, sizeof bonus_bcd); memset(cga_mem, 0x11, sizeof cga_mem); \
    l7_completion_counter = 5; force_level7 = 0; } while (0)

int main(void) {
    /* ---- nivel 3, dt=700: bin=(1350-700)*2=1300, bcd=1300+2000, barra row=0x50 ax=3200 */
    game_tick = 1000; set_score(1234);
    SETUP(3, 0, 1700, 1);
    handle_level_complete();
    CHECK(a_bcd == 1300 && bonus_binary == 1300, "n3: binary_to_bcd(%u) bonus_binary %u (esperado 1300)", a_bcd, bonus_binary);
    CHECK(bcd_val(bonus_bcd) == 3300, "n3: bonus BCD %u (esperado 3300)", bcd_val(bonus_bcd));
    CHECK(bcd_val(current_score) == 4534, "n3: score %u (esperado 4534)", bcd_val(current_score));
    /* nivel != 7: primer bloque (lab_38ba) + segundo (lab_396e) */
    CHECK(l7_completion_counter == 6 && force_level7 == 1, "n3: [0x414]++ y [0x418]=1 (%u %u)", l7_completion_counter, force_level7);
    CHECK(nmask == 2 && masks[0] == 0xaaaa && masks[1] == 0xffff, "n3: mask_score_tiles 0xaaaa luego 0xffff (%u: 0x%x 0x%x)", nmask, masks[0], masks[1]);
    CHECK(nbar == 2 && bars[0] == 0 && bar_flag[0] == 0, "n3: 1a barra ax=0 sin melodia (%d: %u flag %u)", nbar, bars[0], bar_flag[0]);
    CHECK(nbar == 2 && bars[1] == 3200 && bar_row[1] == 0x50 && bar_flag[1] == 1, "n3: 2a barra ax=%u row=%u flag=%u (esperado 3200 / 80 / 1)", bars[1], bar_row[1], bar_flag[1]);
    CHECK(bonus_color == 2 && bonus_duration == 0x1e, "n3: color %u dur %u", bonus_color, bonus_duration);
    CHECK(count(E_LNOTE) > 0 && count(E_VNOTE) == 0 && count(E_PBONUS) == 1 && count(E_PL7) == 0, "n3: melodia de nivel, un print_bonus_score");
    CHECK(flash_calls == 0x1e && count(E_LNOTE) == 0x1e, "n3: %u parpadeos / %d notas (esperado 30)", flash_calls, count(E_LNOTE));
    CHECK(tr[ntr - 1] == E_SIL && cga_mem[0x8e4] == 0x11 && cga_mem[0xc94] == 0x11, "n3: termina en silence_speaker, sin restaurar CGA");

    /* ---- nivel 3 saturado: dt=2000 > 1350 -> bin 0, bcd 2000, row=0xa0, ax=6400 */
    game_tick = 0; set_score(0);
    SETUP(3, 0, 2000, 1);
    handle_level_complete();
    CHECK(a_bcd == 0 && bcd_val(bonus_bcd) == 2000 && bcd_val(current_score) == 2000, "n3 sat: bin %u bcd %u score %u", a_bcd, bcd_val(bonus_bcd), bcd_val(current_score));
    CHECK(nbar == 2 && bars[1] == 6400 && bar_row[1] == 0xa0, "n3 sat: barra %u row %u (esperado 6400 / 160)", bars[1], bar_row[1]);

    /* ---- nivel 6: bin = 2700-200 = 2500 (sin doblar), bcd = 2500+2500, row=0 */
    game_tick = 50; set_score(0);
    SETUP(6, 0, 250, 1);
    handle_level_complete();
    CHECK(a_bcd == 2500 && bcd_val(bonus_bcd) == 5000 && bcd_val(current_score) == 5000, "n6: bin %u bcd %u score %u (esperado 2500/5000/5000)", a_bcd, bcd_val(bonus_bcd), bcd_val(current_score));
    CHECK(nbar == 2 && bars[1] == 0 && bar_row[1] == 0, "n6: barra %u row %u", bars[1], bar_row[1]);

    /* ---- nivel 5, dt=0: bin = 1350*2 = 2700, bcd = 2700+3500 = 6200 */
    game_tick = 77; set_score(0);
    SETUP(5, 0, 77, 1);
    handle_level_complete();
    CHECK(a_bcd == 2700 && bcd_val(bonus_bcd) == 6200 && bcd_val(current_score) == 6200, "n5: bin %u bcd %u score %u", a_bcd, bcd_val(bonus_bcd), bcd_val(current_score));

    /* ---- nivel 7, diff 2, spawn_slot=0xffff: dt contra start_tick; bin=(0x2a30-60)>>1=5370, bcd=5370+4000;
     *      repeticiones dat_36dc[2]=5; sin primer bloque (no toca [0x414]/[0x418], ni mask/barra) */
    start_tick = 3000; game_tick = 7; set_score(10); l7_obj_spawn_slot = 0xffff;
    SETUP(7, 2, 3060, 1);
    handle_level_complete();
    CHECK(a_bcd == 5370 && bcd_val(bonus_bcd) == 9370, "n7: bin %u bcd %u (esperado 5370/9370)", a_bcd, bcd_val(bonus_bcd));
    CHECK(bcd_val(current_score) == 46860, "n7: score %u (esperado 10 + 5*9370 = 46860)", bcd_val(current_score));
    CHECK(l7_completion_counter == 5 && force_level7 == 0 && nmask == 0 && nbar == 0, "n7: sin primer bloque (cnt %u force %u mask %u bar %d)", l7_completion_counter, force_level7, nmask, nbar);
    CHECK(bonus_l7_index == 4 && bonus_row == 0x38 && bonus_color == 1 && bonus_duration == 0x44, "n7: idx %u row %u color %u dur %u", bonus_l7_index, bonus_row, bonus_color, bonus_duration);
    CHECK(count(E_PBONUS) == 1 && count(E_PL7) == 1 && count(E_VNOTE) == 0x44 && count(E_LNOTE) == 0, "n7: textos; %d notas de victoria (esperado 68)", count(E_VNOTE));
    CHECK(count(E_SIL) == 0, "n7: no llama silence_speaker");

    /* ---- nivel 7 con spawn_slot=3 (< 8), diff 1, dt=0: bin=0x2a30>>1=5400, repeticiones 3*2=6, idx 2+0x10 */
    start_tick = 100; set_score(0); l7_obj_spawn_slot = 3;
    SETUP(7, 1, 100, 1);
    handle_level_complete();
    CHECK(a_bcd == 5400 && bonus_l7_index == 18 && bcd_val(current_score) == 56400, "n7b: bin %u idx %u score %u (esperado 5400/18/56400)", a_bcd, bonus_l7_index, bcd_val(current_score));

    /* ---- nivel 7: devuelve a CGA las dos regiones que guardo save_score_regions (0x8e4: 4x8 words, 0xc94: 20x8) */
    start_tick = 0; set_score(0); l7_obj_spawn_slot = 0xffff;
    SETUP(7, 0, 0, 1);
    for (size_t i = 0; i < sizeof cga_mem; i++) cga_mem[i] = (uint8_t)(i * 7 + 3);
    static uint8_t orig[CGA_MEM_SIZE]; memcpy(orig, cga_mem, sizeof orig);
    scribble = 1;
    handle_level_complete();
    scribble = 0;
    int ok = 1;
    for (int r = 0; r < 8; r++) {
        size_t o1 = 0x8e4 + (size_t)(r & 1) * 0x2000 + (size_t)(r >> 1) * 80;
        size_t o2 = 0xc94 + (size_t)(r & 1) * 0x2000 + (size_t)(r >> 1) * 80;
        ok &= memcmp(&cga_mem[o1], &orig[o1], 8) == 0;
        ok &= memcmp(&cga_mem[o2], &orig[o2], 40) == 0;
    }
    CHECK(ok, "n7: regiones restauradas con el contenido original");

    if (fails) { printf("test_level_complete: %d FALLOS\n", fails); return 1; }
    printf("test_level_complete: OK\n");
    return 0;
}
