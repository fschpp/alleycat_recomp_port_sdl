/* T30 — nivel 6, helpers B (level_objects.asm L2741-2816). Sin SDL.
 * Los valores esperados NO salen del C: rectangulos de colision calculados a mano desde check_rect_collision
 * (level_objects.asm L14-39: A.right>=B.x, max(A.x-B.w,0)<=B.x, A.bottom>=B.y, max(A.y-B.h,0)<=B.y), palabras de
 * dat_4100 leidas del DS (0xffff,0xffc0,0xc0c0,0xffff,0xff03,0x0303...) y un modelo de pantalla. Ver PROGRESS.md §6aa. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "alley.h"
#include "level6.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint8_t model[CGA_MEM_SIZE];
/* bloque w words x h filas en `at`: fila r en at + (r&1)*0x2000 + (r>>1)*80 */
static void m_blit(const uint8_t *src, uint16_t at, int w, int h) {
    for (int r = 0; r < h; r++)
        memcpy(&model[at + (r & 1) * 0x2000 + (r >> 1) * 80], src + r * w * 2, (size_t)w * 2);
}
/* igual pero sprite AND fondo (draw_alley_foreground / draw_l6_tracker) */
static void m_and(const uint8_t *src, uint16_t at, int w, int h) {
    for (int r = 0; r < h; r++)
        for (int b = 0; b < w * 2; b++)
            model[at + (r & 1) * 0x2000 + (r >> 1) * 80 + b] &= src[r * w * 2 + b];
}
static void fill(void) { for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + 3); memcpy(model, cga_mem, CGA_MEM_SIZE); }
static bool same(void) { return memcmp(cga_mem, model, CGA_MEM_SIZE) == 0; }

static void set_alley_buf(uint16_t pos, uint16_t dims) {
    for (int i = 0; i < 128; i++) alley_save_buf[i] = (uint16_t)(0x1234 + 0x0101 * i);
    cat_draw_pos = pos; buffer_size = dims;
}
static void model_alley_restore(uint16_t pos, int w, int h) { m_blit((const uint8_t *)alley_save_buf, pos, w, h); }

