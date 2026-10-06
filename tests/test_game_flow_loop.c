/* T41 — loop del callejon, manejador de muerte y selector de nivel (entry.asm L144-236). Sin SDL.
 * Modelo del selector independiente (LFSR propio, tablas de cat.asm); trazas del loop escritas a mano
 * desde el ASM con --wrap. Ver PROGRESS.md §6al. */
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

enum { E_KEYS = 1, E_ANIM, E_ENEMIES, E_SOUND, E_THROWN, E_JUMP, E_GRAV, E_FALL, E_CYCLE, E_LIVES };
static int tr[256]; static int ntr;
static void ev(int e) { if (ntr < 256) tr[ntr++] = e; }
void input_process_keys(void)        { ev(E_KEYS); }
void __wrap_update_alley_movement(void) { ev(E_ANIM); }
void __wrap_update_enemies(void)     { ev(E_ENEMIES); }
void __wrap_play_sound(void)         { ev(E_SOUND); }
void __wrap_update_thrown_objects(void) { ev(E_THROWN); }
void __wrap_update_cat_jump(void)    { ev(E_JUMP); }
void __wrap_apply_cat_gravity(void)  { ev(E_GRAV); }
void __wrap_animate_falling(void)    { ev(E_FALL); }
void __wrap_update_cycle_objects(void) { ev(E_CYCLE); }
static int kill_lives_in_draw;
void __wrap_draw_lives(void)         { ev(E_LIVES); if (kill_lives_in_draw) lives_count = 0; }

static void check_trace(const char *name, const int *exp, int n) {
    int ok = (ntr == n) && (n == 0 || memcmp(tr, exp, (size_t)n * sizeof(int)) == 0);
    CHECK(ok, "%s: traza distinta (obtenidas %d, esperadas %d)", name, ntr, n);
    if (!ok) { printf("  got:"); for (int i = 0; i < ntr; i++) printf(" %d", tr[i]);
               printf("\n  exp:"); for (int i = 0; i < n; i++) printf(" %d", exp[i]); printf("\n"); }
}
static uint16_t fake_tick = 0x4321;
static uint16_t tick_fn(void) { return fake_tick; }

/* ---- modelo independiente del selector (desde entry.asm L212-234 y cat.asm L158-161) ---- */
static uint16_t mseed;
static uint16_t m_random(void) {                       /* cga.asm random */
    uint8_t lo = (uint8_t)(mseed & 0xff), hi = (uint8_t)(mseed >> 8);
    unsigned c = (unsigned)(((lo ^ hi) >> 1) & 1);
    mseed = (uint16_t)((mseed >> 1) | (c << 15));
    return mseed;
}
static const uint8_t POOL[12] = { 4,4,1,1,3,3,5,5,6,6,4,1 };
static const uint8_t HARD[5]  = { 1,3,4,5,6 };
static uint16_t m_last, m_prev;
static uint16_t m_select(uint16_t diff) {
    for (;;) {
        uint16_t dx = m_random(); uint16_t al; bool hard = false;
        if ((dx & 0xa0) == 0) hard = true;
        else if ((diff & 3) == 3) hard = true;
        else al = POOL[(diff & 3) * 4 + (dx & 3)];
        if (hard) { do { dx = m_random() & 7; } while (dx >= 5); al = HARD[dx]; }
        if (al == m_last && al == m_prev) continue;
        m_prev = m_last; m_last = al; return al;
    }
}

