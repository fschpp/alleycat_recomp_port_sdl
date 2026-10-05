/* T20 — init/update/close_level3_doors (level_objects.asm L1376-1440). Sin SDL.
 * Los tres huecos de puerta (CGA 0x3f0/0x3f8/0x400, 2 words x 16 filas) se rellenan primero con
 * un patrón distinto de 0xAA y se comprueba que solo se cierra la puerta tocada. */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "cga.h"
#include "cat_state.h"
#include "alley.h"
#include "game_setup.h"
#include "level3_enemy.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)
static void tick_wait(void) { struct timespec ts = {0, 120000000L}; nanosleep(&ts, NULL); }

/* ¿el hueco de la puerta (offset CGA `a`) vale 0xAA en sus 16 filas x 4 bytes? */
static bool door_closed(uint16_t a) {
    for (int r = 0; r < 16; r++) {
        size_t base = (size_t)a + (size_t)((r & 1) ? 0x2000 : 0) + (size_t)(r >> 1) * 80;
        for (int k = 0; k < 4; k++) if (cga_mem[base + k] != 0xaa) return false;
    }
    return true;
}

int main(void) {
    cga_init();
    const uint16_t addr[3] = {0x3f0, 0x3f8, 0x400};
    memset(cga_mem, 0x55, sizeof cga_mem);
    cat_x = 0; setup_alley(); save_alley_buffer();
    init_level3_doors();
    object_hit = 0; cat_caught = 0;

    /* 1. sin solape (cat_y=0x50, fuera de [0x0a,0x28]): nada cambia */
    cat_x = 0xc0; cat_y = 0x50; tick_wait(); update_level3_doors();
    CHECK(!door_closed(addr[0]) && cat_caught == 0, "sin solape no debe cerrar");

    /* 2. bordes en X: puerta 0 = [0xa8,0xd0] (a la izquierda no hay otra puerta) y puerta 2 =
     * [0xe8,0x110] (a la derecha no hay otra). Las puertas se solapan entre sí en X (cada una mide
     * 0x28 de rango), por eso se aíslan por los extremos. */
    cat_y = 0x18; cat_x = 0xa7; tick_wait(); update_level3_doors();
    CHECK(!door_closed(addr[0]), "borde X izq. fuera (0xa7) no debe cerrar la puerta 0");
    cat_y = 0x18; cat_x = 0x111; tick_wait(); update_level3_doors();
    CHECK(!door_closed(addr[2]), "borde X der. fuera (0x111) no debe cerrar la puerta 2");
    /* bordes en Y (para puerta 0, cat_x=0xc0 no toca la puerta 1 que empieza en 0xc8): [0x0a, 0x28] */
    for (int i = 0; i < 2; i++) {
        cat_x = 0xc0; cat_y = (i == 0) ? 0x09 : 0x29;
        tick_wait(); update_level3_doors();
        CHECK(!door_closed(addr[0]), "borde Y fuera (0x%x) no debe cerrar", (unsigned)cat_y);
    }
    CHECK(cat_caught == 0 && !door_closed(addr[1]), "ningún cierre espurio antes del paso 3");

    /* 3. solape justo en los bordes dentro: cierra la puerta 0 y solo ella */
    cat_x = 0xa8; cat_y = 0x0a; tick_wait(); update_level3_doors();
    CHECK(door_closed(addr[0]), "puerta 0 debe cerrarse en el borde interior");
    CHECK(!door_closed(addr[1]) && !door_closed(addr[2]), "solo la puerta 0 se cierra");
    CHECK(cat_caught == 0, "quedan 2 puertas: cat_caught sigue en 0");

    /* 4. la puerta 0 ya cerrada no vuelve a dispararse (active=0) */
    memset(cga_mem + addr[0], 0x11, 4); tick_wait(); update_level3_doors();
    CHECK(cga_mem[addr[0]] == 0x11, "puerta cerrada no se reprocesa");

    /* 5. el bucle recorre bx=4,2,0: con solape en las puertas 1 y 2 a la vez gana la puerta 2 (bx=4)
     * (cat_x=0xe8 solapa con x=0xe0 [0xc8..0xf0] y x=0x100 [0xe8..0x110]) */
    cat_x = 0xe8; cat_y = 0x18; tick_wait(); update_level3_doors();
    CHECK(door_closed(addr[2]) && !door_closed(addr[1]), "con dos solapes gana bx=4 (puerta 2)");
    CHECK(cat_caught == 0, "queda 1 puerta");

    /* 6. última puerta: cierra y cat_caught=1 (object_hit==0) */
    tick_wait(); update_level3_doors();
    CHECK(door_closed(addr[1]), "puerta 1 se cierra");
    CHECK(cat_caught == 1, "al cerrar la última puerta cat_caught debe ser 1");

    /* 7. con object_hit != 0 la última puerta NO marca cat_caught */
    init_level3_doors(); cat_caught = 0; object_hit = 1;
    for (int i = 0; i < 3; i++) close_level3_door((uint16_t)(2 * i));
    CHECK(cat_caught == 0, "con object_hit=1 no se marca cat_caught");

    /* 8. mismo tick => retorno inmediato (cmp dx,[dat_37b8]) */
    init_level3_doors(); object_hit = 0;
    cat_x = 0xc0; cat_y = 0x18; memset(cga_mem + addr[0], 0x11, 4);
    tick_wait(); update_level3_doors();            /* cierra la 0 */
    memset(cga_mem + addr[0], 0x11, 4); cat_x = 0xc0;
    init_level3_doors();                           /* reactiva, pero el tick no cambió */
    update_level3_doors();
    CHECK(cga_mem[addr[0]] == 0x11, "mismo tick: no procesa");

    printf(fails ? "RESULT: %d FAIL\n" : "RESULT: all OK\n", fails);
    return fails != 0;
}
