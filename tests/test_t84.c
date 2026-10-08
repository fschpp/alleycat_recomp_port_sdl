/* T84: la cinematica de victoria del nivel 7 (run_victory_sequence) dibuja en cga_mem mientras bloquea; debe
 * presentar el video durante sus esperas (l7_step_hook), y lo presentado debe ir cambiando (animacion), no un
 * cuadro fijo. Uso: make test-t84 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level7_epilogue.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint16_t tk;
static uint16_t fake_tick(void) { return (uint16_t)(tk += 1); }

#define MAXS 4096
static uint32_t seen[MAXS]; static int nseen, calls;
static uint32_t hash_screen(void) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < CGA_MEM_SIZE; i++) { h ^= cga_mem[i]; h *= 16777619u; }
    return h;
}
static int nz[MAXS];
/* bytes distintos de cero en las 8 primeras filas del primer corazon con flecha (x=0x84, y=0x1c, 17 bytes de ancho:
 * 28 px de sprite + flecha). Los pasos siguientes (y+8, y+16...) no las tapan, asi que tras el paso 2 deben estar
 * restauradas al fondo (0 en este test). */
static size_t row_off(int y) { return (size_t)((y >> 1) * 80 + (y & 1) * 0x2000); }
static int count_nz(void) {
    int c = 0;
    for (int y = 0x1c; y < 0x1c + 8; y++)
        for (int b = 0x84 / 4; b < 0x84 / 4 + 17; b++) c += cga_mem[row_off(y) + (size_t)b] != 0;
    return c;
}
static void hook(void) {
    if (calls < MAXS) nz[calls] = count_nz();
    calls++;
    uint32_t h = hash_screen();
    for (int i = 0; i < nseen; i++) if (seen[i] == h) return;
    if (nseen < MAXS) seen[nseen++] = h;
}

int main(void) {
    cga_init(); l7_tick_fn = fake_tick;
    memset(cga_mem, 0xaa, CGA_MEM_SIZE);
    cat_x = 0x98; cat_y = 0x14; lives_count = 3; difficulty_level = 0;

    /* sin gancho: no debe romper (tests, NULL = nada) */
    l7_step_hook = NULL;
    run_victory_sequence();

    memset(cga_mem, 0x00, CGA_MEM_SIZE);
    cat_x = 0x98; cat_y = 0x14;
    l7_step_hook = hook;
    run_victory_sequence();
    printf("presentaciones=%d, cuadros distintos=%d, nz:", calls, nseen); for (int i = 0; i < calls && i < 8; i++) printf(" %d", nz[i]); printf("\n");
    CHECK(calls >= 10, "la cinematica presento el video solo %d veces", calls);
    CHECK(nseen >= 5, "la cinematica no se anima: solo %d cuadros distintos presentados", nseen);

    /* T84b: el corazon con flecha que baja (position_victory_cat) debe borrarse en cada paso. El cuadro 3 es aun el
     * primer paso (hay 3 presentaciones de espera); del 4 en adelante son los pasos 2..n. */
    CHECK(calls > 8 && nz[0] > 0, "el primer corazon no se dibujo (nz=%d)", calls > 0 ? nz[0] : -1);
    for (int i = 4; i < 8 && i < calls; i++)
        CHECK(nz[i] == 0, "cuadro %d: el corazon del primer paso no se borro (%d bytes sin restaurar)", i, nz[i]);

    printf(fails ? "test-t84: %d FALLOS\n" : "test-t84: OK\n", fails);
    return fails != 0;
}
