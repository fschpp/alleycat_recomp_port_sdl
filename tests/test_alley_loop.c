/* T19 — 600 frames headless del loop del callejón (entry.asm L131-180), sin SDL.
 * Reproduce main.c: clear_screen/render_sprites/setup_alley/init_* y el orden del loop,
 * con entrada simulada (derecha 200 frames, izquierda 200, derecha 200).
 * Uso: make test-alley   → escribe build/alley_f{0,150,300,600}.ppm */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "cga.h"
#include "cat_state.h"
#include "input.h"
#include "game_setup.h"
#include "alley_movement.h"
#include "enemy.h"
#include "cycle_objects.h"
#include "jump_gravity.h"
#include "fall_object.h"
#include "score.h"
#include "alley_drawing.h"
#include "throw.h"
#include "sound.h"

/* stubs de input.c / video.c / audio.c */
int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
bool audio_init(void) { return false; } void audio_shutdown(void) {}

static void dump_ppm(const char *path) {
    static const uint8_t pal[4][3] = {{0,0,0},{0,255,255},{255,0,255},{255,255,255}};
    FILE *f = fopen(path, "wb"); if (!f) return;
    fprintf(f, "P6\n320 200\n255\n");
    for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) {
        size_t off = (size_t)((y & 1) ? 0x2000 : 0) + (size_t)(y >> 1) * 80 + (size_t)(x >> 2);
        int idx = (cga_mem[off] >> (6 - 2 * (x & 3))) & 3;
        fwrite(pal[idx], 1, 3, f);
    }
    fclose(f);
}
static void counts(const char *tag) {
    long c[4] = {0};
    for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) {
        size_t off = (size_t)((y & 1) ? 0x2000 : 0) + (size_t)(y >> 1) * 80 + (size_t)(x >> 2);
        c[(cga_mem[off] >> (6 - 2 * (x & 3))) & 3]++;
    }
    printf("%-6s px: c0=%ld c1=%ld c2=%ld c3=%ld | cat_x=%d cat_y=%u lives=%u floor=%u\n",
           tag, c[0], c[1], c[2], c[3], (int)cat_x, (unsigned)cat_y, (unsigned)lives_count,
           (unsigned)current_floor);
}

int main(void) {
    cga_init();
    lives_count = 3; clear_score(); clear_high_score(); level_number = 0;
    clear_screen(); render_sprites(); lives_display = 0xff; silence_speaker();
    level_number = 0; cat_x = 0; setup_alley();
    init_sound(); init_player(); reset_jump(); init_cycle_objects();
    draw_high_score_display(); draw_current_score(); init_music();
    dump_ppm("build/alley_f0.ppm"); counts("f0");
    int fail = 0;
    for (int f = 1; f <= 600; f++) {
        input_horizontal = (f <= 200 || f > 400) ? 1 : -1;
        immune_flag = 0;
        update_alley_movement();
        update_enemies();
        bool run = true;
        if (enemy_active == 0) { frame_counter++; if (frame_counter & 3) run = false; }
        if (run) {
            play_sound(); update_thrown_objects(); update_cat_jump(); apply_cat_gravity();
            animate_falling(); update_cycle_objects(); draw_lives();
            if (cat_died) { counts("DIED"); break; }
        }
        if (current_floor > 2) { printf("FAIL current_floor=%u\n", (unsigned)current_floor); fail = 1; }
        if (f == 150 || f == 300) { char p[64]; snprintf(p, sizeof p, "build/alley_f%d.ppm", f); dump_ppm(p); counts(p + 6); }
        struct timespec ts = {0, 33000000L}; nanosleep(&ts, NULL);
    }
    dump_ppm("build/alley_f600.ppm"); counts("f600");
    return fail;
}
