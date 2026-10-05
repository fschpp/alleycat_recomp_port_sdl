/* T28 — update_level5_objects (level_objects.asm L2438-2602). Sin SDL.
 * Los valores esperados se calcularon a mano rama por rama desde el ASM (no desde el C; no hay emulador
 * x86 en este entorno). Ver PROGRESS.md §6y. restore_alley_buffer, save_cat_background, start_tone y
 * silence_speaker se interceptan con -Wl,--wrap. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "level5.h"
#include "speaker.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static int n_restore, n_save, n_tone, n_silence;
static uint16_t tone_ax, tone_bx, sil_div; static uint8_t sil_p61;
void __wrap_restore_alley_buffer(void) { n_restore++; }
void __wrap_save_cat_background(void) { n_save++; }
void __wrap_start_tone(uint16_t a, uint16_t b) { n_tone++; tone_ax = a; tone_bx = b; }
void __real_silence_speaker(void);
void __wrap_silence_speaker(void) {
    n_silence++; sil_div = speaker_current_divisor(); sil_p61 = speaker_current_port61();
    __real_silence_speaker();
}

static uint8_t bg[CGA_MEM_SIZE];

/* blit_masked: resultado = sprite AND fondo, palabra a palabra (ver cga.c). */
static int row_is_and(size_t at, const uint8_t *spr, int nbytes) {
    for (int i = 0; i < nbytes; i++) if (cga_mem[at + i] != (uint8_t)(spr[i] & bg[at + i])) return 0;
    return 1;
}

/* Perch dibujado en (0x90,0x86) sobre un fondo conocido; el resto del estado como tras init_level5_objects. */
static void reset(void) {
    for (size_t i = 0; i < CGA_MEM_SIZE; i++) cga_mem[i] = (uint8_t)(i * 7 + 3);
    memcpy(bg, cga_mem, CGA_MEM_SIZE);
    init_level5_objects();
    l5_dat_40ad = 0; l5_dat_40b0 = 0; l5_dat_40b1 = 0; l5_dat_40af = 0;
    cat_x = 0x10; cat_y = 0x60; scroll_direction = 0; in_level_mode = 0; transition_timer = 0;
    thrown_obj_x = 300; thrown_obj_y = 0;          /* lejos: check_l5_thrown_near = false */
    enemy_chasing = 0; sound_enabled = 0; object_hit = 0; cat_y_bottom = 0; speaker_reset();
    l5_tick_override = 100; l5_tick_advance = 0;
    n_restore = n_save = n_tone = n_silence = 0;
}

