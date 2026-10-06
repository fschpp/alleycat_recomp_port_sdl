/* T52/T53 — show_title_screen, move_title_cat, animate_title_icon (ui.asm L55-236). Sin SDL.
 * Las funciones grandes que llama (init_player, draw_alley_scene, ...) se envuelven con --wrap y se registran. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "ui.h"
#include "input.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static char order[64]; static int norder;
static int music_calls, update_calls, restarts, last_scroll;
static uint16_t last_restart_tick;
static void rec(char c) { if (norder < 63) order[norder++] = c; order[norder] = 0; }
void __wrap_init_player(void)               { rec('P'); }
void __wrap_init_cycle_objects(void)        { rec('O'); }
void __wrap_draw_alley_scene(void)          { rec('A'); }
void __wrap_setup_alley(void)               { rec('S'); }
void __wrap_draw_high_score_display(void)   { rec('H'); }
void __wrap_draw_current_score(void)        { rec('C'); }
void __wrap_draw_lives(void)                { rec('L'); }
void __wrap_init_sound(void)                { rec('I'); }
void __wrap_play_music_note(void)           { music_calls++; }
void __wrap_update_alley_movement(void)     { update_calls++; last_scroll = scroll_speed; }
void __wrap_title_music_restart(uint16_t t) { restarts++; last_restart_tick = t; }

static uint16_t now;
static uint16_t tickfn(void) { return now; }
static int iter, step, press_at, joy_mode;
static void hook(void) { iter++; now = (uint16_t)(now + step); if (iter == press_at) keyboard_counter++; }
static uint8_t joyfn(void) {   /* joy_mode 1: pulsado siempre; 2: suelto 3 vueltas y luego pulsado */
    if (joy_mode == 1) return 0xef;
    return iter <= 3 ? 0xff : 0xef;
}

static void reset_run(void) {
    memset(cga_mem, 0, sizeof cga_mem); norder = 0; order[0] = 0;
    music_calls = update_calls = restarts = iter = 0; keyboard_counter = 0x0100;
    input_horizontal = input_vertical = 5; cat_x = 0x77; level_number = 3; lives_count = 0; lives_display = 0;
    use_joystick = 0; joy_port_fn = NULL; joy_mode = 0;
}
static int blit_ok(uint16_t src, size_t dst, int w_words, int h) {   /* primera fila y ultima fila coinciden con los datos */
    for (int r = 0; r < h; r++) {
        size_t off = dst;
        for (int k = 0; k < r; k++) off = (off & 0x2000) ? (off ^ 0x2000) + 80 : off ^ 0x2000;   /* siguiente fila CGA */
        if (memcmp(&cga_mem[off], &ds_pool[src + r * w_words * 2], (size_t)w_words * 2) != 0) return 0;
    }
    return 1;
}

