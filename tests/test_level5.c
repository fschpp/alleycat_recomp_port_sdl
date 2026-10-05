/* T25/T26 — nivel 5, helpers A y B (level_objects.asm L2362-2437 y L2603-2665). Sin SDL.
 * Los valores esperados salen de calcular a mano cada rama del ASM (no de ejecutar el C ni un
 * emulador x86: no hay en este entorno). Ver PROGRESS.md §6v. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level5.h"
#include "gen/ds_pool.h"

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

    /* ---- T26: helpers B ---- */
    /* 6) check_l5_perch_hit: perch (100,100) 0x18x0x10 vs. gato 0x18x0x0e */
    l5_dat_40a8 = 100; l5_dat_40aa = 100;
    struct { int16_t cx; uint8_t cy; uint8_t exp; const char *n; } ph[] = {
        {100, 100, 1, "solapado"},
        {124, 100, 1, "borde der.: perch.x+0x18 == cat_x"},
        {125, 100, 0, "fuera por 1 a la derecha"},
        { 76, 100, 1, "borde izq.: perch.x-0x18 == cat_x"},
        { 75, 100, 0, "fuera por 1 a la izquierda"},
        {100, 116, 1, "borde inf.: perch.y+0x10 == cat_y"},
        {100, 117, 0, "fuera por 1 abajo"},
        {100,  86, 1, "borde sup.: perch.y-0x0e == cat_y"},
        {100,  85, 0, "fuera por 1 arriba"},
    };
    for (unsigned i = 0; i < sizeof ph / sizeof ph[0]; i++) {
        cat_x = ph[i].cx; cat_y = ph[i].cy;
        CHECK(check_l5_perch_hit() == (bool)ph[i].exp, "perch_hit %s", ph[i].n);
    }

    /* 7) draw/erase del perch: round-trip sobre un fondo no uniforme */
    for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + 3);
    static uint8_t bg[CGA_MEM_SIZE]; memcpy(bg, cga_mem, CGA_MEM_SIZE);
    l5_dat_40ab = 0x1514;
    draw_l5_perch();
    CHECK(l5_dat_40a6 == 0x1514, "draw: dat_40a6=%x", l5_dat_40a6);
    CHECK(memcmp(bg, cga_mem, CGA_MEM_SIZE) != 0, "draw no cambia la CGA");
    /* fila 0 (bytes 0..5 en 0x1514): AND de los 3 words del sprite (ff 00 0f 00 0f ff) con el fondo */
    for (int k = 0; k < 6; k++) {
        uint8_t want = (uint8_t)(bg[0x1514 + k] & ds_pool[0x3fbe + k]);
        CHECK(cga_mem[0x1514 + k] == want, "draw fila 0 byte %d: %02x != %02x", k, cga_mem[0x1514 + k], want);
    }
    l5_dat_40ab = 0x0000;                        /* erase debe usar dat_40a6, no dat_40ab */
    erase_l5_perch();
    CHECK(memcmp(bg, cga_mem, CGA_MEM_SIZE) == 0, "erase no restaura el fondo exacto");

    /* 8) check_l5_thrown_near: perch_x = 100 -> rango 80..128, y >= 0x66 */
    l5_dat_40a8 = 100;
    struct { int16_t tx; uint8_t ty; uint8_t exp; const char *n; } tn[] = {
        {100, 0x66, 1, "centro, y justo en el umbral"},
        {100, 0x65, 0, "y por debajo del umbral"},
        { 80, 0x70, 1, "borde izq.: perch.x-0x14"},
        { 79, 0x70, 0, "fuera por 1 a la izquierda"},
        {128, 0x70, 1, "borde der.: perch.x+0x1c (= -0x14 + 0x30)"},
        {129, 0x70, 0, "fuera por 1 a la derecha"},
    };
    for (unsigned i = 0; i < sizeof tn / sizeof tn[0]; i++) {
        thrown_obj_x = tn[i].tx; thrown_obj_y = tn[i].ty;
        CHECK(check_l5_thrown_near() == (bool)tn[i].exp, "thrown_near %s", tn[i].n);
    }
    l5_dat_40a8 = 0x10; thrown_obj_x = 0x20; thrown_obj_y = 0x70;     /* perch_x-0x14 envuelve a 0xfffc */
    CHECK(!check_l5_thrown_near(), "thrown_near con envoltura sin signo");

    /* 9) check_thrown_near_cat: gato (100,80) -> caja x=92, y=83, 0x28 x 0x0e; lanzado 0x10 x 0x1e */
    cat_x = 100; cat_y = 80;
    struct { int16_t tx; uint8_t ty; uint8_t exp; const char *n; } nc[] = {
        {100, 80, 1, "solapado"},
        { 76, 80, 1, "borde izq.: tx+0x10 == bx"},
        { 75, 80, 0, "fuera por 1 a la izquierda"},
        {132, 80, 1, "borde der.: tx-0x28 == bx"},
        {133, 80, 0, "fuera por 1 a la derecha"},
        {100, 53, 1, "borde sup.: ty+0x1e == dh"},
        {100, 52, 0, "fuera por 1 arriba"},
        {100, 97, 1, "borde inf.: ty-0x0e == dh"},
        {100, 98, 0, "fuera por 1 abajo"},
    };
    for (unsigned i = 0; i < sizeof nc / sizeof nc[0]; i++) {
        thrown_obj_x = nc[i].tx; thrown_obj_y = nc[i].ty;
        CHECK(check_thrown_near_cat() == (bool)nc[i].exp, "near_cat %s", nc[i].n);
    }
    cat_x = 3; cat_y = 80;                         /* cat_x-8 hace préstamo -> bx = 0 */
    thrown_obj_x = 40; thrown_obj_y = 80; CHECK(check_thrown_near_cat(), "near_cat clamp: tx=40");
    thrown_obj_x = 41; thrown_obj_y = 80; CHECK(!check_thrown_near_cat(), "near_cat clamp: tx=41");
    cat_x = 8; thrown_obj_x = 40; thrown_obj_y = 80; CHECK(check_thrown_near_cat(), "near_cat cat_x=8 (bx=0 sin préstamo): tx=40");
    thrown_obj_x = 41; CHECK(!check_thrown_near_cat(), "near_cat cat_x=8: tx=41");
    cat_x = 100; cat_y = 0xfe; thrown_obj_x = 100;            /* dh = 0xfe+3 envuelve a 1 (8 bits) */
    thrown_obj_y = 0x0f; CHECK(check_thrown_near_cat(), "near_cat dh=1: ty-0x0e == dh");
    thrown_obj_y = 0x10; CHECK(!check_thrown_near_cat(), "near_cat dh=1: ty-0x0e == dh+1");

    if (fails) { printf("%d FAIL\n", fails); return 1; }
    printf("test_level5: OK\n");
    return 0;
}
