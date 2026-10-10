/* T85 (paridad 1 a 1, PLAN_PARIDAD §9): regresiones halladas por el diferencial por iteracion contra el cat.exe original.
 *  1. apply_cat_gravity (level_physics.asm lab_18b5..lab_18e1): el proyectil no aterriza al llegar a gravity_target_height;
 *     se hunde y se le recorta la altura (bh -= al - target); aterriza cuando al - target >= bh.
 *     Caso real del original (STATE_f12 k=11113): y=0x60, frame 0xc->0xd, target 0x61, dims 0x0902 -> y=0x66, save_dims=0x0402.
 *  2. setup_alley: `mov word [0x561],0xb03` (buffer_size), antes omitido.
 * Uso: make test-t85 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "cga.h"
#include "cat_state.h"
#include "jump_gravity.h"
#include "game_setup.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint16_t tk = 100;
uint16_t __wrap_read_bios_tick(void) { return tk; }
void __wrap_speaker_spin_cycles(uint32_t c) { (void)c; }

static void arm(uint8_t y, uint8_t frame, uint8_t target, uint16_t dims) {
    cat_x = 0; gravity_x = 0x12e; gravity_y = y; gravity_frame = frame; gravity_target_height = target;
    gravity_cur_dims = dims; gravity_drift_dir = 0; idle_aggro_flag = 0; gravity_h_speed = 0x40; deduct_life = 1;
    gravity_last_tick = (uint16_t)(tk - 1);
}

int main(void) {
    cga_init();
    /* 1a: caso real del original: sigue cayendo con la altura recortada a 4 (9 - 5) */
    arm(0x60, 0xc, 0x61, 0x0902);
    apply_cat_gravity();
    CHECK(gravity_y == 0x66, "gravity_y=%02x (esperado 66)", gravity_y);
    CHECK(gravity_save_dims == 0x0402, "gravity_save_dims=%04x (esperado 0402)", gravity_save_dims);
    CHECK(deduct_life == 1, "deduct_life no debe tocarse mientras el proyectil sigue en el aire");
    /* 1b: justo al agotar la altura (al - target == bh) aterriza */
    tk++; arm(0x69, 0x0, 0x61, 0x0902);          /* al = 0x69 + (1>>1) = 0x69; 0x69-0x61 = 8 < 9 -> aun cae (h=1) */
    apply_cat_gravity();
    CHECK(gravity_y == 0x69 && gravity_save_dims == 0x0102, "h=1: y=%02x dims=%04x", gravity_y, gravity_save_dims);
    tk++; arm(0x6a, 0x0, 0x61, 0x0902);          /* al = 0x6a; 0x6a-0x61 = 9 == bh -> aterriza */
    apply_cat_gravity();
    CHECK(gravity_y == 0 && deduct_life == 0, "debia aterrizar: y=%02x deduct=%u", gravity_y, deduct_life);
    /* 1c: por encima del objetivo (al < target) no recorta */
    tk++; arm(0x40, 0x0, 0x61, 0x0902);
    apply_cat_gravity();
    CHECK(gravity_y == 0x40 && gravity_save_dims == 0x0902, "sin recorte: y=%02x dims=%04x", gravity_y, gravity_save_dims);
    /* 2 */
    buffer_size = 0; setup_alley();
    CHECK(buffer_size == 0xb03, "setup_alley: buffer_size=%04x (esperado 0b03)", buffer_size);
    if (fails) { printf("test-t85: %d fallos\n", fails); return 1; }
    printf("test-t85: OK\n"); return 0;
}