int main(void) {
    game_tick_fn = tick_fn;

    /* --- A: selector == modelo, con historial, 8 dificultades x 6 semillas x 4000 tiradas --- */
    const uint16_t seeds[6] = { 0x7777, 0xFA59, 0x1234, 0x8001, 0xFFFF, 0x0abc };
    /* NOTA: la semilla 1 (y el estado 0) es un punto fijo degenerado del LFSR: con rng_seed==0 el selector
     * del original re-sortea para siempre cuando last==prev; read_pit_counter evita sembrar 0, y 1 -> 0. No se usa. */
    long ncmp = 0; int hist_ok = 1;
    for (uint16_t d = 0; d < 8; d++) for (int s = 0; s < 6; s++) {
        rng_seed = mseed = seeds[s]; difficulty_level = d;
        last_level = m_last = 0xffff; prev_level = m_prev = 0xffff;
        for (int i = 0; i < 4000; i++) {
            uint16_t got = select_next_level(), exp = m_select(d);
            ncmp++;
            if (got != exp || rng_seed != mseed || last_level != m_last || prev_level != m_prev || level_number != (int16_t)exp) {
                CHECK(0, "A d=%u seed=0x%x i=%d: got %u exp %u (rng 0x%x/0x%x last %x/%x prev %x/%x)", d, seeds[s], i, got, exp, rng_seed, mseed, last_level, m_last, prev_level, m_prev);
                goto after_A;
            }
            if (!(got == 1 || got == 3 || got == 4 || got == 5 || got == 6)) hist_ok = 0;
        }
    }
after_A:
    CHECK(hist_ok, "A salio un nivel fuera de {1,3,4,5,6}");

    /* --- B: no se repite 3 veces seguidas (los dos ultimos distintos del nuevo) --- */
    rng_seed = 0x5a5a; difficulty_level = 1; last_level = prev_level = 0xffff;
    { uint16_t a = 0xffff, b = 0xffff; int bad = 0;
      for (int i = 0; i < 20000; i++) { uint16_t l = select_next_level(); if (l == a && l == b) bad++; b = a; a = l; }
      CHECK(bad == 0, "B %d repeticiones triples", bad); }
    /* re-roll forzado: last=prev=X; el resultado nunca puede ser X */
    for (int s = 1; s < 3000; s++) { rng_seed = (uint16_t)(s * 7919u + 1); difficulty_level = (uint16_t)(s & 3);
        last_level = prev_level = 4; uint16_t l = select_next_level(); if (l == 4) { CHECK(0, "B re-roll devolvio 4 con last=prev=4 (s=%d)", s); break; } }

    /* --- C: histograma de 100000 tiradas sin historial: puerto == modelo independiente (exacto).
     * Los pesos "teoricos" (dl uniforme e independiente: P(tabla hard)=1/4) se imprimen solo como referencia:
     * este LFSR desplaza 1 bit por llamada, asi que las tiradas consecutivas estan correlacionadas y la
     * distribucion real se aparta de ellos (el comentario del ASM habla de "~62%", no de 75%). --- */
    printf("histograma (100000 tiradas, sin historial) niveles 1/3/4/5/6: observado(ref. uniforme)\n");
    for (uint16_t d = 0; d < 4; d++) {
        long h[8] = {0}, hm[8] = {0};
        rng_seed = 0x2468; mseed = 0x2468; difficulty_level = d;
        for (int i = 0; i < 100000; i++) {
            last_level = prev_level = 0xffff; m_last = m_prev = 0xffff;
            h[select_next_level()]++; hm[m_select(d)]++;
        }
        double exp[8] = {0};
        for (int k = 0; k < 5; k++) exp[HARD[k]] += (d == 3 ? 1.0 : 0.25) / 5.0;
        if (d != 3) for (int k = 0; k < 4; k++) exp[POOL[d * 4 + k]] += 0.75 / 4.0;
        printf("  dif %u:", d);
        for (int l = 1; l <= 6; l++) if (l != 2) printf(" %d=%.3f(%.3f)", l, (double)h[l] / 100000.0, exp[l]);
        printf("\n");
        CHECK(memcmp(h, hm, sizeof h) == 0, "C dif %u: histograma distinto del modelo", d);
        CHECK(h[0] == 0 && h[2] == 0 && h[7] == 0, "C dif %u: salieron niveles 0/2/7", d);
        long tot = 0; for (int l = 0; l < 8; l++) tot += h[l];
        CHECK(tot == 100000, "C dif %u: total %ld", d, tot);
    }
    /* dificultad 3 y >=4: la 3 no tiene tabla (solo hard); 4..7 se reducen con &3 */
    { long hh[8] = {0}; rng_seed = 0x3333; difficulty_level = 3;
      for (int i = 0; i < 20000; i++) { last_level = prev_level = 0xffff; hh[select_next_level()]++; }
      CHECK(hh[1] && hh[3] && hh[4] && hh[5] && hh[6], "C dif 3 debe cubrir los 5 niveles de la tabla hard"); }

    /* --- D: manejador de muerte --- */
    cat_x = 123; cat_y = 77; saved_cat_x = 0; saved_cat_y = 0; start_in_level = 0; force_level7 = 0;
    rng_seed = mseed = 0x1357; difficulty_level = 2; last_level = m_last = 3; prev_level = m_prev = 1; level_number = 0;
    game_death_handler();
    { uint16_t e = m_select(2);
      CHECK(game_tick == 0x4321 && saved_cat_x == 123 && saved_cat_y == 77 && start_in_level == 1, "D estado: tick=%x sx=%d sy=%d sil=%d", game_tick, saved_cat_x, saved_cat_y, start_in_level);
      CHECK(level_number == (int16_t)e && last_level == m_last && prev_level == m_prev && rng_seed == mseed, "D eleccion level=%d exp=%u last=%u prev=%u", level_number, e, last_level, prev_level); }
    /* force_level7: level 7, bandera a 0, sin tocar historial ni rng */
    force_level7 = 1; last_level = 5; prev_level = 6; rng_seed = 0x1111; start_in_level = 0; fake_tick = 0x0777;
    game_death_handler();
    CHECK(level_number == 7 && force_level7 == 0 && last_level == 5 && prev_level == 6 && rng_seed == 0x1111 && start_in_level == 1 && game_tick == 0x0777,
          "D force7: level=%d force=%d last=%u prev=%u rng=%x sil=%d tick=%x", level_number, force_level7, last_level, prev_level, rng_seed, start_in_level, game_tick);

    /* --- E: pasada del loop --- */
    memset(tr, 0, sizeof tr);
#define RESET() do { ntr = 0; } while (0)
#define BASE() do { lives_count = 3; show_attract = false; restart_game = false; cat_died = 0; enemy_active = 0; frame_counter = 0; immune_flag = 5; } while (0)
    BASE(); lives_count = 0; RESET(); CHECK(game_alley_frame() == GF_TO_0081 && ntr == 0, "E1 lives==0 -> 0081 sin llamar a nada (ntr=%d)", ntr);
    BASE(); show_attract = true; restart_game = true; RESET();
    CHECK(game_alley_frame() == GF_TO_00A3, "E2 show_attract gana a restart");
    { const int e[] = { E_KEYS }; check_trace("E2", e, 1); }
    BASE(); restart_game = true; RESET();
    CHECK(game_alley_frame() == GF_TO_00AE, "E3 restart -> 00AE");
    { const int e[] = { E_KEYS }; check_trace("E3", e, 1); }

    /* sin enemigo: 8 frames; la fisica corre solo cuando frame_counter&3 == 0 (frames 4 y 8) */
    BASE(); enemy_active = 0; RESET(); int ran = 0, stay = 0;
    for (int f = 1; f <= 8; f++) {
        int before = ntr; gf_next_t r = game_alley_frame(); stay += (r == GF_STAY);
        int n = ntr - before; int physics = (n == 3 + 7);
        if (n != 3 && n != 10) CHECK(0, "E4 frame %d: %d llamadas", f, n);
        CHECK(physics == ((f & 3) == 0), "E4 frame %d fisica=%d (esperada %d)", f, physics, (f & 3) == 0);
        ran += physics;
    }
    CHECK(ran == 2 && stay == 8 && frame_counter == 8, "E4 ran=%d stay=%d frame_counter=%d", ran, stay, frame_counter);
    CHECK(immune_flag == 0, "E4 immune_flag=%d", immune_flag);
    { const int e[] = { E_KEYS, E_ANIM, E_ENEMIES, E_KEYS, E_ANIM, E_ENEMIES, E_KEYS, E_ANIM, E_ENEMIES,
                        E_KEYS, E_ANIM, E_ENEMIES, E_SOUND, E_THROWN, E_JUMP, E_GRAV, E_FALL, E_CYCLE, E_LIVES };
      int save = ntr; ntr = 0; (void)save;
      /* reconstruye los primeros 4 frames para comparar la traza exacta */
      BASE(); enemy_active = 0; RESET(); for (int f = 0; f < 4; f++) game_alley_frame();
      check_trace("E4 4 frames", e, (int)(sizeof e / sizeof e[0])); }

    /* con enemigo: fisica en cada frame y frame_counter intacto */
    BASE(); enemy_active = 1; frame_counter = 9; RESET();
    CHECK(game_alley_frame() == GF_STAY && frame_counter == 9, "E5 con enemigo: frame_counter=%d", frame_counter);
    { const int e[] = { E_KEYS, E_ANIM, E_ENEMIES, E_SOUND, E_THROWN, E_JUMP, E_GRAV, E_FALL, E_CYCLE, E_LIVES }; check_trace("E5", e, 10); }

    /* cat_died (solo se mira tras la fisica): sin vidas -> 0081 y sin manejador; con vidas -> 0238 */
    BASE(); enemy_active = 1; cat_died = 1; lives_count = 3; start_in_level = 0; force_level7 = 0; cat_x = 55; cat_y = 66;
    rng_seed = 0x2222; difficulty_level = 0; last_level = prev_level = 0xffff; RESET();
    { gf_next_t r = game_alley_frame();
      CHECK(r == GF_TO_0238 && start_in_level == 1 && saved_cat_x == 55 && saved_cat_y == 66 && level_number != 0, "E6 muerte con vidas: r=%d sil=%d lvl=%d", r, start_in_level, level_number);
      CHECK(level_number == 1 || level_number == 3 || level_number == 4 || level_number == 5 || level_number == 6, "E6 level_number=%d", level_number); }
    BASE(); enemy_active = 1; cat_died = 1; lives_count = 0; start_in_level = 0; RESET();
    { gf_next_t r = game_alley_frame();                    /* lives_count==0 se mira al inicio: no corre nada */
      CHECK(r == GF_TO_0081 && start_in_level == 0 && ntr == 0, "E7 lives==0 con cat_died: r=%d sil=%d ntr=%d", r, start_in_level, ntr); }
    /* las vidas llegan a 0 durante la fisica: tras cat_died, lives==0 -> 0081 sin pasar por lab_01b7 */
    BASE(); enemy_active = 1; cat_died = 1; lives_count = 3; start_in_level = 0; saved_cat_x = -1; kill_lives_in_draw = 1; RESET();
    { gf_next_t r = game_alley_frame(); kill_lives_in_draw = 0;
      CHECK(r == GF_TO_0081 && start_in_level == 0 && saved_cat_x == -1, "E8 vidas a 0 en la fisica: r=%d sil=%d sx=%d", r, start_in_level, saved_cat_x); }

    if (fails) { printf("test_game_flow_loop: %d FALLOS\n", fails); return 1; }
    printf("test_game_flow_loop: OK (%ld comparaciones del selector)\n", ncmp);
    return 0;
}
