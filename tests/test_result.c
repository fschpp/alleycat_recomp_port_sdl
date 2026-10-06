/* T46 — show_level_result / draw_result_frame (enemy.asm L275-353). Sin SDL. Mascaras esperadas de un modelo
 * Python independiente del ASM. PROGRESS.md §6aq. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

extern uint16_t result_dissolve_mask, result_sprite_ptr; extern uint8_t result_frame_counter;
enum { E_INIT = 1, E_NOTE, E_EXTRA, E_SIL, E_LOVE };
static int tr[200], ntr; static uint16_t masks[64]; static int nmask; static uint16_t sprites[64];
static uint8_t first_region[12][16]; static int have_first;
static void ev(int e) { if (ntr < 200) tr[ntr++] = e; }
static size_t row_off(int r) { return (size_t)(r & 1) * 0x2000 + (size_t)(r >> 1) * 80; }
void __wrap_init_result_melody(void) { ev(E_INIT); }
void __wrap_show_extra_life(void)    { ev(E_EXTRA); }
void __wrap_silence_speaker(void)    { ev(E_SIL); }
void __wrap_love_scene_outro(void)   { ev(E_LOVE); }
void __wrap_play_result_note(void) {
    ev(E_NOTE);
    if (nmask < 64) { masks[nmask] = result_dissolve_mask; sprites[nmask] = result_sprite_ptr; nmask++; }
    if (!have_first) { have_first = 1; for (int r = 0; r < 12; r++) memcpy(first_region[r], &cga_mem[row_off(94 + r) + 32], 16); }
}
static uint16_t fake_tick; static uint16_t tickfn(void) { return fake_tick++; }

static const uint16_t exp_masks[28] = { 0x8080, 0xc0c0, 0xe0e0, 0xf0f0, 0xf8f8, 0xfcfc, 0xfefe, 0xffff, 0xffff, 0xffff, 0xaaaa, 0xaaaa, 0xffff, 0xffff, 0x5555, 0x5555, 0xffff, 0xffff, 0xaaaa, 0xaaaa, 0xffff, 0xffff, 0x5555, 0x5555, 0xffff, 0xffff, 0xaaaa, 0xaaaa };

#define RUN(obj, lvlst, diff, lives, timer) do { \
    ntr = 0; nmask = 0; have_first = 0; fake_tick = 100; game_tick_fn = tickfn; \
    object_hit = (obj); level_state = (lvlst); difficulty_level = (diff); lives_count = (lives); game_timer = (timer); \
    memset(cga_mem, 0, sizeof cga_mem); show_level_result(); } while (0)

static uint16_t dsw(uint16_t o) { return (uint16_t)(ds_pool[o] | (ds_pool[o + 1] << 8)); }

int main(void) {
    /* level_state == 7: solo love_scene_outro */
    RUN(0, 7, 0, 3, 0);
    CHECK(ntr == 1 && tr[0] == E_LOVE, "nivel 7: solo love_scene_outro (ntr=%d)", ntr);

    /* object_hit == 0: sprite por defecto 0x185b, sin tocar vidas ni game_timer; 28 cuadros */
    RUN(0, 3, 0, 3, 4);
    CHECK(tr[0] == E_INIT && tr[ntr - 1] == E_SIL && ntr == 1 + 28 + 1, "object_hit=0: traza (ntr=%d)", ntr);
    CHECK(lives_count == 3 && game_timer == 4, "object_hit=0: vidas %d timer %d", lives_count, game_timer);
    CHECK(nmask == 28, "28 notas (%d)", nmask);
    for (int i = 0; i < nmask && i < 28; i++) {
        CHECK(masks[i] == exp_masks[i], "mascara cuadro %d: 0x%04x != 0x%04x", i, masks[i], exp_masks[i]);
        CHECK(sprites[i] == 0x185b, "sprite cuadro %d: 0x%04x", i, sprites[i]);
    }
    /* cuadro 1: region de pantalla = sprite & 0x80 por byte; ultimo cuadro (mascara 0xaaaa) = sprite & 0xaa; ademas un cuadro con 0xffff (cuadro 8) = sprite exacto */
    int bad = 0;
    for (int r = 0; r < 12; r++) for (int b = 0; b < 16; b++)
        if (first_region[r][b] != (ds_pool[0x185b + r * 16 + b] & 0x80)) bad++;
    CHECK(bad == 0, "cuadro 1: %d bytes distintos de sprite&0x80", bad);
    bad = 0;
    for (int r = 0; r < 12; r++) for (int b = 0; b < 16; b++)
        if (cga_mem[row_off(94 + r) + 32 + b] != (ds_pool[0x185b + r * 16 + b] & 0xaa)) bad++;
    CHECK(bad == 0, "ultimo cuadro: %d bytes distintos de sprite&0xaa", bad);

    /* object_hit != 0: sprite de result_sprite_table[game_timer&6], timer+=2, vidas-1 (no baja de 0) */
    for (int t = 0; t < 8; t += 2) {
        RUN(1, 3, 0, 3, (uint16_t)(t + 0x10));
        CHECK(sprites[0] == dsw((uint16_t)(0x1c26 + t)), "timer %d: sprite 0x%04x", t, sprites[0]);
        CHECK(game_timer == t + 0x12 && lives_count == 2, "timer %d: efectos timer=%d lives=%d", t, game_timer, lives_count);
        CHECK(nmask == 28 && ntr == 30, "timer %d: traza", t);
    }
    RUN(1, 3, 0, 0, 0);
    CHECK(lives_count == 0 && nmask == 28, "vidas 0 no baja");

    /* 0xdd + difficulty != 0 + vidas restantes >= 1: show_extra_life, sin cuadros */
    RUN(0xdd, 3, 1, 3, 0);
    CHECK(ntr == 3 && tr[0] == E_INIT && tr[1] == E_EXTRA && tr[2] == E_SIL && lives_count == 2, "0xdd extra vida (ntr=%d lives=%d)", ntr, lives_count);
    RUN(0xdd, 3, 1, 2, 0);       /* 2 -> 1: queda >= 1, extra vida */
    CHECK(ntr == 3 && tr[1] == E_EXTRA && lives_count == 1, "0xdd vidas 2->1: extra vida");
    RUN(0xdd, 3, 1, 1, 0);       /* 1 -> 0: jb, cuadros normales */
    CHECK(nmask == 28 && lives_count == 0, "0xdd con vidas 1->0: cuadros");
    RUN(0xdd, 3, 0, 3, 0);       /* dificultad 0: cuadros */
    CHECK(nmask == 28 && lives_count == 2, "0xdd dificultad 0: cuadros");
    RUN(0xdc, 3, 1, 3, 0);       /* != 0xdd */
    CHECK(nmask == 28, "0xdc: cuadros");

    if (fails) { printf("%d fallos\n", fails); return 1; }
    printf("test_result: OK\n");
    return 0;
}
