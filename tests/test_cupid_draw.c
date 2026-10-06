/* T57 — draw_cupid, erase_cupid, cupid_toggle_window, check_cupid_collision (ui.asm L676-791). Sin SDL.
 * draw_bg_tile y start_tone se envuelven y se registran. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "cupid.h"
#include "level_collision.h"
#include "input.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static int tile_calls; static uint16_t tile_x, tile_bx; static uint8_t tile_y;
static int region_clean_at_tile;                 /* ¿el fondo estaba restaurado (erase_cupid ya corrio) cuando se llamo a draw_bg_tile? */
static uint8_t original[sizeof cga_mem]; static size_t watch_addr;
static int tone_calls; static uint16_t tone_a, tone_b;
static int region_equal(const uint8_t *a, size_t addr) {      /* 2 words x 8 filas desde addr */
    size_t off = addr;
    for (int r = 0; r < 8; r++) {
        if (memcmp(&cga_mem[off], &a[off], 4) != 0) return 0;
        off = (off & 0x2000) ? (off ^ 0x2000) + 80 : off ^ 0x2000;
    }
    return 1;
}
void __wrap_draw_bg_tile(uint16_t x, uint8_t y, uint16_t bx) {
    tile_calls++; tile_x = x; tile_y = y; tile_bx = bx;
    region_clean_at_tile = region_equal(original, watch_addr);
}
void __wrap_start_tone(uint16_t a, uint16_t b) { tone_calls++; tone_a = a; tone_b = b; }

static void pattern(void) { for (size_t i = 0; i < sizeof cga_mem; i++) cga_mem[i] = (uint8_t)(i * 13 + (i >> 7) + 5); }
static const uint8_t ROW_Y[7]  = { 0xb0, 0x98, 0x80, 0x68, 0x50, 0x38, 0x20 };
static const uint8_t TILE_Y[7] = { 0xbf, 0xa7, 0x8f, 0x77, 0x5f, 0x47, 0x2f };