int main(void) {
    /* 1) prepare_l6_erase */
    fill();
    l6_dat_44d1 = 0x429c; l6_dat_43dc = 0x1000; l6_dat_44d0 = 0; l6_dat_43e0 = 1;
    draw_l6_tracker();
    CHECK(!same(), "1a: el tracker no se dibujo");
    l6_dat_44bd = 1;
    prepare_l6_erase();
    CHECK(same(), "1a: dat_44bd=1 -> erase_l6_tracker no dejo la pantalla original");
    CHECK(l6_dat_44bd == 0, "1a: dat_44bd=%u debia quedar 0", l6_dat_44bd);

    fill(); set_alley_buf(0x1200, 0x0a02);
    l6_dat_44bd = 0; l6_dat_43e0 = 0; l6_dat_43de = 0x1800;      /* si tocara el tracker, se veria en 0x1800 */
    prepare_l6_erase();
    model_alley_restore(0x1200, 2, 10);
    CHECK(same(), "1b: dat_44bd=0 -> restore_alley_buffer (2x10 en 0x1200)");
    CHECK(l6_dat_44bd == 0, "1b: dat_44bd cambio");

    /* 2) clear_l6_object: 5x13 words de 0xAA en dat_44da; no toca ds_pool */
    static uint8_t ds_before[0x100];
    memcpy(ds_before, ds_pool, sizeof ds_before);
    uint16_t addrs[3] = {0x1546, 0x17c0, 0x1a84};
    for (int i = 0; i < 3; i++) {
        fill(); l6_dat_44da = addrs[i];
        clear_l6_object();
        uint8_t blk[0x82]; memset(blk, 0xaa, sizeof blk);
        m_blit(blk, addrs[i], 5, 13);
        CHECK(same(), "2: clear_l6_object en %04x != modelo", addrs[i]);
        CHECK(cga_mem[addrs[i]] == 0xaa && cga_mem[addrs[i] + 9] == 0xaa && cga_mem[addrs[i] + 10] == (uint8_t)((addrs[i] + 10) * 7 + 3),
              "2: bordes del bloque en %04x", addrs[i]);
    }
    CHECK(memcmp(ds_before, ds_pool, sizeof ds_before) == 0, "2: clear_l6_object piso ds_pool[0..0xff]");

    /* 3) refresh_l6_display */
    fill(); set_alley_buf(0x1200, 0x0a02); cat_sprite_ptr = &ds_pool[0x431e]; cat_sprite_dims = 0x0a03;
    l6_dat_44d9 = 0;
    l6_dat_44bd = 1; refresh_l6_display();
    l6_dat_44bd = 0; refresh_l6_display();
    CHECK(same(), "3a: dat_44d9=0 no debe dibujar nada");

    l6_dat_44d9 = 1; l6_dat_44bd = 1; l6_dat_44d1 = 0x429c; l6_dat_43dc = 0x1000; l6_dat_44d0 = 0; l6_dat_43e0 = 1;
    refresh_l6_display();
    m_and(&ds_pool[0x429c], 0x1000, 3, 10);
    CHECK(same(), "3b: dat_44d9=1, dat_44bd=1 -> draw_l6_tracker (sprite AND fondo 3x10)");
    CHECK(l6_dat_43e0 == 0 && l6_dat_43de == 0x1000, "3b: estado del tracker e0=%u de=%x", l6_dat_43e0, l6_dat_43de);

    fill(); cat_draw_pos = 0x1400; l6_dat_44d9 = 1; l6_dat_44bd = 0;
    refresh_l6_display();
    m_and(&ds_pool[0x431e], 0x1400, 3, 10);
    CHECK(same(), "3c: dat_44d9=1, dat_44bd=0 -> draw_alley_foreground (3x10 en 0x1400)");
    CHECK(buffer_size == 0x0a03, "3c: buffer_size=%04x", buffer_size);

    /* 4) check_l6_proximity: rectangulos a mano. Objeto: x = dat_43e1[slot]-0x14, y = dat_43f9[slot], 0x28 x 6;
     * gato: 0x18 x 0x0e. Colision <=> cat_x en [x-0x18 (clamp 0), x+0x28] y cat_y en [y-0x0e, y+6].
     * slot 0: x=0x2c-0x14=0x18,y=0x88 -> cat_x [0x00,0x40], cat_y [0x7a,0x8e]
     * slot 4: x=0x5c-0x14=0x48,y=0x98 -> cat_x [0x30,0x70], cat_y [0x8a,0x9e]
     * slot 11: x=0x124-0x14=0x110,y=0xa8 -> cat_x [0xf8,0x138], cat_y [0x9a,0xae] */
    static const struct { uint16_t slot; int16_t cx; uint8_t cy; int hit; } cases[] = {
        {0, 0x30, 0x88, 1}, {0, 0x40, 0x8e, 1}, {0, 0x41, 0x88, 0}, {0, 0x30, 0x8f, 0}, {0, 0x30, 0x79, 0}, {0, 0x00, 0x7a, 1},
        {4, 0x30, 0x8a, 1}, {4, 0x2f, 0x8a, 0}, {4, 0x70, 0x9e, 1}, {4, 0x71, 0x9e, 0},
        {11, 0xf8, 0x9a, 1}, {11, 0xf7, 0x9a, 0}, {11, 0x138, 0xae, 1}, {11, 0x139, 0xae, 0}, {11, 0x100, 0x99, 0},
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        fill(); set_alley_buf(0x1200, 0x0a02);
        cat_x = cases[i].cx; cat_y = cases[i].cy;
        l6_dat_44d9 = 0x77; l6_dat_44bd = 0;
        check_l6_proximity(cases[i].slot);
        if (cases[i].hit) {
            model_alley_restore(0x1200, 2, 10);      /* dat_44bd=0 -> restore_alley_buffer */
            CHECK(l6_dat_44d9 == 1, "4 caso %u: dat_44d9=%u esperado 1", i, l6_dat_44d9);
            CHECK(same(), "4 caso %u: choque con dat_44bd=0 debia restaurar el fondo del callejon", i);
        } else {
            CHECK(l6_dat_44d9 == 0, "4 caso %u: dat_44d9=%u esperado 0", i, l6_dat_44d9);
            CHECK(same(), "4 caso %u: sin choque no debe tocar la pantalla", i);
        }
    }
    /* choque con el tracker dibujado (dat_44bd != 0) -> erase_l6_tracker, no restore_alley_buffer */
    fill(); set_alley_buf(0x1200, 0x0a02);
    l6_dat_44d1 = 0x429c; l6_dat_43dc = 0x1000; l6_dat_44d0 = 0; l6_dat_43e0 = 1;
    draw_l6_tracker(); l6_dat_44bd = 1; cat_x = 0x30; cat_y = 0x88;
    check_l6_proximity(0);
    CHECK(same() && l6_dat_44d9 == 1, "4t: choque con dat_44bd=1 debia borrar el tracker (y no tocar 0x1200)");
    CHECK(l6_dat_44bd == 1, "4t: check_l6_proximity no debe cambiar dat_44bd");

    /* 5) draw_l6_alert: slot 0 usa sprite 0x431e (src +6, x-6); slot 1 usa 0x429c */
    fill(); l6_obj_state[0] = 2;      /* src = 0x4100 + 4 + 6 = 0x410a (word 0x0303); dst = 0x1546+0xa7-6 = 0x15e7 */
    draw_l6_alert(0);
    CHECK(cga_mem[0x15e7] == 0x03 && cga_mem[0x15e8] == 0x03, "5a: bytes %02x %02x esperado 03 03", cga_mem[0x15e7], cga_mem[0x15e8]);
    { uint8_t w[2] = {0x03, 0x03}; m_blit(w, 0x15e7, 1, 1); }
    CHECK(same(), "5a: draw_l6_alert(0) toco algo mas que 2 bytes");
    fill(); l6_obj_state[1] = 1;      /* src = 0x4102 (word 0xffc0 -> c0 ff); dst = 0x155a+0xa7 = 0x1601 */
    draw_l6_alert(1);
    CHECK(cga_mem[0x1601] == 0xc0 && cga_mem[0x1602] == 0xff, "5b: bytes %02x %02x esperado c0 ff", cga_mem[0x1601], cga_mem[0x1602]);
    { uint8_t w[2] = {0xc0, 0xff}; m_blit(w, 0x1601, 1, 1); }
    CHECK(same(), "5b: draw_l6_alert(1) toco algo mas que 2 bytes");
    fill(); l6_obj_state[0] = 0;      /* src = 0x4106 (word 0xffff); dst = 0x15e7 */
    draw_l6_alert(0);
    { uint8_t w[2] = {0xff, 0xff}; m_blit(w, 0x15e7, 1, 1); }
    CHECK(same(), "5c: draw_l6_alert(0) estado 0");

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_level6b: OK\n");
    return 0;
}
