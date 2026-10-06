/* T54 — show_attract_mode, detect_joystick, test_joystick_axis (ui.asm L300-382 y L431-469) e int9_set_scancode.
 * Sin SDL. Las teclas se simulan con un guion por vuelta de ui_wait_hook (matriz de teclas + keyboard_counter). */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "ui.h"
#include "input.h"
#include "hardware.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint16_t now;
static uint16_t tickfn(void) { return now; }

/* guion: en la vuelta `iter` aplica una accion */
typedef struct { int iter; int idx; int press; int count; } Ev;   /* idx = indice de matriz (-1 ninguno), press 1/0, count = ++keyboard_counter */
static const Ev *script; static int nscript, iter, tick_step;
static void hook(void) {
    iter++; now = (uint16_t)(now + tick_step);
    for (int i = 0; i < nscript; i++) if (script[i].iter == iter) {
        if (script[i].idx >= 0) key_matrix[script[i].idx] = script[i].press ? 0x00 : 0x80;
        if (script[i].count) keyboard_counter++;
    }
}
static void reset_run(const Ev *s, int n) {
    memset(cga_mem, 0, sizeof cga_mem); memset(key_matrix, 0x80, sizeof key_matrix);
    script = s; nscript = n; iter = 0; tick_step = 1; now = 0x4000; keyboard_counter = 0x0200;
    use_joystick = 5; diff_icon_idx = 9; title_joy_offset = 0x1234; bios_equipment = 0x0020; joy_port_fn = NULL;
}
static uint8_t snap[sizeof cga_mem];
static void line(uint16_t ofs, int n) { title_joy_offset = ofs; for (int i = 0; i < n; i++) display_text_line(); }

static int joy_calls, joy_ok_after;
static uint8_t joy_axis_fn(void) { return (++joy_calls > joy_ok_after) ? 0xec : 0xff; }   /* bits 0-1 a 0 = ejes responden; bit 4 a 0 = boton pulsado */
static uint8_t joy_button_fn(void) { return 0xef; }                                        /* bit 4 a 0: boton pulsado */

/* Referencia independiente: el texto que debe quedar en pantalla, repitiendo literalmente la secuencia del ASM. */
static void ref_start(void) { clear_cga(); line(0x0, 1); }

