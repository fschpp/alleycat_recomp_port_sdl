/* T24 — update_level4_state (level_objects.asm L1720-1823). Sin SDL.
 * Los valores esperados salen de leer el ASM y las tablas del data segment real (no de ejecutar el
 * C): dat_3c5a, dat_3ce3 (sembrada por el test), l4_platform_offset, l4_obj_x_table y
 * l4_anim_offset_table (DS 0x3d06). No hay emulador x86 en este entorno: ver PROGRESS.md §6u. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level4.h"
#include "level45_state.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void reset(void) {
    memset(cga_mem, 0, CGA_MEM_SIZE);
    l3_door_anim_frame = 0; l3_platform_id = 0; auto_walk = 0; joy_button = 0; at_platform = 1;
    enemy_chasing = 1;                 /* salta erase/draw del objeto lanzado (se prueba aparte) */
    thrown_obj_x = 0; thrown_obj_y = 0; /* lejos del gato */
    cat_x = 100; cat_y = 100; cat_y_bottom = 0;
    l4_last_tick = 0; l4_dat_3d18 = 0;
    memset(l4_dat_3ce3, 0, sizeof l4_dat_3ce3);
}

static uint16_t anim_tbl(unsigned frame) {   /* DS 0x3d06 + frame */
    return (uint16_t)(ds_pool[0x3d06 + frame] | (ds_pool[0x3d06 + frame + 1] << 8));
}

int main(void) {
    /* 1) guardas con la animación parada: ninguna debe disparar */
    reset(); l3_platform_id = 1; l4_tick_override = 100; auto_walk = 1;
    update_level4_state(); CHECK(l3_door_anim_frame == 0, "auto_walk!=0 debe retornar");
    reset(); l3_platform_id = 1; l4_tick_override = 100; joy_button = 1;
    update_level4_state(); CHECK(l3_door_anim_frame == 0, "joy_button!=0 debe retornar");
    reset(); l3_platform_id = 0; l4_tick_override = 100;
    update_level4_state(); CHECK(l3_door_anim_frame == 0, "platform_id==0 debe retornar");
    reset(); l3_platform_id = 1; l4_tick_override = 100; thrown_obj_x = 100; thrown_obj_y = 100;
    update_level4_state(); CHECK(l3_door_anim_frame == 0, "objeto lanzado sobre el gato debe retornar");
    reset(); l3_platform_id = 1; l4_tick_override = 5; l4_dat_3d18 = 0;   /* 5 - 0 < 0xc */
    update_level4_state(); CHECK(l3_door_anim_frame == 0, "menos de 0xc ticks debe retornar");

    /* 2) caso A: platform_id=1 (bl=0, <3), dat_3ce3[0]=5 (>=3).
     * esperado: cga_1=0x0516, cga_2=0x80, cga_3=0x0b5e, base=0, y=0x50, x=0x70+8=0x78 */
    reset(); l3_platform_id = 1; l4_dat_3ce3[0] = 5; l4_tick_override = 100;
    update_level4_state();
    CHECK(l4_dat_3d18 == 100, "dat_3d18=%u", l4_dat_3d18);
    CHECK(at_platform == 0, "at_platform");
    CHECK(joy_button == 0x10, "joy_button=%u", joy_button);
    CHECK(l3_door_cga_1 == 0x0516, "cga_1=%x", l3_door_cga_1);
    CHECK(l3_door_cga_2 == 0x80, "cga_2=%x", l3_door_cga_2);
    CHECK(l3_door_cga_3 == 0x0b5e, "cga_3=%x", l3_door_cga_3);
    CHECK(l3_door_sprite_base == 0, "sprite_base=%x", l3_door_sprite_base);
    CHECK(l4_obj_cur_y == 0x50, "cur_y=%x", l4_obj_cur_y);
    CHECK(l4_obj_cur_x == 0x78, "cur_x=%x", l4_obj_cur_x);
    CHECK(l3_door_anim_frame == 0xc, "frame=%x (0xe-2)", l3_door_anim_frame);
    CHECK(l4_last_tick == 100, "last_tick");
    CHECK(cat_x == 100 && cat_y == 100, "el gato no se mueve en los frames >= 8");
    {   /* frame 0xc >= 8: dibuja en cga_1 el sprite DS (cga_2 + tabla[0xc]) */
        uint16_t src = (uint16_t)(0x80 + anim_tbl(0xc));
        CHECK(memcmp(&cga_mem[0x0516], &ds_pool[src], 4) == 0, "fila 0 del sprite en cga_1");
    }
    /* mismo tick: no avanza */
    update_level4_state(); CHECK(l3_door_anim_frame == 0xc, "mismo tick no debe avanzar");
    /* avanzar frames 0xa, 0x8 (origen) y 0x6..0x0 (destino) */
    const unsigned expect[] = {0xa, 0x8, 0x6, 0x4, 0x2, 0x0};
    for (unsigned i = 0; i < 6; i++) {
        l4_tick_override = 101 + (int32_t)i;
        update_level4_state();
        CHECK(l3_door_anim_frame == expect[i], "frame %u: esperado %x, real %x", i, expect[i], l3_door_anim_frame);
        if (expect[i] >= 8) {
            CHECK(cat_x == 100 && cat_y == 100, "frame %x: gato quieto", expect[i]);
        } else {
            CHECK(cat_x == 0x78 && cat_y == 0x50 && cat_y_bottom == 0x82,
                  "frame %x: gato en destino (x=%d y=%u b=%u)", expect[i], cat_x, cat_y, cat_y_bottom);
            uint16_t src = (uint16_t)(0 + anim_tbl(expect[i]));
            CHECK(memcmp(&cga_mem[0x0b5e], &ds_pool[src], 4) == 0, "frame %x: sprite en cga_3", expect[i]);
        }
    }
    CHECK(l4_last_tick == 106, "last_tick final=%u", l4_last_tick);

    /* 3) caso B: platform_id=4 (bl=3: cga_2=0), dat_3ce3[3]=2 (<3: base=0x80).
     * esperado: cga_1=0x0a06, cga_3=0x0660, y=0x28, x=0x78+8=0x80 */
    reset(); l3_platform_id = 4; l4_dat_3ce3[3] = 2; l4_tick_override = 200;
    update_level4_state();
    CHECK(l3_door_cga_1 == 0x0a06, "B cga_1=%x", l3_door_cga_1);
    CHECK(l3_door_cga_2 == 0, "B cga_2=%x", l3_door_cga_2);
    CHECK(l3_door_cga_3 == 0x0660, "B cga_3=%x", l3_door_cga_3);
    CHECK(l3_door_sprite_base == 0x80, "B base=%x", l3_door_sprite_base);
    CHECK(l4_obj_cur_y == 0x28, "B cur_y=%x", l4_obj_cur_y);
    CHECK(l4_obj_cur_x == 0x80, "B cur_x=%x", l4_obj_cur_x);

    if (fails) { printf("%d FAIL\n", fails); return 1; }
    printf("test_level4_state: OK\n");
    return 0;
}
