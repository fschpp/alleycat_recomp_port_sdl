/* T40 — game_start / game_flow_run (entry.asm L27-143). Sin SDL.
 * Las llamadas a otras unidades se envuelven con --wrap y se registran en una traza;
 * la traza esperada esta escrita a mano desde el ASM (no desde game_flow.c). Ver PROGRESS.md §6ak. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "level7_epilogue.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

enum { E_SETPAL = 1, E_CLRHS, E_CLRSC, E_UPDHS, E_SIL, E_TITLE, E_ATTRACT, E_CLRSCREEN, E_RENDER,
       E_SETUP_ALLEY, E_SETUP_LEVEL, E_INIT_SOUND, E_INIT_PLAYER, E_RESET_JUMP, E_INIT_OBJ,
       E_DRAW_SCORE, E_DRAW_HS, E_INIT_MUSIC };
static int tr[256]; static int ntr;
static void ev(int e) { if (ntr < 256) tr[ntr++] = e; }
static void reset_trace(void) { ntr = 0; }

/* --- wraps --- */
void __real_clear_score(void); void __real_clear_high_score(void); void __real_update_high_score(void);
void __real_setup_alley(void); void __real_setup_level(void);
void __wrap_set_palette(void)            { ev(E_SETPAL); }
void __wrap_clear_high_score(void)       { ev(E_CLRHS); __real_clear_high_score(); }
void __wrap_clear_score(void)            { ev(E_CLRSC); __real_clear_score(); }
void __wrap_update_high_score(void)      { ev(E_UPDHS); __real_update_high_score(); }
void __wrap_silence_speaker(void)        { ev(E_SIL); }
static uint16_t attract_sets_diff = 0xffff;                    /* simula la eleccion de dificultad (T54) */
void __wrap_show_title_screen(void)      { ev(E_TITLE); }
void __wrap_show_attract_mode(void)      { ev(E_ATTRACT); if (attract_sets_diff != 0xffff) diff_icon_idx = attract_sets_diff; }
void __wrap_clear_screen(void)           { ev(E_CLRSCREEN); }
void __wrap_render_sprites(void)         { ev(E_RENDER); }
void __wrap_setup_alley(void)            { ev(E_SETUP_ALLEY); __real_setup_alley(); }
void __wrap_setup_level(void)            { ev(E_SETUP_LEVEL); }
void __wrap_init_sound(void)             { ev(E_INIT_SOUND); }
void __wrap_init_player(void)            { ev(E_INIT_PLAYER); }
void __wrap_reset_jump(void)             { ev(E_RESET_JUMP); }
void __wrap_init_cycle_objects(void)     { ev(E_INIT_OBJ); }
void __wrap_draw_high_score_display(void){ ev(E_DRAW_SCORE); }
void __wrap_draw_current_score(void)     { ev(E_DRAW_HS); }
void __wrap_init_music(void)             { ev(E_INIT_MUSIC); }

static uint16_t fake_tick = 0x1234, fake_pit = 0x0abc;
static uint16_t tick_fn(void) { return fake_tick; }
static uint16_t pit_fn(void)  { return fake_pit; }

static void check_trace(const char *name, const int *exp, int n) {
    int ok = (ntr == n) && memcmp(tr, exp, (size_t)n * sizeof(int)) == 0;
    CHECK(ok, "%s: traza distinta (obtenidas %d, esperadas %d)", name, ntr, n);
    if (!ok) {
        printf("  got:"); for (int i = 0; i < ntr; i++) printf(" %d", tr[i]);
        printf("\n  exp:"); for (int i = 0; i < n; i++) printf(" %d", exp[i]); printf("\n");
    }
}

/* Tramo comun lab_00ae..lab_0140 (callejon), segun el ASM. */
#define NEWGAME   E_CLRSC, E_SETPAL, E_SIL, E_SIL
#define ALLEY     E_CLRSCREEN, E_RENDER, E_SIL, E_SETUP_ALLEY
#define TAIL      E_INIT_SOUND, E_INIT_PLAYER, E_RESET_JUMP, E_INIT_OBJ, E_DRAW_SCORE, E_DRAW_HS, E_INIT_MUSIC

