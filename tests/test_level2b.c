/* T34 — check_level_objects (level_objects.asm L690-805). Sin SDL. Modelo independiente escrito desde el ASM
 * (rect_collision con las restas sin signo del ASM, no la funcion bajo prueba). Ver PROGRESS.md §6ae. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level2.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* ---- relojes simulados: cada lectura de tick avanza 1; el retrace aparece cada 3 consultas ---- */
static uint16_t tick_now; static unsigned vs_calls, tick_calls;
static uint16_t fake_tick(void) { tick_calls++; return tick_now++; }
static bool fake_vsync(void) { return (++vs_calls % 3) == 0; }

/* ---- modelo ---- */
static bool m_rect(uint16_t ax, uint8_t dl, uint16_t si, uint8_t cl, uint16_t bx, uint8_t dh, uint16_t di, uint8_t ch) {
    uint32_t r = (uint32_t)ax + si; if (r > 0xffff) return false;             /* add ax,si / cmp ax,bx / jc: (ver nota) */
    if ((uint16_t)r < bx) return false;
    int32_t a = (int32_t)ax - di; if (a < 0) a = 0;
    if ((uint16_t)a > bx) return false;
    uint16_t b = (uint16_t)dl + cl; if (b > 0xff) return false;                /* add dl,cl: el carry cuenta como jc */
    if ((uint8_t)b < dh) return false;
    int32_t c = (int32_t)dl - ch; if (c < 0) c = 0;
    if ((uint8_t)c > dh) return false;
    return true;
}
static const uint16_t W[2] = {8, 0x10}, H[2] = {6, 2};
static const uint8_t PAT_BLOCK = 0x55;

static struct { uint8_t act[24], hit[24]; uint16_t x[24]; uint8_t y[24]; uint16_t cga[24]; uint8_t d3410, d351b, caught; } M;
static bool m_check(bool immune) {
    M.d351b = 0;
    for (unsigned s = 0; s < 24; s++) {
        if (M.act[s]) continue;
        unsigned k = s >= 12;
        if (!m_rect(M.x[s], M.y[s], W[k], (uint8_t)H[k], (uint16_t)cat_x, cat_y, 0x18, 0xe)) continue;
        if (!(s < 12 || M.caught || immune)) return 0xdead;                    /* golpe fatal: se prueba aparte */
        M.d351b++;
        M.act[s] = 1;
        if (s < 12 && --M.d3410 == 0 && !immune) M.caught = 1;
    }
    return M.d351b != 0;
}

static void reset_state(void) {
    memset(l2_obj_active, 0, sizeof l2_obj_active); memset(l2_obj_hit, 1, sizeof l2_obj_hit);
    memset(l2_obj_x, 0, sizeof l2_obj_x); memset(l2_obj_cga_addr, 0, sizeof l2_obj_cga_addr);
    for (int i = 0; i < 24; i++) l2_obj_y[i] = 0xa8;                          /* lejos del gato */
    for (int i = 0; i < 24; i++) l2_obj_x[i] = 0x100;
    l2_dat_3410 = 0xc; l2_dat_351b = 0; cat_caught = 0; immune_flag = 0; object_hit = 0;
    cat_x = 100; cat_y = 40;
    memset(cga_mem, 0, CGA_MEM_SIZE);
}
static size_t addr(uint8_t y, uint16_t x) { return (size_t)(y >> 1) * 80 + (y & 1) * 0x2000 + (x >> 2); }

