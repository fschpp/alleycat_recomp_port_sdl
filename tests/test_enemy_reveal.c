/* T76 — recorte parcial ("reveal") del perro en la aproximacion (enemy.asm L564-593).
 * Fuerza la aparicion del perro (fall_hit=1) con el gato a la izquierda (entra por la derecha,
 * enemy_dir=-1) y a la derecha (entra por la izquierda, enemy_dir=+1) y avanza 5 ticks.
 * Comprueba por tick: ancho = 4-al words, enemy_x, y que los pixeles dibujados son exactamente
 * los del frame completo recortado (parte delantera si entra por la derecha, trasera si por la
 * izquierda), y que nada se "enrolla" a la fila siguiente (x=0x138 + 8 px = 320).
 * Uso: make test-enemy-reveal -> build/enemy_reveal_{R,L}_t{1..5}.ppm */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "enemy.h"
#include "bios_clock.h"
#include "sound.h"
#include "gen/enemy_sprites_ex.h"

/* stubs de input.c / video.c / audio.c */
int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
bool audio_init(void) { return false; } void audio_shutdown(void) {}

static uint16_t fake_tick;
static uint16_t hook(void) { return fake_tick; }

static int px(int x, int y) {
    size_t off = (size_t)((y & 1) ? 0x2000 : 0) + (size_t)(y >> 1) * 80 + (size_t)(x >> 2);
    return (cga_mem[off] >> (6 - 2 * (x & 3))) & 3;
}
static void dump_ppm(const char *path) {
    static const uint8_t pal[4][3] = {{0,0,0},{0,255,255},{255,0,255},{255,255,255}};
    FILE *f = fopen(path, "wb"); if (!f) return;
    fprintf(f, "P6\n320 200\n255\n");
    for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) fwrite(pal[px(x, y)], 1, 3, f);
    fclose(f);
}
/* Dibuja el frame completo sobre pantalla limpia en x0 y devuelve sus pixeles. */
static void ref_full(const cat_walk_frame_t *fr, int x0, int ref[15][32]) {
    uint8_t save[CGA_MEM_SIZE]; memcpy(save, cga_mem, sizeof save);
    memset(cga_mem, 0, CGA_MEM_SIZE);
    uint16_t tmp[60];
    blit_transparent(fr->data, (size_t)calc_cga_addr(enemy_y_pos, (uint16_t)x0, NULL), 4, 15, tmp);
    for (int r = 0; r < 15; r++) for (int c = 0; c < 32; c++)
        ref[r][c] = (x0 + c < 320) ? px(x0 + c, enemy_y_pos + r) : 0;
    memcpy(cga_mem, save, sizeof save);
}

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void run(const char *tag, int16_t catx, int8_t want_dir) {
    cga_init(); memset(cga_mem, 0, CGA_MEM_SIZE);
    level_number = 0; lives_count = 3; cat_x = catx; cat_y = 0x60; fall_hit = 1;
    init_sound(); silence_speaker();
    bios_clock_hook = hook; fake_tick = 100; enemy_last_tick = 0; enemy_tick_counter = 0;
    for (int t = 1; t <= 5; t++) {
        fake_tick = (uint16_t)(fake_tick + 3);
        update_enemies();
        char p[64]; snprintf(p, sizeof p, "build/enemy_reveal_%s_t%d.ppm", tag, t); dump_ppm(p);
        printf("%s t%d: approach=%u chasing=%u dir=%d x=%u dims=0x%04x\n", tag, t,
               (unsigned)enemy_approach_timer, (unsigned)enemy_chasing, (int)enemy_dir,
               (unsigned)enemy_x, (unsigned)enemy_sprite_dims);
        if (t == 1) CHECK(enemy_dir == want_dir, "%s: enemy_dir=%d", tag, (int)enemy_dir);
        if (t <= 3) {                                    /* approach_timer = 3,2,1 -> al = timer */
            unsigned al = 4u - t, cl = 4u - al;
            CHECK(enemy_approach_timer == al, "%s t%d: timer=%u", tag, t, (unsigned)enemy_approach_timer);
            CHECK((enemy_sprite_dims & 0xff) == cl && (enemy_sprite_dims >> 8) == 15,
                  "%s t%d: dims=0x%04x esperado 0x0f%02x", tag, t, (unsigned)enemy_sprite_dims, cl);
            unsigned wantx = (want_dir == -1) ? 0x120u + al * 8u : 0u;
            CHECK(enemy_x == wantx, "%s t%d: enemy_x=0x%x esperado 0x%x", tag, t, (unsigned)enemy_x, wantx);
            const cat_walk_frame_t *fr = &enemy_sprite_frames[(enemy_sprite_ptr / 2) > 7 ? 7 : enemy_sprite_ptr / 2];
            int ref[15][32]; ref_full(fr, (want_dir == -1) ? (int)enemy_x : 0, ref);
            int skip = (want_dir == -1) ? 0 : (int)(al * 8);
            int bad = 0;
            for (int r = 0; r < 15; r++) for (int c = 0; c < (int)(cl * 8); c++)
                if (px((int)enemy_x + c, enemy_y_pos + r) != ref[r][skip + c]) bad++;
            CHECK(bad == 0, "%s t%d: %d pixeles distintos del frame completo recortado", tag, t, bad);
            if (want_dir == -1) {                        /* nada enrollado al borde izquierdo */
                int wrap = 0;
                for (int r = 0; r < 16; r++) for (int c = 0; c < 8; c++) if (px(c, enemy_y_pos + r)) wrap++;
                CHECK(wrap == 0, "%s t%d: %d pixeles enrollados a x=0", tag, t, wrap);
            }
        } else if (t == 4) {
            CHECK(enemy_chasing == 1 && enemy_approach_timer == 0, "%s t4: no pasa a persecucion", tag);
        }
    }
}

int main(void) {
    run("R", 0x30, -1);     /* gato a la izquierda: el perro entra por la derecha */
    run("L", 0xd0, 1);      /* gato a la derecha: el perro entra por la izquierda */
    if (fails) { printf("test_enemy_reveal: %d FALLO(S)\n", fails); return 1; }
    printf("test_enemy_reveal: OK\n");
    return 0;
}