int main(void) {
    game_tick_fn = tickfn; ui_wait_hook = hook;

    /* ---- int9_set_scancode: la tabla de DS 0x6a1 da Y/N/K/H/T/A en los indices 10..15 */
    memset(key_matrix, 0x80, sizeof key_matrix);
    static const uint8_t sc[6] = { 0x15, 0x31, 0x25, 0x23, 0x14, 0x1e };
    for (int i = 0; i < 6; i++) {
        int_least32_t k = KEY_IDX_JOY_YES + i;
        int9_set_scancode(sc[i], true);  CHECK(key_matrix[k] == 0x00, "scancode %02x pulsado -> matriz[%d]=0", sc[i], (int)k);
        int9_set_scancode((uint8_t)(sc[i] | 0x80), false); CHECK(key_matrix[k] == 0x80, "scancode %02x soltado (bit 7) -> 0x80", sc[i]);
    }
    int9_set_scancode(0x02, true); { int all = 1; for (int i = 0; i < KEY_MATRIX_SIZE; i++) all = all && key_matrix[i] == 0x80; CHECK(all, "scancode fuera de la tabla no toca la matriz"); }
    CHECK(ds_pool[0x6a1 + KEY_IDX_JOY_YES] == 0x15 && ds_pool[0x6a1 + KEY_IDX_DIFF3] == 0x1e, "indices KEY_IDX_* coinciden con la tabla");

    /* ---- test_joystick_axis: responde a la 1a lectura / agota 0x12 ticks */
    { static const Ev e[1] = {{ -1, -1, 0, 0 }}; reset_run(e, 0); }
    joy_calls = 0; joy_ok_after = 0; joy_port_fn = joy_axis_fn; tick_step = 5; now = 100;
    CHECK(test_joystick_axis() == 0 && iter == 1, "eje responde en la 1a lectura -> CF=0 (vueltas %d)", iter);
    iter = 0; now = 100; joy_calls = 0; joy_ok_after = 1000;
    CHECK(test_joystick_axis() == 1 && iter == 4, "sin respuesta: CF=1 tras 4 lecturas de 5 ticks (>= 0x12) (vueltas %d)", iter);
    CHECK(title_input_tick == 100, "title_input_tick = tick de arranque (%u)", title_input_tick);
    iter = 0; now = 0xfff0; joy_calls = 0; joy_ok_after = 1000; tick_step = 6;                  /* el tick da la vuelta: 0xfff0 -> 0x0006 */
    CHECK(test_joystick_axis() == 1 && iter == 3, "timeout con vuelta del contador de ticks (vueltas %d, 3)", iter);

    /* ---- detect_joystick */
    static uint8_t zero[sizeof cga_mem];
    memset(zero, 0, sizeof zero);
    { static const Ev e[1] = {{ 3, -1, 0, 1 }}; reset_run(e, 1); }                    /* sin game port: aviso + espera tecla */
    CHECK(detect_joystick() == 1 && iter == 3, "sin bit 12 de equipo: CF=1 tras esperar una tecla (vueltas %d)", iter);
    memcpy(snap, cga_mem, sizeof snap); memset(cga_mem, 0, sizeof cga_mem); line(0x24, 4);
    CHECK(memcmp(snap, cga_mem, sizeof snap) == 0 && memcmp(snap, zero, sizeof snap) != 0, "aviso: 4 lineas desde title_joy_offset=0x24");
    CHECK(title_joy_offset == 0x2c, "title_joy_offset termina en 0x2c (%x)", title_joy_offset);

    { static const Ev e[1] = {{ -1, -1, 0, 0 }}; reset_run(e, 0); }                   /* game port y eje responde */
    bios_equipment = 0x1020; joy_calls = 0; joy_ok_after = 0; joy_port_fn = joy_axis_fn;
    CHECK(detect_joystick() == 0 && iter == 1 && memcmp(cga_mem, zero, sizeof cga_mem) == 0, "joystick presente: CF=0, sin texto (vueltas %d)", iter);

    bios_equipment = 0x1020; joy_calls = 0; joy_ok_after = 4; iter = 0; tick_step = 5; now = 100;   /* 1er intento agota, 2o responde */
    CHECK(detect_joystick() == 0 && iter == 5, "1er test agota (4 lecturas), el 2o responde -> CF=0 (vueltas %d)", iter);

    { static const Ev e[1] = {{ 20, -1, 0, 1 }}; reset_run(e, 1); }                   /* ambos intentos agotan */
    bios_equipment = 0x1020; joy_calls = 0; joy_ok_after = 1000; joy_port_fn = joy_axis_fn; tick_step = 5; now = 100;
    CHECK(detect_joystick() == 1 && iter == 20, "ambos intentos agotan (8 lecturas) y luego tecla en la vuelta 20 -> CF=1 (vueltas %d)", iter);

    /* ---- show_attract_mode: teclado. N, luego K/H/T/A */
    { static const Ev e[] = { {2,-1,0,1}, {4,KEY_IDX_JOY_NO,1,1}, {6,-1,0,1}, {9,KEY_IDX_DIFF1,1,1}, {12,-1,0,1} };
      reset_run(e, 5); show_attract_mode();
      CHECK(use_joystick == 0 && diff_icon_idx == 1, "teclado: use_joystick=%d, dificultad=%d (0, 1)", use_joystick, diff_icon_idx);
      CHECK(iter == 12, "teclado: 12 vueltas (%d); teclas ajenas no avanzan", iter);
      memcpy(snap, cga_mem, sizeof snap); memset(cga_mem, 0, sizeof cga_mem);
      ref_start(); line(0x2, 5); line(0xc, 5); line(0x1c, 2); line(0x16, 1);
      CHECK(memcmp(snap, cga_mem, sizeof snap) == 0, "teclado: texto final = secuencia del ASM (1+5+5 lineas, 0x1c x2, 0x16 x1)");
      CHECK(title_joy_offset == 0x18, "teclado: title_joy_offset final 0x18 (%x)", title_joy_offset); }

    /* dificultad: K=0 H=1 T=2 A=3 y prioridad K > H > T > A cuando hay varias pulsadas */
    static const struct { int keys[4]; int want; } dc[] = {
        {{1,0,0,0},0}, {{0,1,0,0},1}, {{0,0,1,0},2}, {{0,0,0,1},3}, {{0,0,1,1},2}, {{0,1,0,1},1}, {{1,0,0,1},0}, {{1,1,1,1},0} };
    for (unsigned c = 0; c < sizeof dc / sizeof dc[0]; c++) {
        Ev e[8]; int n = 0;
        e[n++] = (Ev){ 4, KEY_IDX_JOY_NO, 1, 1 };
        for (int k = 0; k < 4; k++) if (dc[c].keys[k]) e[n++] = (Ev){ 7, KEY_IDX_DIFF0 + k, 1, k == 0 || !dc[c].keys[0] ? 0 : 0 };
        e[n++] = (Ev){ 7, -1, 0, 1 };
        e[n++] = (Ev){ 10, -1, 0, 1 };
        reset_run(e, n); show_attract_mode();
        CHECK(diff_icon_idx == dc[c].want && iter == 10, "dificultad caso %u: %d (esperado %d), vueltas %d", c, diff_icon_idx, dc[c].want, iter);
    }

    /* tecla de dificultad ajena: no sale hasta que haya una (matriz sin K/H/T/A -> otra espera) */
    { static const Ev e[] = { {2,KEY_IDX_JOY_NO,1,1}, {4,-1,0,1}, {6,-1,0,1}, {8,KEY_IDX_DIFF3,1,1}, {10,-1,0,1} };
      reset_run(e, 5); show_attract_mode(); CHECK(diff_icon_idx == 3 && iter == 10, "A tras dos teclas ajenas -> 3 (vueltas %d)", iter); }

    /* ---- Y sin joystick: aviso, espera tecla y vuelve a empezar; luego N */
    { static const Ev e[] = { {3,KEY_IDX_JOY_YES,1,1}, {4,KEY_IDX_JOY_YES,0,0}, {5,-1,0,1}, {7,KEY_IDX_JOY_NO,1,1}, {9,KEY_IDX_DIFF0,1,1}, {11,-1,0,1} };
      reset_run(e, 6); show_attract_mode();
      CHECK(use_joystick == 0 && diff_icon_idx == 0 && iter == 11, "Y sin game port: reinicia y sigue (uj=%d, dif=%d, vueltas %d)", use_joystick, diff_icon_idx, iter);
      memcpy(snap, cga_mem, sizeof snap); memset(cga_mem, 0, sizeof cga_mem);
      ref_start(); line(0x2, 5); line(0xc, 5); line(0x1c, 2); line(0x16, 1);
      CHECK(memcmp(snap, cga_mem, sizeof snap) == 0, "tras el reinicio la pantalla se borra (el aviso no queda)"); }

    /* ---- Y con joystick presente: use_joystick=1, instrucciones del joystick, espera el boton */
    { static const Ev e[] = { {3,KEY_IDX_JOY_YES,1,1}, {8,KEY_IDX_DIFF3,1,1} };
      reset_run(e, 2); bios_equipment = 0x1020; joy_calls = 0; joy_ok_after = 0;
      joy_port_fn = joy_axis_fn;                                                        /* 0xec: ejes responden; boton (bit 4) = 0 */
      show_attract_mode();
      CHECK(use_joystick == 1 && diff_icon_idx == 3, "joystick: use_joystick=%d, dificultad=%d (1, 3)", use_joystick, diff_icon_idx);
      CHECK(iter == 9, "joystick: vueltas %d (9)", iter);
      memcpy(snap, cga_mem, sizeof snap); memset(cga_mem, 0, sizeof cga_mem);
      ref_start(); line(0x2, 5); line(0xc, 5); line(0x20, 2); line(0x18, 2);
      CHECK(memcmp(snap, cga_mem, sizeof snap) == 0, "joystick: texto final = secuencia del ASM (0x20 x2, 0x18 x2)");
      CHECK(title_joy_offset == 0x1c, "joystick: title_joy_offset final 0x1c (%x)", title_joy_offset); }

    /* game port presente pero los ejes no responden (2 intentos de 4 lecturas): aviso, tecla (vuelta 14), reinicio, N, K */
    { static const Ev e[] = { {2,KEY_IDX_JOY_YES,1,1}, {4,KEY_IDX_JOY_YES,0,0}, {14,-1,0,1}, {16,KEY_IDX_JOY_NO,1,1}, {18,KEY_IDX_DIFF0,1,1}, {20,-1,0,1} };
      reset_run(e, 6); bios_equipment = 0x1020; joy_port_fn = joy_button_fn; tick_step = 5;   /* 0xef: eje (bits 0-1) a 1 */
      show_attract_mode();
      CHECK(use_joystick == 0 && diff_icon_idx == 0 && iter == 20, "game port sin respuesta de ejes: aviso y N (uj=%d, vueltas %d)", use_joystick, iter); }

    if (!fails) printf("test_attract: OK\n");
    return fails;
}