int main(void) {
    l2_tick_fn = fake_tick; l2_vsync_fn = fake_vsync;

    /* 1. sin colision: false, nada cambia */
    reset_state();
    CHECK(check_level_objects() == false, "sin colision devuelve CF=0");
    CHECK(l2_dat_351b == 0 && l2_dat_3410 == 0xc && cat_caught == 0 && object_hit == 0, "sin colision: estado intacto");
    CHECK(l2_dat_3511 == 0x18, "el barrido termina en dat_3511 = 0x18 (es 0x%x)", l2_dat_3511);

    /* 2. bloque (slot 3) atrapado: active, dat_3410--, borra el patron, devuelve CF=1 */
    reset_state();
    l2_obj_x[3] = 100; l2_obj_y[3] = 40; l2_obj_hit[3] = 0; l2_obj_cga_addr[3] = (uint16_t)addr(40, 100);
    CHECK(check_level_objects() == true, "bloque atrapado devuelve CF=1");
    CHECK(l2_obj_active[3] == 1 && l2_dat_351b == 1 && l2_dat_3410 == 0xb, "bloque: active=1, 351b=1, 3410=0xb");
    CHECK(cat_caught == 0 && object_hit == 0, "bloque: gato no atrapado");
    CHECK(cga_mem[addr(40, 100)] == PAT_BLOCK && cga_mem[addr(40, 100) + 1] == PAT_BLOCK, "bloque: patron 0x55 en la fila 0");

    /* 2b. restore_alley_buffer SOLO en la primera captura del barrido: el slot 3 borra DENTRO de la zona restaurada; si el
     *     restore se repitiera en la 2a captura (slot 4) pisaria ese patron con el buffer guardado. */
    reset_state();
    memset((void *)alley_save_buf, 0xAB, 64);
    buffer_size = 0x0302; cat_draw_pos = (uint16_t)addr(120, 200);          /* 3 filas x 2 palabras */
    l2_obj_x[3] = 100; l2_obj_y[3] = 40; l2_obj_hit[3] = 0; l2_obj_cga_addr[3] = (uint16_t)addr(120, 200);
    l2_obj_x[4] = 100; l2_obj_y[4] = 40; l2_obj_hit[4] = 0; l2_obj_cga_addr[4] = (uint16_t)addr(160, 100);
    CHECK(check_level_objects() == true, "dos capturas devuelve CF=1");
    CHECK(l2_dat_351b == 2 && l2_dat_3410 == 0xa, "dos capturas: 351b=2, 3410=0xa");
    CHECK(cga_mem[addr(120, 200)] == PAT_BLOCK, "restore solo 1 vez: el patron del slot 3 sobrevive");
    CHECK(cga_mem[addr(121, 200) + 2] == 0xAB && cga_mem[addr(122, 200) + 3] == 0xAB, "restore: filas 1 y 2 del buffer en su sitio");
    reset_state(); memset((void *)alley_save_buf, 0xAB, 64);
    buffer_size = 0x0302; cat_draw_pos = (uint16_t)addr(120, 200);
    l2_obj_x[3] = 100; l2_obj_y[3] = 40;
    check_level_objects();
    CHECK(cga_mem[addr(120, 200)] == 0xAB, "una captura: restore_alley_buffer escribe el buffer guardado");

    /* 3. el ultimo bloque (dat_3410 = 1) atrapa al gato; con immune_flag no */
    reset_state(); l2_dat_3410 = 1;
    l2_obj_x[3] = 100; l2_obj_y[3] = 40;
    check_level_objects();
    CHECK(cat_caught == 1 && l2_dat_3410 == 0, "ultimo bloque: cat_caught = 1");
    reset_state(); l2_dat_3410 = 1; immune_flag = 1;
    l2_obj_x[3] = 100; l2_obj_y[3] = 40;
    check_level_objects();
    CHECK(cat_caught == 0 && l2_dat_3410 == 0, "ultimo bloque inmune: cat_caught = 0");

    /* 4. golpe fatal (slot 15): sprite, object_hit, bloquea >= 0xd ticks, CF=0, no marca active ni 351b */
    reset_state(); tick_now = 1000; tick_calls = 0; vs_calls = 0;
    l2_obj_x[15] = 100; l2_obj_y[15] = 40;
    CHECK(check_level_objects() == false, "golpe fatal devuelve CF=0");
    CHECK(object_hit == 1 && l2_obj_active[15] == 0 && l2_dat_351b == 0, "fatal: object_hit=1, sin captura");
    CHECK(l2_dat_3511 == 15, "fatal: el barrido se corta en el slot 15 (es %u)", l2_dat_3511);
    CHECK(tick_calls == 0xd + 1, "fatal: lecturas de tick = %u (1 inicial + 13 vueltas)", tick_calls);
    /* dx empujado en cada vuelta: tick0, 1, 2, ..., 12 -> la ultima (12) es par -> borde 0xf; con 1000 (par) la primera es 0xf tambien */
    CHECK(l2_border_color == 0xf, "fatal: ultimo borde = 0xf (es %u)", l2_border_color);
    {
        const uint8_t *sp = &ds_pool[0x3350]; int bad = 0;
        for (int r = 0; r < 0x12; r++) for (int b = 0; b < 10; b++) if (cga_mem[addr((uint8_t)(40 + r), 92) + b] != sp[r * 10 + b]) bad++;
        CHECK(bad == 0, "fatal: l1_anim_sprite_c en (92,40), %d bytes distintos", bad);
    }

    /* 5. clamps de posicion del sprite fatal */
    struct { int16_t cx; uint8_t cy; uint16_t ex; uint8_t ey; } cl[] = { {3, 0xb8, 0, 0xb4}, {0x130, 0x20, 0x116, 0x20}, {0x117 + 8, 0xb5, 0x116, 0xb4}, {0x116 + 8, 0xb4, 0x116, 0xb4}, {0x118 + 8, 0x20, 0x116, 0x20}, {0x115 + 8, 0x20, 0x115, 0x20} };
    for (unsigned i = 0; i < sizeof cl / sizeof cl[0]; i++) {
        reset_state(); tick_now = 0; vs_calls = 0;
        cat_x = cl[i].cx; cat_y = cl[i].cy; l2_obj_x[12] = (uint16_t)cat_x; l2_obj_y[12] = cat_y;
        check_level_objects();
        const uint8_t *sp = &ds_pool[0x3350]; int bad = 0;
        for (int r = 0; r < 0x12; r++) for (int b = 0; b < 10; b++) if (cga_mem[addr((uint8_t)(cl[i].ey + r), cl[i].ex) + b] != sp[r * 10 + b]) bad++;
        CHECK(bad == 0, "clamp caso %u: sprite en (%u,%u), %d bytes distintos", i, cl[i].ex, cl[i].ey, bad);
    }

    /* 6. objeto (slot >= 12) con gato atrapado o inmune: se captura como un bloque pero sin tocar dat_3410 */
    for (int mode = 0; mode < 2; mode++) {
        reset_state(); cat_caught = (uint8_t)(mode == 0); immune_flag = (uint8_t)(mode == 1);
        l2_obj_x[15] = 100; l2_obj_y[15] = 40; l2_obj_hit[15] = 0; l2_obj_cga_addr[15] = (uint16_t)addr(40, 100);
        bool r = check_level_objects();
        CHECK(r && object_hit == 0 && l2_obj_active[15] == 0 && l2_dat_3410 == 0xc, "slot>=12 con caught/immune (modo %d): r=%d", mode, r);
        /* reset_caught_objects lo desactiva de nuevo y manda el objeto a un borde: x = 0 (cat_x=100 <= 0xa0 -> 0x12e) */
        CHECK(l2_obj_x[15] == 0x12e, "reset_caught_objects reubica el objeto en x=0x12e (es 0x%x)", l2_obj_x[15]);
        CHECK(cga_mem[addr(40, 100)] == PAT_BLOCK, "objeto: patron de borrado 2x2");
    }

    /* 7. aleatorio: solo capturas (sin fatales) contra el modelo, 4000 escenarios */
    uint32_t rs = 12345; unsigned nhit = 0;
    #define RND() (rs = rs * 1664525u + 1013904223u, (rs >> 8))
    for (int it = 0; it < 4000; it++) {
        reset_state();
        bool immune = RND() & 1; immune_flag = immune;
        cat_caught = (uint8_t)(RND() % 3 != 0);
        cat_x = (int16_t)(RND() % 320); cat_y = (uint8_t)(RND() % 200);
        l2_dat_3410 = (uint8_t)(RND() % 4);
        for (int s = 0; s < 24; s++) {
            bool near = RND() % 3 == 0;
            if (s >= 12 && !cat_caught && !immune) near = false;
            l2_obj_x[s] = near ? (uint16_t)(cat_x + (int)(RND() % 40) - 20) : (uint16_t)(RND() % 340);
            l2_obj_y[s] = near ? (uint8_t)(cat_y + (int)(RND() % 24) - 12) : (uint8_t)(RND() % 255);
            l2_obj_active[s] = RND() % 5 == 0;
            l2_obj_hit[s] = 1;
        }
        /* nada activo en 12..23 al llegar a reset_caught_objects que cambie lo comparado: se compara solo 0..11 y los escalares */
        for (int s = 12; s < 24; s++) { if (!(cat_caught || immune)) { l2_obj_active[s] = 0; } }
        memset(&M, 0, sizeof M);
        for (int s = 0; s < 24; s++) { M.act[s] = l2_obj_active[s]; M.x[s] = l2_obj_x[s]; M.y[s] = l2_obj_y[s]; }
        M.d3410 = l2_dat_3410; M.caught = cat_caught;
        int fatal_possible = 0;
        for (int s = 12; s < 24; s++) if (!M.act[s] && m_rect(M.x[s], M.y[s], 0x10, 2, (uint16_t)cat_x, cat_y, 0x18, 0xe) && !(M.caught || immune)) fatal_possible = 1;
        if (fatal_possible) continue;
        bool exp = m_check(immune);
        bool got = check_level_objects();
        nhit += exp;
        CHECK(got == exp, "it %d: CF %d vs %d", it, got, exp);
        for (int s = 0; s < 12; s++) CHECK(l2_obj_active[s] == M.act[s], "it %d: active[%d]", it, s);
        CHECK(l2_dat_3410 == M.d3410 && cat_caught == M.caught, "it %d: 3410 %u/%u caught %u/%u", it, l2_dat_3410, M.d3410, cat_caught, M.caught);
        if (!exp) CHECK(l2_dat_351b == 0, "it %d: 351b", it);
    }
    CHECK(nhit > 300, "el aleatorio ejercita capturas (%u)", nhit);

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_level2b: OK (%u escenarios con captura)\n", nhit);
    return 0;
}
