/* T44 — level_transition (enemy.asm L110-158). Sin SDL; trazas escritas a mano desde el ASM. PROGRESS.md §6ao. */
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

enum { E_VICSEQ = 1, E_WIPE, E_SIL, E_PAL, E_MARCH, E_COMPLETE, E_RESULT };
static int tr[32]; static int ntr; static uint16_t wipe_pat[4]; static int nwipe;
static void ev(int e) { if (ntr < 32) tr[ntr++] = e; }
void __wrap_run_victory_sequence(void)  { ev(E_VICSEQ); }
void __wrap_animate_screen_wipe(void)   { ev(E_WIPE); if (nwipe < 4) wipe_pat[nwipe++] = wipe_fill_pattern; }
void __wrap_silence_speaker(void)       { ev(E_SIL); }
void __wrap_set_palette(void)           { ev(E_PAL); }
void __wrap_play_victory_march(void)    { ev(E_MARCH); }
void __wrap_handle_level_complete(void) { ev(E_COMPLETE); }
void __wrap_show_level_result(void)     { ev(E_RESULT); }

#define RUN(name, lvl, st, caught, ...) do { \
    int e_[] = { __VA_ARGS__ }; \
    ntr = 0; nwipe = 0; level_number = (lvl); level_state = (st); cat_caught = (caught); cat_x = 1; cat_y = 2; wipe_fill_pattern = 0x1234; \
    level_transition(); \
    int ok = (ntr == (int)(sizeof e_ / sizeof e_[0])) && memcmp(tr, e_, sizeof e_) == 0; \
    CHECK(ok, "%s: traza distinta", name); \
    if (!ok) { printf("  got:"); for (int i = 0; i < ntr; i++) printf(" %d", tr[i]); printf("\n"); } } while (0)

int main(void) {
    /* entrar a un nivel (level_state=0): barrido a 0, silencio, paleta, barrido con el patron del nivel, silencio */
    for (int n = 1; n <= 7; n++) {
        RUN("entrar", n, 0, 0, E_WIPE, E_SIL, E_PAL, E_WIPE, E_SIL);
        uint16_t exp = (n == 7 || n == 2) ? 0x5555 : 0xaaaa;
        CHECK(wipe_pat[0] == 0x0000, "nivel %d: primer barrido con patron 0 (0x%04x)", n, wipe_pat[0]);
        CHECK(wipe_pat[1] == exp, "nivel %d: segundo barrido 0x%04x, esperado 0x%04x", n, wipe_pat[1], exp);
        CHECK(cat_x == 1 && cat_y == 2, "nivel %d: no toca la posicion del gato", n);
    }
    /* cat_caught distinto de 0 al ENTRAR a un nivel (level_number != 0, level_state != 7) no cambia nada */
    RUN("entrar con cat_caught", 3, 0, 1, E_WIPE, E_SIL, E_PAL, E_WIPE, E_SIL);

    /* salir al callejon (level_number=0): hay una pantalla extra segun cat_caught / level_state */
    RUN("salir sin completar", 0, 4, 0, E_WIPE, E_SIL, E_PAL, E_RESULT, E_WIPE, E_SIL);
    CHECK(wipe_pat[1] == 0xaaaa, "salir al callejon: patron 0xaaaa (0x%04x)", wipe_pat[1]);
    RUN("salir completando (no 7)", 0, 3, 1, E_WIPE, E_SIL, E_PAL, E_COMPLETE, E_WIPE, E_SIL);
    RUN("salir de 7 sin completar", 0, 7, 0, E_WIPE, E_SIL, E_PAL, E_RESULT, E_WIPE, E_SIL);
    RUN("salir de 7 completando", 0, 7, 1, E_VICSEQ, E_WIPE, E_SIL, E_PAL, E_MARCH, E_WIPE, E_SIL);
    CHECK(cat_x == 0x98 && cat_y == 0x5f, "tras run_victory_sequence el gato va a (0x98,0x5f): (%d,%d)", cat_x, cat_y);
    RUN("salir de 2 sin completar", 0, 2, 0, E_WIPE, E_SIL, E_PAL, E_RESULT, E_WIPE, E_SIL);
    /* level_state == 7 pero level_number != 0 (no ocurre en el juego): solo la secuencia de victoria */
    RUN("estado raro 7/5", 5, 7, 1, E_VICSEQ, E_WIPE, E_SIL, E_PAL, E_WIPE, E_SIL);

    if (fails == 0) printf("test_transition: OK\n");
    return fails ? 1 : 0;
}
