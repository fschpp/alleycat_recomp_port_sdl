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
/* T84c: el segundo sprite de position_victory_cat (dat_4a82) es un gato de 4 words x 13 filas (mov cx,0xd04), no
 * 13x4. Tras el primer paso (x=0x84, y=0x1c) las 13 filas en addr+0xf3 deben ser exactamente esos bytes (blit
 * opaco), y la fila 14 (debajo) no debe ser del gato. nz[] guarda cuantas filas coinciden. */
static int icon_rows_match(void) {
    extern const uint8_t ds_pool[];
    size_t a = (size_t)calc_cga_addr(0x1c, 0x84, NULL) + 0xf3;
    int ok = 0;
    for (int r = 0; r < 13; r++) {
        if (memcmp(&cga_mem[a], &ds_pool[0x4a82 + (size_t)r * 8], 8) == 0) ok++;
        a ^= 0x2000; if ((a & 0x2000) == 0) a += 0x50;
    }
    return ok;
}
static int count_nz(void) { return icon_rows_match(); }
static int last_total;
static int total_nz(void) { int c = 0; for (size_t i = 0; i < CGA_MEM_SIZE; i++) c += cga_mem[i] != 0; return c; }
static void hook(void) {
    last_total = total_nz();
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

    CHECK(calls > 8, "pocas presentaciones (%d)", calls);
    /* T84d: las oleadas (init_victory_wave) dibujan los 8 corazones tras la primera pasada (l7_cupid_active se pone
     * a 0 en cada pasada, lab_520c) y llenan la pantalla; con solo el lider quedaban ~4700 bytes, ahora ~7800. */
    CHECK(last_total > 6500, "las oleadas de corazones no llenan la pantalla (%d bytes dibujados en el ultimo cuadro)", last_total);
    CHECK(nz[0] == 13, "el gato del primer paso no coincide con dat_4a82 4x13 (%d/13 filas)", nz[0]);

    printf(fails ? "test-t84: %d FALLOS\n" : "test-t84: OK\n", fails);
    return fails != 0;
}
