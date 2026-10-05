/* T25 — nivel 5, helpers A (level_objects.asm L2362-2437). Sin SDL.
 * Los valores esperados salen de calcular a mano cada rama del ASM (no de ejecutar el C ni un
 * emulador x86: no hay en este entorno). Ver PROGRESS.md §6v. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level5.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void) {
    /* 1) check_l5_landing */
    in_level_mode = 0; cat_y = 0x88; CHECK(check_l5_landing(), "landing 0x88");
    cat_y = 0x8f;                    CHECK(check_l5_landing(), "landing 0x8f (&0xf8 = 0x88)");
    cat_y = 0x90;                    CHECK(!check_l5_landing(), "landing 0x90");
    cat_y = 0x87;                    CHECK(!check_l5_landing(), "landing 0x87");
    in_level_mode = 1; cat_y = 0x88; CHECK(!check_l5_landing(), "landing con in_level_mode=1");
    in_level_mode = 0;

    /* 2) calc_l5_direction: 4 posiciones relativas + igualdad */
    l5_dat_40b2 = 100; l5_dat_40b4 = 50; cat_x = 60; cat_y = 40;       /* objeto a la derecha y abajo */
    calc_l5_direction();
    CHECK(l5_dat_40ca == 0x01 && l5_dat_40cb == 0x01, "dir ++ : %02x %02x", l5_dat_40ca, l5_dat_40cb);
    CHECK(l5_dat_40cc == 40 + 2 * 10, "dist ++ = %u", l5_dat_40cc);
    l5_dat_40b2 = 60; l5_dat_40b4 = 40; cat_x = 100; cat_y = 50;       /* izquierda y arriba (con not) */
    calc_l5_direction();
    CHECK(l5_dat_40ca == 0xff && l5_dat_40cb == 0xff, "dir -- : %02x %02x", l5_dat_40ca, l5_dat_40cb);
    CHECK(l5_dat_40cc == 39 + 2 * 9, "dist -- = %u (not: 0xffd8 -> 0x27, 0xf6 -> 0x09)", l5_dat_40cc);
    l5_dat_40b2 = 100; l5_dat_40b4 = 40; cat_x = 60; cat_y = 50;       /* derecha y arriba */
    calc_l5_direction();
    CHECK(l5_dat_40ca == 0x01 && l5_dat_40cb == 0xff, "dir +- : %02x %02x", l5_dat_40ca, l5_dat_40cb);
    CHECK(l5_dat_40cc == 40 + 2 * 9, "dist +- = %u", l5_dat_40cc);
    l5_dat_40b2 = 60; l5_dat_40b4 = 50; cat_x = 100; cat_y = 40;       /* izquierda y abajo */
    calc_l5_direction();
    CHECK(l5_dat_40ca == 0xff && l5_dat_40cb == 0x01, "dir -+ : %02x %02x", l5_dat_40ca, l5_dat_40cb);
    CHECK(l5_dat_40cc == 39 + 2 * 10, "dist -+ = %u", l5_dat_40cc);
    l5_dat_40b2 = 77; l5_dat_40b4 = 33; cat_x = 77; cat_y = 33;        /* iguales: sin préstamo */
    calc_l5_direction();
    CHECK(l5_dat_40ca == 0x01 && l5_dat_40cb == 0x01 && l5_dat_40cc == 0, "igual: %02x %02x %u",
          l5_dat_40ca, l5_dat_40cb, l5_dat_40cc);

    /* 3) check_l5_cat_catch: objeto 8x5 en (100,100) vs. gato 0x18x0x0e */
    l5_dat_40b2 = 100; l5_dat_40b4 = 100;
    struct { int16_t cx; uint8_t cy; uint8_t hit; uint8_t exp; const char *n; } cc[] = {
        {100, 100, 0, 1, "solapado"},
        {100, 100, 1, 0, "solapado pero object_hit != 0"},
        {108, 100, 0, 1, "borde X: A.right == B.x"},
        {109, 100, 0, 0, "fuera por 1 en X"},
        {100, 105, 0, 1, "borde Y: A.bottom == B.y"},
        {100, 106, 0, 0, "fuera por 1 en Y"},
        { 76, 100, 0, 1, "borde izq.: A.x - 0x18 == B.x"},
        { 75, 100, 0, 0, "fuera por la izquierda"},
        {200, 100, 0, 0, "lejos"},
    };
    for (unsigned i = 0; i < sizeof cc / sizeof cc[0]; i++) {
        cat_x = cc[i].cx; cat_y = cc[i].cy; object_hit = cc[i].hit; cat_caught = 0;
        check_l5_cat_catch();
        CHECK(cat_caught == cc[i].exp, "cat_catch %s: cat_caught=%u", cc[i].n, cat_caught);
    }
    object_hit = 0; cat_caught = 0;

    /* 4) check_l5_thrown: objeto 8x5 en (100,100) vs. lanzado 0x10x0x1e */
    struct { int16_t tx; uint8_t ty; uint8_t exp; const char *n; } tt[] = {
        {100, 100, 1, "solapado"},
        {108, 105, 1, "borde X/Y derecho-inferior"},
        {109, 100, 0, "fuera por 1 en X"},
        {100, 106, 0, "fuera por 1 en Y"},
        { 84, 100, 1, "borde izq.: A.x - 0x10 == B.x"},
        { 83, 100, 0, "fuera por la izquierda"},
        {100,  70, 1, "borde sup.: A.y - 0x1e == B.y"},
        {100,  69, 0, "fuera por arriba"},
    };
    for (unsigned i = 0; i < sizeof tt / sizeof tt[0]; i++) {
        thrown_obj_x = tt[i].tx; thrown_obj_y = tt[i].ty; l5_dat_40b8 = 0;
        check_l5_thrown();
        CHECK(l5_dat_40b8 == (tt[i].exp ? 0xff : 0x00), "thrown %s: dat_40b8=%02x", tt[i].n, l5_dat_40b8);
    }

    /* 5) init_level5_objects: calc_cga_addr(0x86, 0x90) = 0x86*0x28 + (0x90>>2) = 0x14f0 + 0x24 */
    l5_dat_40af = 7; l5_dat_40b1 = 7; l5_dat_40b9 = 7; l5_dat_40b8 = 7; l5_dat_40c8 = 7;
    init_level5_objects();
    CHECK(l5_dat_40a8 == 0x90 && l5_dat_40aa == 0x86, "init perch=(%x,%x)", l5_dat_40a8, l5_dat_40aa);
    CHECK(l5_dat_40ab == 0x1514, "init cga addr=%x", l5_dat_40ab);
    CHECK(l5_dat_40af == 0 && l5_dat_40b1 == 0 && l5_dat_40b9 == 1 && l5_dat_40b8 == 0 && l5_dat_40c8 == 0xff,
          "init flags %u %u %u %u %x", l5_dat_40af, l5_dat_40b1, l5_dat_40b9, l5_dat_40b8, l5_dat_40c8);

    if (fails) { printf("%d FAIL\n", fails); return 1; }
    printf("test_level5: OK\n");
    return 0;
}