int main(void) {
    game_tick_fn = tickfn; ui_wait_hook = hook;
    const uint16_t ICON0 = (uint16_t)(ds_pool[0x6a8f] | (ds_pool[0x6a90] << 8));
    const uint16_t ICON1 = (uint16_t)(ds_pool[0x6a91] | (ds_pool[0x6a92] << 8));

    /* ---- animate_title_icon: alterna los dos frames; el primer idx+=2 da el frame 1 */
    memset(cga_mem, 0, sizeof cga_mem);
    animate_title_icon(); CHECK(blit_ok(ICON1, 0x1d38, 10, 12), "icono: 1a llamada (idx 0 -> 2) = frame 1");
    animate_title_icon(); CHECK(blit_ok(ICON0, 0x1d38, 10, 12), "icono: 2a llamada = frame 0");
    animate_title_icon(); CHECK(blit_ok(ICON1, 0x1d38, 10, 12), "icono: 3a llamada = frame 1");

    /* ---- move_title_cat */
    use_joystick = 0; now = 1000;
    cat_x = 0x20; input_horizontal = 0; update_calls = 0; scroll_speed = 0;
    move_title_cat(); CHECK(input_horizontal == 1 && update_calls == 1 && last_scroll == 4, "x=0x20 (<=0x20) -> derecha, anima con scroll_speed 4");
    cat_x = 0x0; input_horizontal = 0; move_title_cat(); CHECK(input_horizontal == 1, "x=0 -> derecha");
    cat_x = 0x120; input_horizontal = 0; move_title_cat(); CHECK(input_horizontal == -1, "x=0x120 -> izquierda");
    cat_x = 0x150; input_horizontal = 0; move_title_cat(); CHECK(input_horizontal == -1, "x>0x120 -> izquierda");
    cat_x = 0x11f; input_horizontal = 7; now = 1000;                 /* en medio, pero attract_start_tick es de antes */
    /* fija attract_start_tick con una llamada a traves de la rama aleatoria: ver abajo */
    for (int pass = 0; pass < 2; pass++) {
        /* dentro de la zona: sin cambio mientras dt < 0x12; con dt >= 0x12 decide el LFSR (dl > 0xa0: parado; bit0 -> +1/-1) */
        extern uint16_t rng_seed;
        int seen_still = 0, seen_r = 0, seen_l = 0, seen_hold = 0;
        for (uint16_t seed = 1; seed < 400; seed++) {
            uint16_t save = rng_seed; rng_seed = seed; uint16_t dx = cga_random(); rng_seed = seed;
            cat_x = 0x80; input_horizontal = 7;
            now = (uint16_t)(now + 0x40);                        /* >= 0x12 desde el ultimo start_tick */
            move_title_cat();
            int8_t want = (uint8_t)dx > 0xa0 ? 0 : ((dx & 1) ? 1 : -1);
            CHECK(input_horizontal == want, "semilla %u dl=%02x -> %d (esperado %d)", seed, dx & 0xff, input_horizontal, want);
            if (want == 0) seen_still++; else if (want == 1) seen_r++; else seen_l++;
            /* justo despues: dt < 0x12 -> no toca input_horizontal */
            rng_seed = seed; input_horizontal = 7; now = (uint16_t)(now + 0x11); cat_x = 0x80;
            move_title_cat(); if (input_horizontal == 7) seen_hold++;
            rng_seed = save;
        }
        CHECK(seen_still && seen_r && seen_l, "se ven las 3 salidas (%d/%d/%d)", seen_still, seen_r, seen_l);
        CHECK(seen_hold == 399, "dt < 0x12 no cambia (%d)", seen_hold);
        (void)pass; break;
    }

    /* ---- show_title_screen: tecla en la vuelta 7 */
    reset_run(); now = 0xfff0; step = 10; press_at = 7; attract_shown = 0;
    show_title_screen();
    /* orden: init_player, init_objects, draw_alley_scene, (blits), setup_alley, draw_score(H), draw_high_score(C), draw_lives, init_sound */
    CHECK(!strcmp(order, "POASHCLI"), "orden de llamadas '%s'", order);
    CHECK(iter == 7 && music_calls == 7 && update_calls == 7, "vueltas %d, musica %d, gato %d (7)", iter, music_calls, update_calls);
    CHECK(level_number == 0 && cat_x == 0 && cat_y == 0x60 && cat_y_bottom == 0x92, "estado del gato");
    CHECK(lives_count == 9 && lives_display == 0xff, "vidas");
    CHECK(input_vertical == 0, "input_vertical 0");
    CHECK(restarts == 1 && last_restart_tick == 0xfff0, "musica reiniciada con el tick inicial (%04x)", last_restart_tick);
    /* ---- el logo se dibuja: filas de los 6 bitmaps (el fondo esta envuelto, no los pisa) */
    CHECK(blit_ok(0x6152, 0x00bd, 11, 29), "bitmap 6152 en 0x00bd");
    CHECK(blit_ok(0x63d0, 0x069e, 14, 22), "bitmap 63d0 en 0x069e");
    CHECK(blit_ok(0x6638, 0x0a78, 3, 12), "bitmap 6638 en 0x0a78");
    CHECK(blit_ok(0x6680, 0x0ca8, 14, 8), "bitmap 6680 en 0x0ca8");
    CHECK(blit_ok(0x6760, 0x1d6e, 12, 11), "bitmap 6760 en 0x1d6e");
    CHECK(blit_ok(0x6868, 0x1dec, 4, 8), "bitmap 6868 en 0x1dec");
    /* el icono se anima al llegar a 40 >= 0x24 ticks (vuelta 4): idx 2 -> 4 -> frame 0 */
    CHECK(blit_ok(ICON0, 0x1d38, 10, 12), "icono: 1 animacion en 7 vueltas -> frame 0");

    /* ---- timeout hacia el modo demo (attract_shown = 0): sale cuando dt > 846+6 = 852; 10 ticks por vuelta */
    reset_run(); now = 0x1000; step = 10; press_at = -1; attract_shown = 0;
    show_title_screen();
    CHECK(iter == 86 && music_calls == 85, "timeout: vueltas %d (86), musica %d (85)", iter, music_calls);
    /* animacion del icono cada >= 0x24 ticks: con pasos de 10 -> cada 40 ticks; 21 veces en 85 vueltas -> idx 2+42=44 -> frame 0 */
    CHECK(blit_ok(ICON0, 0x1d38, 10, 12), "icono tras 21 animaciones = frame 0");

    /* ---- attract_shown != 0: no hay timeout; al llegar a 846+0x48 = 918 ticks reinicia el ciclo (lab_5d54) */
    reset_run(); now = 0xff00; step = 10; press_at = 200; attract_shown = 1;
    show_title_screen();
    CHECK(iter == 200, "sin timeout: sale por tecla en la vuelta 200 (%d)", iter);
    CHECK(restarts == 1 + 2, "reinicios del ciclo: inicial + 2 (en las vueltas 92 y 184) = 3 (%d)", restarts);
    CHECK(music_calls == 200 - 2, "musica %d (vueltas sin reinicio: 198)", music_calls);

    /* ---- joystick: pulsado desde el principio no cuenta; hay que soltar y volver a pulsar */
    reset_run(); now = 0x2000; step = 10; press_at = -1; attract_shown = 0; use_joystick = 1; joy_port_fn = joyfn; joy_mode = 2;
    show_title_screen();
    /* vueltas 1..3 suelto (attract_key_pressed=1); 4a pulsado -> sale */
    CHECK(iter == 4, "joystick: sale a la 4a vuelta (%d)", iter);
    reset_run(); now = 0x2000; step = 10; press_at = 30; attract_shown = 0; use_joystick = 1; joy_port_fn = joyfn; joy_mode = 1;
    show_title_screen();
    CHECK(iter == 30, "joystick pulsado desde el inicio no sale; la tecla si (vuelta %d)", iter);

    ui_wait_hook = NULL; game_tick_fn = NULL; joy_port_fn = NULL;
    if (fails) { printf("test_title: %d FALLOS\n", fails); return 1; }
    printf("test_title: OK\n");
    return 0;
}