static void dirty_state(void) {
    lives_count = 9; difficulty_level = 7; level_number = 5; game_timer = 99; start_tick = 5;
    l7_completion_counter = 7; force_level7 = 1; start_in_level = 1; restart_game = true; show_attract = true;
    attract_shown = 1; last_level = 3; prev_level = 4; sound_enabled = 0; round_counter = 8; video_mode = 1;
    use_joystick = 1; lives_display = 0; diff_icon_idx = 6; cat_x = 200; game_mode = 9; anim_counter = 0; anim_step = 0;
}

int main(void) {
    game_tick_fn = tick_fn; pit_counter_fn = pit_fn;

    /* --- A: entry completo, con el attract fijando dificultad 5 --- */
    dirty_state(); start_in_level = 0; restart_game = false; show_attract = false; rom_id = 0xff;
    attract_sets_diff = 5; rng_seed = 1; reset_trace();
    game_start();
    { const int e[] = { E_SETPAL, E_CLRHS, E_CLRSC,                      /* entry */
                        E_UPDHS, E_SETPAL, E_SIL, E_TITLE, E_SIL,         /* lab_0081 */
                        E_SIL, E_ATTRACT,                                 /* lab_00a3 (attract_shown==0) */
                        NEWGAME, ALLEY, TAIL };
      check_trace("A entry", e, (int)(sizeof e / sizeof e[0])); }
    CHECK(lives_count == 3, "A lives_count=%d", lives_count);
    CHECK(difficulty_level == 5, "A difficulty_level=%u (debe copiar difficulty_counter)", difficulty_level);
    CHECK(level_number == 0, "A level_number=%d", level_number);
    CHECK(game_timer == 0 && round_counter == 0 && l7_completion_counter == 0, "A timers game=%u round=%u elapsed=%u", game_timer, round_counter, l7_completion_counter);
    CHECK(start_tick == 0x1234, "A start_tick=0x%x", start_tick);
    CHECK(force_level7 == 0 && start_in_level == 0, "A force_level7/start_in_level");
    CHECK(!restart_game && !show_attract, "A restart/show_attract");
    CHECK(attract_shown == 1, "A attract_shown=%d", attract_shown);
    CHECK(last_level == 0xffff && prev_level == 0xffff, "A last/prev=%x/%x", last_level, prev_level);
    CHECK(sound_enabled == 0xff, "A sound_enabled=0x%x", sound_enabled);
    CHECK(video_mode == 6, "A video_mode=%d (rom_id 0xff -> 6)", video_mode);
    CHECK(use_joystick == 0, "A use_joystick");
    CHECK(rng_seed == 0x0abc, "A rng_seed=0x%x (PIT)", rng_seed);
    CHECK(lives_display == 0xff, "A lives_display=0x%x", lives_display);
    CHECK(cat_x == 0 && cat_y == 0xb4 && cat_y_bottom == 0xe6, "A cat pos %d,%d,%d", cat_x, cat_y, cat_y_bottom);
    CHECK(scroll_direction == 1 && game_mode == 0 && anim_counter == 1, "A setup_alley real (scroll_dir=%d mode=%d anim=%d)", scroll_direction, game_mode, anim_counter);

    /* video_mode con ROM PCjr */
    rom_id = 0xfd; attract_shown = 0; game_start();
    CHECK(video_mode == 4, "A2 video_mode PCjr=%d", video_mode);
    rom_id = 0xff;

    /* --- B: entry con titulo ya mostrado antes (attract_shown=1): salta el attract.
     *        lab_0081 -> lab_00ae --- */
    dirty_state(); restart_game = false; show_attract = false; start_in_level = 0; attract_shown = 1;
    reset_trace(); game_flow_run(GF_LAB_0081);
    { const int e[] = { E_UPDHS, E_SETPAL, E_SIL, E_TITLE, E_SIL, NEWGAME, ALLEY, TAIL };
      check_trace("B 0081 con attract_shown", e, (int)(sizeof e / sizeof e[0])); }
    CHECK(attract_shown == 1 && lives_count == 3 && difficulty_level == 6, "B difficulty_level debe ser difficulty_counter=6 (era %u)", difficulty_level);

    /* --- C: lab_00f3 con start_in_level=1: setup_level + game_mode=2, anim 1/0x20 (sin setup_alley) --- */
    dirty_state(); restart_game = false; show_attract = false; lives_count = 2; start_in_level = 1;
    reset_trace(); game_flow_run(GF_LAB_00F3);
    { const int e[] = { E_CLRSCREEN, E_RENDER, E_SIL, E_SETUP_LEVEL, TAIL };
      check_trace("C 00f3 start_in_level", e, (int)(sizeof e / sizeof e[0])); }
    CHECK(game_mode == 2 && anim_counter == 1 && anim_step == 0x20, "C mode=%d anim=%d step=0x%x", game_mode, anim_counter, anim_step);
    CHECK(level_number == 0 && lives_display == 0xff && lives_count == 2, "C level_number/lives_display/lives_count (no debe tocar vidas)");
    CHECK(cat_x == 200, "C cat_x=%d (la rama setup_level no pone cat_x=0)", cat_x);

    /* --- D: lab_00f3 sin start_in_level: cat_x=0 y setup_alley --- */
    dirty_state(); restart_game = false; show_attract = false; lives_count = 2; start_in_level = 0;
    reset_trace(); game_flow_run(GF_LAB_00F3);
    { const int e[] = { ALLEY, TAIL }; check_trace("D 00f3 callejon", e, (int)(sizeof e / sizeof e[0])); }
    CHECK(cat_x == 0, "D cat_x=%d", cat_x);

    /* --- E: ramas de salida de lab_00f3 --- */
    dirty_state(); restart_game = false; show_attract = false; lives_count = 0; start_in_level = 0; attract_shown = 1;
    reset_trace(); game_flow_run(GF_LAB_00F3);                 /* game over -> lab_0081 -> (attract_shown) -> lab_00ae */
    { const int e[] = { E_UPDHS, E_SETPAL, E_SIL, E_TITLE, E_SIL, NEWGAME, ALLEY, TAIL };
      check_trace("E1 game over", e, (int)(sizeof e / sizeof e[0])); }
    CHECK(lives_count == 3, "E1 lives_count=%d", lives_count);

    dirty_state(); restart_game = true; show_attract = false; lives_count = 1; start_in_level = 1; attract_shown = 1;
    reset_trace(); game_flow_run(GF_LAB_00F3);                 /* restart -> lab_00ae (sin titulo) */
    { const int e[] = { NEWGAME, ALLEY, TAIL }; check_trace("E2 restart", e, (int)(sizeof e / sizeof e[0])); }
    CHECK(lives_count == 3 && !restart_game && start_in_level == 0, "E2 lives=%d restart=%d start_in_level=%d", lives_count, restart_game, start_in_level);

    dirty_state(); restart_game = false; show_attract = true; lives_count = 1; start_in_level = 0; attract_shown = 0;
    reset_trace(); game_flow_run(GF_LAB_00F3);                 /* timeout -> lab_00a3 -> lab_00ae */
    { const int e[] = { E_SIL, E_ATTRACT, NEWGAME, ALLEY, TAIL }; check_trace("E3 attract por timeout", e, (int)(sizeof e / sizeof e[0])); }
    CHECK(attract_shown == 1 && !show_attract, "E3 attract_shown/show_attract");

    /* prioridad: lives==0 gana a restart y a show_attract */
    dirty_state(); restart_game = true; show_attract = true; lives_count = 0; start_in_level = 0; attract_shown = 1;
    reset_trace(); game_flow_run(GF_LAB_00F3);
    CHECK(ntr > 0 && tr[0] == E_UPDHS, "E4 lives==0 debe ir a lab_0081 aunque haya restart/show_attract");

    /* --- F: read_pit_counter --- */
    fake_pit = 0; rng_seed = 1; read_pit_counter();
    CHECK(rng_seed == 0xfa59, "F pit=0 -> 0xfa59 (era 0x%x)", rng_seed);
    fake_pit = 0x8001; read_pit_counter();
    CHECK(rng_seed == 0x8001, "F pit=0x8001 -> semilla (era 0x%x)", rng_seed);

    if (fails) { printf("test_game_flow: %d FALLOS\n", fails); return 1; }
    printf("test_game_flow: OK\n");
    return 0;
}