int main(void) {
    /* tablas de DS */
    for (int r = 0; r < 7; r++) {
        CHECK(ds_pool[0x2bd4 + r] == ROW_Y[r], "window_row_y_table[%d]", r);
        CHECK(ds_pool[0x7120 + r] == TILE_Y[r], "cupid_sprite_end[%d]", r);
        CHECK(ds_pool[0x2bdb + r] == 18 * r, "window_row_col_offset[%d]", r);
    }
    for (int c = 0; c < 16; c++) CHECK((ds_pool[0x70fc + 2*c] | (ds_pool[0x70fd + 2*c] << 8)) == 0x20 + 0x10 * c, "cupid_sprite_offset[%d]", c);

    /* ---- draw_cupid: frame = 6f30 + (arrow & 0x1e0) (+0xc0 si dir != 0xff) */
    static const uint16_t arrows[] = { 0x0, 0x1c, 0x20, 0x40, 0x60, 0x80, 0xa0 };
    for (int d = 0; d < 2; d++) for (unsigned k = 0; k < sizeof arrows / sizeof arrows[0]; k++) {
        pattern(); memcpy(original, cga_mem, sizeof original);
        size_t addr = 0x0a50 + 0x20 * k; uint8_t sh;
        cupid_arrow_x = arrows[k]; cupid_dir = d ? 0x01 : 0xff; cupid_draw_addr = (uint16_t)addr; cupid_drawn = 1; cupid_erase_addr = 0;
        draw_cupid();
        unsigned src = 0x6f30 + (arrows[k] & 0x1e0) + (d ? 0xc0 : 0);
        static uint8_t ref[sizeof cga_mem]; memcpy(ref, cga_mem, sizeof ref);
        memcpy(cga_mem, original, sizeof original);
        blit_transparent(&ds_pool[src], addr, 2, 8, NULL);
        CHECK(memcmp(ref, cga_mem, sizeof ref) == 0, "draw: arrow %x dir %s -> frame en 0x%04x", arrows[k], d ? "der" : "izq", src);
        CHECK(cupid_drawn == 0 && cupid_erase_addr == addr, "draw: drawn=%u erase_addr=%04x", cupid_drawn, cupid_erase_addr);
        (void)sh;
        /* erase_cupid devuelve el fondo (aunque draw_addr ya cambio) */
        cupid_draw_addr = 0x1234;
        memcpy(cga_mem, ref, sizeof ref);
        erase_cupid();
        CHECK(memcmp(cga_mem, original, sizeof original) == 0, "erase: restaura el fondo (arrow %x dir %d)", arrows[k], d);
    }
    /* erase_cupid no hace nada si cupid_drawn != 0 */
    pattern(); memcpy(original, cga_mem, sizeof original);
    cupid_drawn = 0; cupid_dir = 1; cupid_arrow_x = 0; cupid_draw_addr = 0x0a50; draw_cupid();
    static uint8_t drawn_img[sizeof cga_mem]; memcpy(drawn_img, cga_mem, sizeof drawn_img);
    cupid_drawn = 1; erase_cupid();
    CHECK(memcmp(cga_mem, drawn_img, sizeof drawn_img) == 0, "erase con drawn=1: sin cambios");
    CHECK(memcmp(drawn_img, original, sizeof original) != 0, "el sprite se ve (la pantalla cambio)");

    /* ---- check_cupid_collision */
    cat_x = 100; cat_y = 0x60;
    cupid_active = 0; in_level_mode = 0; anim_counter = 0; anim_step = 0; transition_timer = 0; tone_calls = 0;
    cupid_x = 100; cupid_y = 0x60;
    CHECK(check_cupid_collision() == 0 && tone_calls == 0 && in_level_mode == 0, "inactivo: CF=0 sin efectos");
    cupid_active = 1;
    cupid_x = 100; cupid_y = 0x60;
    CHECK(check_cupid_collision() == 1, "solapado: CF=1");
    CHECK(in_level_mode == 1 && anim_counter == 2 && anim_step == 0x20 && transition_timer == 8, "efectos: in_level_mode=%d anim_counter=%d anim_step=%x transition_timer=%d", in_level_mode, anim_counter, anim_step, transition_timer);
    CHECK(tone_calls == 1 && tone_a == 0x91d && tone_b == 0xce4, "start_tone(0x91d, 0xce4) (%x,%x) x%d", tone_a, tone_b, tone_calls);
    in_level_mode = 0; anim_counter = 0; anim_step = 0; transition_timer = 0; tone_calls = 0;
    cupid_x = 250; cupid_y = 0x60;
    CHECK(check_cupid_collision() == 0 && tone_calls == 0 && in_level_mode == 0 && anim_step == 0, "lejos: CF=0 sin efectos");
    { int mism = 0, hits = 0;                                    /* barrido contra check_rect_collision con los parametros del ASM */
      for (int cx = 0; cx < 0x130; cx += 3) for (int cy = 0x40; cy < 0x80; cy += 2) {
          cupid_x = (uint16_t)cx; cupid_y = (uint8_t)cy; tone_calls = 0;
          int cf = check_cupid_collision();
          int ref = check_rect_collision((int16_t)cx, (uint8_t)cy, 0x10, 0x08, 100, 0x60, 0x18, 0x0e) ? 1 : 0;
          if (cf != ref) mism++;
          hits += cf;
      }
      CHECK(mism == 0 && hits > 0, "barrido de posiciones: %d diferencias, %d choques", mism, hits); }
    CHECK(check_cupid_collision() == check_cupid_collision(), "determinista");

    /* ---- cupid_toggle_window: 7 filas x 16 columnas */
    int bad = 0;
    for (int r = 0; r < 7; r++) for (int c = 0; c < 16; c++) for (int sub = 0; sub < 2; sub++) {
        pattern(); memcpy(original, cga_mem, sizeof original);
        memset(window_open_state, 0, sizeof window_open_state);
        int idx = r * 18 + c;
        window_open_state[idx] = (uint8_t)(sub ? 2 : 0);
        cupid_y = (uint8_t)(ROW_Y[r] + 8 + (c & 7));                /* (y-8) & 0xf8 == tabla: vale cualquiera de las 8 filas de pixeles */
        cupid_x = (uint16_t)((c + 2) * 16 + (c & 15));
        cupid_prev_x = 0xffff; tile_calls = 0;
        cupid_dir = 1; cupid_arrow_x = 0; cupid_drawn = 1;
        uint8_t shx; cupid_draw_addr = (uint16_t)calc_cga_addr(cupid_y, cupid_x, &shx);
        draw_cupid(); watch_addr = cupid_draw_addr;
        cupid_toggle_window();
        int exp_state = sub ? 0 : 2;
        if (window_open_state[idx] != exp_state || cupid_prev_x != idx || tile_calls != 1 || tile_x != 0x20 + 0x10 * c || tile_y != TILE_Y[r] || tile_bx != (uint16_t)exp_state || !region_clean_at_tile) {
            bad++; if (bad < 6) printf("  fila %d col %d: estado %d (esp %d) prev %d tile x=%x y=%x bx=%x clean=%d\n", r, c, window_open_state[idx], exp_state, cupid_prev_x, tile_x, tile_y, tile_bx, region_clean_at_tile);
        }
        /* despues del toggle el cupido vuelve a estar dibujado y erase_addr = draw_addr */
        if (cupid_drawn != 0 || cupid_erase_addr != cupid_draw_addr) bad++;
        /* mismo sitio otra vez: no cambia nada (prev_x == indice) */
        tile_calls = 0; cupid_toggle_window();
        if (window_open_state[idx] != exp_state || tile_calls != 0) bad++;
    }
    CHECK(bad == 0, "toggle: %d errores en 7x16x2 casos", bad);

    /* alternar dos veces la misma ventana (pasando por otra) deja el estado inicial */
    memset(window_open_state, 0, sizeof window_open_state); window_open_state[1 * 18 + 5] = 2;
    cupid_prev_x = 0xffff; cupid_drawn = 1;
    cupid_y = (uint8_t)(ROW_Y[1] + 8); cupid_x = (5 + 2) * 16; cupid_toggle_window();
    cupid_y = (uint8_t)(ROW_Y[1] + 8); cupid_x = (6 + 2) * 16; cupid_toggle_window();
    cupid_y = (uint8_t)(ROW_Y[1] + 8); cupid_x = (5 + 2) * 16; cupid_toggle_window();
    CHECK(window_open_state[1 * 18 + 5] == 2 && window_open_state[1 * 18 + 6] == 2, "dos toggles de la misma ventana -> estado inicial (%d), la vecina quedo en %d", window_open_state[1 * 18 + 5], window_open_state[1 * 18 + 6]);

    /* fuera de rango: sin cambios */
    memset(window_open_state, 0, sizeof window_open_state); cupid_prev_x = 0xffff; tile_calls = 0;
    cupid_y = (uint8_t)(ROW_Y[2] + 8); cupid_x = 0x1f; cupid_toggle_window();        /* (x>>4)-2 < 0 */
    cupid_x = 0x120; cupid_toggle_window();                                           /* columna 16 */
    cupid_x = 0x200; cupid_toggle_window();
    cupid_y = (uint8_t)(ROW_Y[2] + 8 + 8); cupid_x = 0x60; cupid_toggle_window();      /* y entre filas */
    cupid_y = 0x05; cupid_x = 0x60; cupid_toggle_window();
    { int any = 0; for (size_t i = 0; i < sizeof window_open_state; i++) any |= window_open_state[i]; CHECK(!any && tile_calls == 0 && cupid_prev_x == 0xffff, "fuera de rango / entre filas: sin cambios"); }

    if (!fails) printf("test_cupid_draw: OK\n");
    return fails;
}