int main(void) {
    /* 1) mismo tick: no hace nada; perch >= 0xa4: guarda el tick y sale */
    reset(); l5_dat_40ad = 100;
    update_level5_objects();
    CHECK(l5_dat_40ad == 100 && l5_dat_40b1 == 0 && n_tone == 0, "mismo tick");
    reset(); l5_dat_40aa = 0xa4; cat_x = 0x90; cat_y = 0x86;
    update_level5_objects();
    CHECK(l5_dat_40ad == 100 && cat_x == 0x90 && l5_dat_40af == 0, "perch bajado: ad=%u cat_x=%x", l5_dat_40ad, cat_x);

    /* 2) objeto lanzado cerca del perch (y >= 0x66, 0x7c <= x <= 0xbc): corta y mira el aterrizaje */
    reset(); thrown_obj_x = 0x7c; thrown_obj_y = 0x66; cat_x = 0x90; cat_y = 0x88;
    update_level5_objects();
    CHECK(in_level_mode == 1 && transition_timer == 0x10 && cat_x == 0x90 && l5_dat_40af == 0,
          "cerca+aterrizado: m=%d t=%x cat_x=%x", in_level_mode, transition_timer, cat_x);
    reset(); thrown_obj_x = 0xbc; thrown_obj_y = 0x66; cat_x = 0x90; cat_y = 0x70;   /* sobre el perch, sin aterrizar */
    update_level5_objects();
    CHECK(in_level_mode == 0 && transition_timer == 0 && cat_x == 0x90 && l5_dat_40af == 0 && l5_dat_40ad == 100,
          "cerca sin aterrizar: m=%d cat_x=%x af=%u", in_level_mode, cat_x, l5_dat_40af);
    reset(); thrown_obj_x = 0xbd; thrown_obj_y = 0x66; cat_x = 0x90; cat_y = 0x86;   /* 0xbd: ya no cerca -> empuja */
    update_level5_objects();
    CHECK(in_level_mode == 0 && l5_dat_40af == 1, "limite superior de 'cerca': af=%u", l5_dat_40af);
    reset(); thrown_obj_x = 0x90; thrown_obj_y = 0x65; cat_x = 0x90; cat_y = 0x86;   /* y < 0x66: no cerca */
    update_level5_objects();
    CHECK(l5_dat_40af == 1, "limite inferior de y: af=%u", l5_dat_40af);

    /* 3) el gato pisa el perch: empujon. cat_x=0x90 (== perch x, no es 'ja') -> dir 0xff (der.), +8 hasta 0xb0 */
    reset(); cat_x = 0x90; cat_y = 0x86;
    update_level5_objects();
    CHECK(l5_dat_40b0 == 0xff && l5_dat_40af == 1 && cat_x == 0xb0 && scroll_direction == 1 && cat_y == 0x86,
          "empujon der.: b0=%02x cat_x=%x sd=%d cat_y=%x", l5_dat_40b0, cat_x, scroll_direction, cat_y);
    CHECK(n_restore == 1 && n_save == 1, "empujon: restore=%d save=%d", n_restore, n_save);
    /* gato a la izquierda (0x80 < 0x90, scroll_direction == 0): dir 1 (izq.), -8: 0x78 todavia choca, 0x70 no */
    reset(); cat_x = 0x80; cat_y = 0x86;
    update_level5_objects();
    CHECK(l5_dat_40b0 == 1 && cat_x == 0x70 && scroll_direction == -1, "empujon izq.: b0=%02x cat_x=%x sd=%d", l5_dat_40b0, cat_x, scroll_direction);
    /* scroll_direction != 0: dat_40b0 toma ese valor tal cual (1), aunque el gato este a la derecha */
    reset(); cat_x = 0xa0; cat_y = 0x86; scroll_direction = 1;
    update_level5_objects();
    CHECK(l5_dat_40b0 == 1 && cat_x == 0x70 && scroll_direction == -1, "empujon sd=1: b0=%02x cat_x=%x", l5_dat_40b0, cat_x);   /* 0xa0 -> 6 pasos de -8 */
    /* dat_40af != 0 ya: no recalcula dat_40b0 */
    reset(); cat_x = 0x90; cat_y = 0x86; l5_dat_40af = 1; l5_dat_40b0 = 0x1;
    update_level5_objects();
    CHECK(l5_dat_40b0 == 1 && cat_x == 0x70, "af=1 conserva b0: b0=%u cat_x=%x", l5_dat_40b0, cat_x);   /* 0x90 -> 0x70: 4 pasos de -8 */
    /* in_level_mode: 1 -> cat_y -3 por paso (4 pasos); 2 -> +3 por paso; cat_y_bottom = cat_y + 0x32 */
    reset(); cat_x = 0x90; cat_y = 0x86; in_level_mode = 1;
    update_level5_objects();
    CHECK(cat_x == 0xb0 && cat_y == 0x7a && cat_y_bottom == 0xac, "modo 1: cat_x=%x cat_y=%x yb=%x", cat_x, cat_y, cat_y_bottom);
    reset(); cat_x = 0x90; cat_y = 0x86; in_level_mode = 2;
    update_level5_objects();
    CHECK(cat_x == 0xb0 && cat_y == 0x92 && cat_y_bottom == 0xc4, "modo 2: cat_x=%x cat_y=%x yb=%x", cat_x, cat_y, cat_y_bottom);
    reset(); cat_x = 0x90; cat_y = 0x86; in_level_mode = 0;
    update_level5_objects();
    CHECK(cat_y_bottom == 0, "modo 0 no toca cat_y_bottom: %x", cat_y_bottom);

    /* 4) sin choque con el perch */
    reset(); update_level5_objects();                                  /* b1 == 0 y af == 0: nada */
    CHECK(l5_dat_40b1 == 0 && l5_dat_40a8 == 0x90 && n_tone == 0 && l5_dat_40ad == 100, "nada: b1=%u", l5_dat_40b1);
    /* perch empujado a la derecha (b0 == 1): +8, tono, redibuja en la nueva X, y 0x98 esta en [0x78,0xa8] -> sale */
    reset(); l5_dat_40af = 1; l5_dat_40b0 = 1;
    update_level5_objects();
    CHECK(l5_dat_40a8 == 0x98 && l5_dat_40af == 0 && l5_dat_40b1 == 0 && n_tone == 1 && tone_ax == 0xc00 && tone_bx == 0xb54,
          "perch der.: x=%x af=%u b1=%u tone=%d %x %x", l5_dat_40a8, l5_dat_40af, l5_dat_40b1, n_tone, tone_ax, tone_bx);
    {
        size_t newa = calc_cga_addr(0x86, 0x98, NULL), olda = calc_cga_addr(0x86, 0x90, NULL);
        CHECK(l5_dat_40ab == newa && l5_dat_40a6 == newa, "perch der.: ab=%x a6=%x != %zx", l5_dat_40ab, l5_dat_40a6, newa);
        /* el perch se movio 8 px = 2 bytes: solo el primer word de la posicion vieja queda fuera del nuevo */
        CHECK(memcmp(&cga_mem[olda], &bg[olda], 2) == 0, "perch der.: la posicion vieja quedo sucia");
        CHECK(memcmp(&cga_mem[newa], &bg[newa], 6) != 0, "perch der.: no dibujo en la nueva posicion");
        CHECK(row_is_and(newa, &ds_pool[0x3fbe], 6), "perch der.: fila 0 != sprite AND fondo");
    }
    /* b0 != 1 (0xff) -> -8. 0x80 -> 0x78: dentro; 0x78 -> 0x70: fuera de rango -> pasa a lab_46a2 */
    reset(); l5_dat_40af = 1; l5_dat_40b0 = 0xff; l5_dat_40a8 = 0x80;
    update_level5_objects();
    CHECK(l5_dat_40a8 == 0x78 && l5_dat_40b1 == 0 && n_tone == 1, "perch izq. dentro: x=%x b1=%u", l5_dat_40a8, l5_dat_40b1);
    reset(); l5_dat_40af = 1; l5_dat_40b0 = 0xff; l5_dat_40a8 = 0x78; enemy_chasing = 1; cat_y = 0x88;
    update_level5_objects();
    CHECK(l5_dat_40a8 == 0x70 && l5_dat_40b1 == 1 && in_level_mode == 1 && transition_timer == 0x10,
          "perch izq. fuera + perseguido + aterrizado: x=%x b1=%u m=%d", l5_dat_40a8, l5_dat_40b1, in_level_mode);
    reset(); l5_dat_40af = 1; l5_dat_40b0 = 1; l5_dat_40a8 = 0xa8; enemy_chasing = 1; cat_y = 0x60;   /* 0xb0 > 0xa8 */
    update_level5_objects();
    CHECK(l5_dat_40a8 == 0xb0 && l5_dat_40b1 == 1 && in_level_mode == 0 && transition_timer == 0,
          "perch der. fuera + perseguido sin aterrizar: x=%x b1=%u m=%d", l5_dat_40a8, l5_dat_40b1, in_level_mode);
    reset(); l5_dat_40af = 1; l5_dat_40b0 = 1; l5_dat_40a8 = 0xa0;                                  /* 0xa8: limite incluido */
    update_level5_objects();
    CHECK(l5_dat_40a8 == 0xa8 && l5_dat_40b1 == 0, "perch der. 0xa8 incluido: b1=%u", l5_dat_40b1);
    /* b1 != 0 (perch ya empujado una vez): salta directo a lab_46a2 con enemigo */
    reset(); l5_dat_40b1 = 1; enemy_chasing = 1; cat_y = 0x88;
    update_level5_objects();
    CHECK(in_level_mode == 1 && transition_timer == 0x10 && l5_dat_40aa == 0x86, "b1=1 + perseguido + aterrizado");

    /* 5) descenso bloqueante del perch (un paso por tick BIOS, con tono y sonido activado) */
    reset(); l5_dat_40b1 = 1; sound_enabled = 0xff; cat_x = 0x10; cat_y = 0x60;
    l5_tick_override = 100; l5_tick_advance = 1;
    update_level5_objects();
    CHECK(l5_dat_40aa == 0xa4 && l5_dat_40a8 == 0x90, "descenso: aa=%x a8=%x", l5_dat_40aa, l5_dat_40a8);
    /* ticks leidos: 1 (entrada) + 7 (6 pasos de +5 y la pasada final) => 108; el ultimo leido fue 107, siguiente 108 */
    CHECK(l5_tick_override == 108 && l5_dat_40ad == 107, "ticks: next=%d ad=%u", (int)l5_tick_override, l5_dat_40ad);
    CHECK(n_silence == 1 && sil_div == 0xa4 * 4, "silencio: n=%d div=%x (esperado 0x290)", n_silence, sil_div);
    CHECK((speaker_current_port61() & 3) == 0 && (sil_p61 & 3) == 3, "port61 antes=%02x despues=%02x", sil_p61, speaker_current_port61());
    CHECK(l5_dat_40b2 == 0x90 && l5_dat_40b4 == 0xa4, "objeto movil en el perch: %x,%x", l5_dat_40b2, l5_dat_40b4);
    /* calc_l5_direction con objeto (0x90,0xa4) y gato (0x10,0x60): dx>0 -> 1; dy>0 -> 1 */
    CHECK(l5_dat_40ca == 1 && l5_dat_40cb == 1 && l5_dat_40b7 == 1, "direccion: ca=%u cb=%u b7=%u", l5_dat_40ca, l5_dat_40cb, l5_dat_40b7);
    CHECK(l5_dat_40cc == (0x90 - 0x10) + 2 * (0xa4 - 0x60), "distancia: %u", l5_dat_40cc);
    {
        /* dec word [40a6]: el sprite aterrizado (4x17) se dibuja 1 byte antes de donde estaba el perch */
        size_t perch_at = calc_cga_addr(0xa4, 0x90, NULL);
        CHECK(l5_dat_40a6 == (uint16_t)(perch_at - 1), "40a6=%x esperado %zx", l5_dat_40a6, perch_at - 1);
        CHECK(row_is_and(l5_dat_40a6, &ds_pool[0x3f36], 8), "fila 0 del sprite aterrizado (4 words) != sprite AND fondo");
        /* la fila 0x86 del perch ya no esta: el fondo volvio a su estado original */
        size_t top = calc_cga_addr(0x86, 0x90, NULL);
        CHECK(memcmp(&cga_mem[top], &bg[top], 6) == 0, "residuo del perch en la fila 0x86");
    }
    CHECK(n_tone == 0, "el descenso no usa start_tone");

    /* 6) con sonido apagado no toca el PIT, pero baja igual */
    reset(); l5_dat_40b1 = 1; sound_enabled = 0; l5_tick_override = 200; l5_tick_advance = 1;
    update_level5_objects();
    CHECK(l5_dat_40aa == 0xa4 && n_silence == 1 && sil_div == 0, "sin sonido: aa=%x div=%x", l5_dat_40aa, sil_div);

    if (fails) { printf("%d FALLOS\n", fails); return 1; }
    printf("test_level5_objects: OK\n");
    return 0;
}
