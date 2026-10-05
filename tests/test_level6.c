/* T29 — nivel 6, helpers A (level_objects.asm L2984-3095). Sin SDL.
 * Los valores esperados NO salen del C: direcciones CGA calculadas a mano con la formula
 * (fila&1)*0x2000 + (fila>>1)*80 + x/4, un modelo del LFSR escrito desde cga.asm (L158-165) y un modelo
 * de la pantalla que dibuja objetos y luego tiles en el orden del ASM. Ver PROGRESS.md §6z. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level6.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint16_t W(uint16_t off) { return (uint16_t)(ds_pool[off] | (ds_pool[off + 1] << 8)); }

/* Modelo del LFSR de cga.asm: dl = lo^hi; shr dl,1 x2 (CF = bit 1 de dl); rcr seed,1; devuelve el seed nuevo. */
static uint16_t m_seed;
static uint16_t m_random(void) {
    uint8_t dl = (uint8_t)((m_seed & 0xff) ^ (m_seed >> 8));
    unsigned cf = (dl >> 1) & 1;
    m_seed = (uint16_t)((cf << 15) | (m_seed >> 1));
    return m_seed;
}

/* Pantalla modelo: bloque w words x h filas en `at` (banco 0), fila r en at + (r&1)*0x2000 + (r>>1)*80 */
static uint8_t model[CGA_MEM_SIZE];
static void m_blit(const uint8_t *src, uint16_t at, int w, int h) {
    for (int r = 0; r < h; r++)
        memcpy(&model[at + (r & 1) * 0x2000 + (r >> 1) * 80], src + r * w * 2, (size_t)w * 2);
}

static void fill(void) { for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + 3); memcpy(model, cga_mem, CGA_MEM_SIZE); }

