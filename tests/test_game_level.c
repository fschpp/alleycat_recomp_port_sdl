/* T42 — despacho de nivel (lab_0238) y loops de los niveles 7, 6 y 5 (entry.asm L237-340) + lab_0427.
 * Sin SDL. Las trazas esperadas estan escritas a mano desde el ASM (orden de los `call`).
 * Ver PROGRESS.md §6am. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

enum { E_TRANS = 1, E_BG, E_SETUP, E_SOUND_INIT, E_THR_INIT, E_CUPID_RESET, E_L7_INIT, E_MUSIC,
       E_L5_INIT, E_KEYS, E_PLAY, E_ANIM, E_CUPID, E_L7TICK, E_L7SPAWN, E_L7UPD, E_L6MOVE, E_L6TIME,
       E_ENEMIES, E_THR_TICK, E_L5OBJ, E_L5ANIM };
static int tr[64]; static int ntr;
static void ev(int e) { if (ntr < 64) tr[ntr++] = e; }
void input_process_keys(void)              { ev(E_KEYS); }
void __wrap_level_transition(void)         { ev(E_TRANS); }
void __wrap_draw_level_background(void)    { ev(E_BG); }
void __wrap_setup_level(void)              { ev(E_SETUP); }
void __wrap_init_sound(void)               { ev(E_SOUND_INIT); }
void __wrap_init_thrown_objects(void)      { ev(E_THR_INIT); }
void __wrap_reset_cupid(void)              { ev(E_CUPID_RESET); }
void __wrap_init_level7_objects(void)      { ev(E_L7_INIT); }
void __wrap_init_music(void)               { ev(E_MUSIC); }
void __wrap_init_level5_objects(void)      { ev(E_L5_INIT); }
void __wrap_play_sound(void)               { ev(E_PLAY); }
void __wrap_update_alley_movement(void)    { ev(E_ANIM); }
void __wrap_update_cupid(void)             { ev(E_CUPID); }
void __wrap_tick_level_thrown_objects(void){ ev(E_L7TICK); }
void __wrap_spawn_thrown_object(void)      { ev(E_L7SPAWN); }
void __wrap_update_level7_objects(void)    { ev(E_L7UPD); }
void __wrap_update_level6_movement(void)   { ev(E_L6MOVE); }
void __wrap_update_level6_timing(void)     { ev(E_L6TIME); }
void __wrap_update_enemies(void)           { ev(E_ENEMIES); }
void __wrap_tick_thrown_objects(void)      { ev(E_THR_TICK); }
void __wrap_update_level5_objects(void)    { ev(E_L5OBJ); }
void __wrap_update_level5_anim(void)       { ev(E_L5ANIM); }

static void check_trace(const char *name, const int *exp, int n) {
    int ok = (ntr == n) && (n == 0 || memcmp(tr, exp, (size_t)n * sizeof(int)) == 0);
    CHECK(ok, "%s: traza distinta (obtenidas %d, esperadas %d)", name, ntr, n);
    if (!ok) { printf("  got:"); for (int i = 0; i < ntr; i++) printf(" %d", tr[i]);
               printf("\n  exp:"); for (int i = 0; i < n; i++) printf(" %d", exp[i]); printf("\n"); }
}
#define TRACE(name, ...) do { int e_[] = { __VA_ARGS__ }; check_trace(name, e_, (int)(sizeof e_ / sizeof e_[0])); } while (0)

static void clear_flags(void) {
    cat_died = object_hit = cat_caught = 0; restart_game = show_attract = false; enemy_active = 0;
}

int main(void) {
    /* ---- despacho: niveles 0-4 son de T43 (false, level_state=0, sin llamadas); 9 -> clamp a 0 ---- */
    for (int n = 0; n <= 4; n++) {
        ntr = 0; level_state = 0x1234; level_number = (int16_t)n;
        CHECK(!game_level_enter(), "nivel %d no debe estar portado todavia", n);
        CHECK(level_state == 0, "nivel %d: level_state debe quedar 0", n);
        CHECK(ntr == 0, "nivel %d: no debe llamar nada", n);
    }
    ntr = 0; level_state = 0x1234; level_number = 9;
    CHECK(!game_level_enter() && ntr == 0 && level_state == 0, "level_number 9 -> clamp a 0 (jbe, sin signo)");
    ntr = 0; level_number = (int16_t)0xffff;   /* 0xffff > 7 sin signo -> tambien clamp a 0, no indice negativo */
    CHECK(!game_level_enter() && ntr == 0, "level_number 0xffff -> clamp a 0 sin signo");

    /* ---- init de cada nivel (orden de los `call`, entry.asm) ---- */
    ntr = 0; level_number = 7; level_state = 0x99;
    CHECK(game_level_enter(), "nivel 7");
    CHECK(level_number == 7 && level_state == 0, "nivel 7: level_number/level_state");
    TRACE("init L7 (lab_0260)", E_TRANS, E_BG, E_SETUP, E_SOUND_INIT, E_THR_INIT, E_CUPID_RESET, E_L7_INIT, E_MUSIC);

    ntr = 0; level_number = 6;
    CHECK(game_level_enter(), "nivel 6");
    CHECK(level_number == 6, "nivel 6: level_number");
    /* level6_stubs es un `ret` desnudo: no aparece en la traza */
    TRACE("init L6 (lab_02aa)", E_TRANS, E_BG, E_SETUP, E_THR_INIT, E_SOUND_INIT, E_MUSIC);

    ntr = 0; level_number = 5;
    CHECK(game_level_enter(), "nivel 5");
    CHECK(level_number == 5, "nivel 5: level_number");
    TRACE("init L5 (lab_02fe)", E_TRANS, E_BG, E_L5_INIT, E_SETUP, E_THR_INIT, E_SOUND_INIT, E_MUSIC);

    /* ---- una pasada de cada loop ---- */
    clear_flags(); ntr = 0; level_number = 7;
    CHECK(game_level_frame() == GL_STAY, "L7 sin flags sigue");
    TRACE("frame L7 (lab_027e)", E_KEYS, E_PLAY, E_ANIM, E_CUPID, E_L7TICK, E_L7SPAWN, E_L7UPD);

    clear_flags(); ntr = 0; level_number = 6; enemy_active = 0;
    CHECK(game_level_frame() == GL_STAY, "L6 sin flags sigue");
    TRACE("frame L6 enemy_active=0 (lab_02c5)", E_KEYS, E_PLAY, E_L6MOVE, E_L6TIME, E_ANIM, E_THR_TICK);
    ntr = 0; enemy_active = 1;
    CHECK(game_level_frame() == GL_STAY, "L6 con enemigo sigue");
    TRACE("frame L6 enemy_active=1", E_KEYS, E_PLAY, E_L6MOVE, E_L6TIME, E_ANIM, E_ENEMIES);

    clear_flags(); ntr = 0; level_number = 5;
    CHECK(game_level_frame() == GL_STAY, "L5 sin flags sigue");
    TRACE("frame L5 (lab_0319)", E_KEYS, E_PLAY, E_L5OBJ, E_L5ANIM, E_ANIM, E_THR_TICK, E_ENEMIES);

    /* ---- condiciones de salida: cada nivel mira un conjunto distinto de flags ---- */
    struct { int lvl; const char *flag; } flags_in[] = {
        {7,"cat_died"},{7,"cat_caught"},{7,"show_attract"},{7,"restart_game"},{7,"object_hit"},
        {6,"cat_died"},{6,"object_hit"},{6,"cat_caught"},{6,"restart_game"},{6,"show_attract"},
        {5,"object_hit"},{5,"cat_caught"},{5,"cat_died"},{5,"show_attract"},{5,"restart_game"} };
    for (unsigned i = 0; i < sizeof flags_in / sizeof flags_in[0]; i++) {
        clear_flags(); level_number = (int16_t)flags_in[i].lvl;
        const char *f = flags_in[i].flag;
        if (!strcmp(f, "cat_died")) cat_died = 0xff;
        else if (!strcmp(f, "cat_caught")) cat_caught = 0x01;
        else if (!strcmp(f, "object_hit")) object_hit = 0x80;
        else if (!strcmp(f, "show_attract")) show_attract = true;
        else restart_game = true;
        gl_next_t r = game_level_frame();
        /* el ASM del nivel 7 NO lee object_hit: no debe salir por ese flag */
        bool expect_exit = !(flags_in[i].lvl == 7 && !strcmp(f, "object_hit"));
        CHECK((r == GL_EXIT) == expect_exit, "L%d flag %s: salida=%d, esperado %d", flags_in[i].lvl, f, r == GL_EXIT, expect_exit);
    }

    /* ---- lab_0427 ---- */
    clear_flags(); restart_game = true; show_attract = true; ntr = 0; level_number = 6;
    CHECK(game_level_exit() == GF_TO_00AE, "restart tiene prioridad sobre attract");
    CHECK(ntr == 0 && level_number == 6, "restart: sin level_transition ni cambio de nivel");
    clear_flags(); show_attract = true; ntr = 0;
    CHECK(game_level_exit() == GF_TO_00A3 && ntr == 0, "attract -> lab_00a3");

    clear_flags(); ntr = 0; level_number = 5; start_in_level = 1; level_state = 0;
    CHECK(game_level_exit() == GF_TO_00F3, "salida normal -> lab_00f3");
    CHECK(start_in_level == 1, "sin object_hit start_in_level no se toca");
    CHECK(level_state == 5 && level_number == 0, "level_state = ultimo nivel, level_number = 0");
    TRACE("exit: level_transition", E_TRANS);

    clear_flags(); object_hit = 0x01; ntr = 0; level_number = 6; start_in_level = 1;
    CHECK(game_level_exit() == GF_TO_00F3, "object_hit -> lab_00f3");
    CHECK(start_in_level == 0 && level_state == 6 && level_number == 0, "object_hit: respawn en el callejon");

    if (fails == 0) printf("test_game_level: OK\n");
    return fails ? 1 : 0;
}
