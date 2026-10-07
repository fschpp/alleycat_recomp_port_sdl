/* T74: regresiones de la sesion del soak: (1) door_hit_flag == cat_died (DS 0x551), (2) check_level_platform: entrada y
 * valla segun level_physics.asm L112-139, (3) la CGA descarta accesos fuera de sus 16 KB, (4) bios_clock_hook. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "cga.h"
#include "cat_state.h"
#include "level_collision.h"
#include "bios_clock.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)
static uint16_t hook(void) { return 1234; }

int main(void) {
    cga_init();
    /* 1. alias */
    cat_died = 0; door_hit_flag = 1; CHECK(cat_died == 1, "door_hit_flag no es cat_died");

    /* 2. entrada: in_level_mode=1 y gata sobre el rectangulo -> cat_died=1 y CF=0 (false) */
    level_number = 1; in_level_mode = 1; cat_died = 0; l3_platform_id = 7;
    entrance_x = 100; entrance_y = 80; cat_x = 98; cat_y = 76;
    bool r = check_level_platform();
    CHECK(!r, "entrada: debe devolver false (clc)");
    CHECK(cat_died == 1, "entrada: debe marcar cat_died");
    CHECK(l3_platform_id == 0, "l3_platform_id debe quedar a 0");
    /* lejos de la entrada no marca nada */
    cat_died = 0; cat_x = 250; cat_y = 20;
    (void)check_level_platform();
    CHECK(cat_died == 0, "lejos de la entrada no debe marcar cat_died");

    /* 2b. valla del nivel 3 -> true y at_platform = 1 */
    level_number = 3; in_level_mode = 0; at_platform = 0; cat_x = 0xb0; cat_y = 0x40;
    r = check_level_platform();
    CHECK(r && at_platform == 1, "valla: true y at_platform=1 (r=%d at=%u)", r, (unsigned)at_platform);

    /* 3. CGA fuera de ventana: no revienta y no escribe */
    uint8_t spr[8 * 2 * 2]; memset(spr, 0xff, sizeof spr);
    uint8_t before = cga_mem[0];
    blit_transparent(spr, 0xFFFE, 2, 8, NULL);
    blit_to_cga(spr, 0xFFFF, 2, 8);
    save_from_cga(spr, 0xFFFE, 2, 8);
    CHECK(cga_mem[0] == before, "escritura fuera de ventana toco cga_mem");

    /* 4. reloj compartido */
    bios_clock_hook = hook; CHECK(bios_clock_read() == 1234, "hook ignorado");
    bios_clock_hook = NULL;

    if (fails) return 1;
    printf("test_t74_fixes: OK\n");
    return 0;
}