int main(void) {
    /* 1) calc_l6_addr a mano para tres tiles: (y=0x90,x=0x08), (0xa0,0x28), (0xb0,0x10) y el ultimo (0xb0,0x100) */
    CHECK(calc_l6_addr(0)  == 0x1682, "tile 0: %x", calc_l6_addr(0));    /* 72*80 + 2 = 5762 */
    CHECK(calc_l6_addr(3)  == 0x190a, "tile 3: %x", calc_l6_addr(3));    /* 80*80 + 10 = 6410 */
    CHECK(calc_l6_addr(8)  == 0x1b84, "tile 8: %x", calc_l6_addr(8));    /* 88*80 + 4 = 7044 */
    CHECK(calc_l6_addr(11) == 0x1bc0, "tile 11: %x", calc_l6_addr(11));  /* 88*80 + 64 = 7104 */

    /* 2) draw_l6_tile: tile 3 tipo 2 -> 2 words x 8 filas desde 0x41fc + 2*32 */
    fill(); l6_tile_type[3] = 2;
    draw_l6_tile(3);
    m_blit(&ds_pool[0x41fc + 2 * 32], 0x190a, 2, 8);
    CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) == 0, "draw_l6_tile != modelo");
    CHECK(memcmp(&cga_mem[0x190a], &ds_pool[0x41fc + 64], 4) == 0, "fila 0 del tile");

    /* 3) init_level6_objects: para cada dificultad, comparar pantalla, flags, estado y rng_seed con el modelo */
    static const uint8_t ntype[8] = {6, 6, 7, 7, 8, 8, 9, 9};
    static const uint8_t ntile[8] = {1, 2, 3, 4, 4, 4, 4, 4};
    for (int d = 0; d < 8; d++) {
        fill();
        difficulty_level = (uint16_t)d; rng_seed = 0xace1 + (uint16_t)(d * 0x111);
        m_seed = rng_seed;
        for (int i = 0; i < 12; i++) { l6_obj_flag[i] = 1; l6_obj_state[i] = 0xbeef; l6_tile_type[i] = 0xee; }
        l6_dat_44d0 = 0x55; l6_dat_44bd = 0x55; l6_dat_44be = 0x55; l6_dat_43e0 = 0; l6_dat_44d6 = 0;
        init_level6_objects();
        /* modelo: objetos primero (orden del ASM), luego los 12 tiles de 11 a 0 */
        uint16_t mflag[12] = {0}; int placed = 0;
        while (placed < ntype[d]) {
            uint8_t bl = (uint8_t)(m_random() & 0xff) & 0x1e;
            if (bl >= 0x18 || mflag[bl >> 1]) continue;
            mflag[bl >> 1] = 1; placed++;
            m_blit(&ds_pool[W((uint16_t)(0x4429 + bl))], W((uint16_t)(0x4411 + bl)), 5, 13);
        }
        for (int t = 11; t >= 0; t--) {
            static const uint8_t ty[12] = {0x90,0x90,0x90,0xa0,0xa0,0xa0,0xa0,0xa0,0xb0,0xb0,0xb0,0xb0};
            static const uint16_t tx[12] = {0x8,0x90,0xa0,0x28,0x38,0x78,0xe0,0x120,0x10,0x98,0xd0,0x100};
            uint16_t a = (uint16_t)((ty[t] >> 1) * 80 + tx[t] / 4);    /* filas pares: banco 0 */
            m_blit(&ds_pool[0x41fc + ntile[d] * 32], a, 2, 8);
        }
        CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) == 0, "dif %d: pantalla != modelo", d);
        CHECK(rng_seed == m_seed, "dif %d: rng_seed %04x != %04x (numero de random())", d, rng_seed, m_seed);
        int n = 0, ok = 1;
        for (int i = 0; i < 12; i++) {
            n += l6_obj_flag[i];
            if (l6_obj_flag[i] != mflag[i]) ok = 0;
            if (mflag[i] ? l6_obj_state[i] != 0 : l6_obj_state[i] != 0xbeef) ok = 0;   /* solo los slots usados */
            if (l6_tile_type[i] != ntile[d]) ok = 0;
        }
        CHECK(ok && n == ntype[d], "dif %d: flags/estado/tiles (n=%d)", d, n);
        CHECK(l6_dat_44d0 == 0 && l6_dat_44bd == 0 && l6_dat_44be == 0 && l6_dat_43e0 == 1 && l6_dat_44d6 == 0xc,
              "dif %d: variables finales", d);
    }
    /* los slots posibles son 0..11 (bx = 0..0x16): con dificultad 7 hay 9 de 12 ocupados */
    difficulty_level = 7; fill(); rng_seed = 0x1234; init_level6_objects();
    { int n = 0; for (int i = 0; i < 12; i++) n += l6_obj_flag[i]; CHECK(n == 9, "dif 7: %d objetos", n); }

    /* 4) tracker: draw guarda el fondo y hace sprite AND fondo; erase lo restaura; con dat_43e0 != 0 erase no hace nada */
    fill();
    l6_dat_44d1 = 0x429c; l6_dat_43dc = 0x1000; l6_dat_44d0 = 0x7f; l6_dat_43e0 = 1; l6_dat_43de = 0;
    erase_l6_tracker();
    CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) == 0, "erase con dat_43e0=1 toco la pantalla");
    draw_l6_tracker();
    CHECK(l6_dat_43e0 == 0 && l6_dat_43de == 0x1000, "draw: e0=%u de=%x", l6_dat_43e0, l6_dat_43de);
    {
        int ok = 1;
        for (int r = 0; r < 10; r++)
            for (int b = 0; b < 6; b++) {
                size_t at = 0x1000 + (size_t)(r & 1) * 0x2000 + (size_t)(r >> 1) * 80 + (size_t)b;
                if (cga_mem[at] != (uint8_t)(ds_pool[0x429c + r * 6 + b] & model[at])) ok = 0;
            }
        CHECK(ok, "draw (44d0=0x7f): sprite AND fondo");
        CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) != 0, "draw no dibujo nada");
    }
    erase_l6_tracker();
    CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) == 0, "round-trip draw/erase deja residuo");
    /* dat_44d0 >= 0x80: copia espejada (+0x3c); la direccion borrada es la del dibujo, no la nueva dat_43dc */
    l6_dat_44d0 = 0x80; l6_dat_43dc = 0x1010;
    draw_l6_tracker();
    l6_dat_43dc = 0x1500;
    {
        int ok = 1;
        for (int r = 0; r < 10; r++)
            for (int b = 0; b < 6; b++) {
                size_t at = 0x1010 + (size_t)(r & 1) * 0x2000 + (size_t)(r >> 1) * 80 + (size_t)b;
                if (cga_mem[at] != (uint8_t)(ds_pool[0x429c + 0x3c + r * 6 + b] & model[at])) ok = 0;
            }
        CHECK(ok, "draw (44d0=0x80): sprite +0x3c AND fondo");
    }
    erase_l6_tracker();
    CHECK(memcmp(cga_mem, model, CGA_MEM_SIZE) == 0, "erase usa dat_43de (no dat_43dc)");

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_level6: OK\n");
    return 0;
}
