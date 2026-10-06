/* T51 — check_special_keys, init_bios_data, detect_video, print_startup_msg. Sin SDL. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "bios_text.h"
#include "font8x8.h"
#include "hardware.h"
#include "ui.h"
#include "gen/ds_pool.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static unsigned pix(int x, int y) {
    size_t off = (size_t)(y & 1) * 0x2000 + (size_t)(y >> 1) * 80 + (size_t)(x >> 2);
    return (cga_mem[off] >> (6 - 2 * (x & 3))) & 3;
}
static int cell_is(int row, int col, int ch, int color) {
    for (int r = 0; r < 8; r++) for (int p = 0; p < 8; p++) {
        unsigned want = (font8x8[ch][r] >> p) & 1 ? (unsigned)color : 0u;
        if (pix(col * 8 + p, row * 8 + r) != want) return 0;
    }
    return 1;
}
static int text_at(int row, int col, const char *s, int color) {
    for (int i = 0; s[i]; i++) if (!cell_is(row, col + i, (unsigned char)s[i], color)) return 0;
    return 1;
}

static void all_up(void) { memset(key_matrix, 0x80, sizeof key_matrix); }
/* pulsadas = mascara de teclas (indices) a 0x00 */
static void down(int idx) { key_matrix[idx] = 0x00; }

int main(void) {
    /* ---- init_bios_data */
    memset(key_matrix, 0x00, sizeof key_matrix); keyboard_counter = 0x0010; keyboard_prev = 0;
    init_bios_data();
    int ok = 1; for (int i = 0; i < KEY_MATRIX_SIZE; i++) ok &= key_matrix[i] == 0x80;
    CHECK(ok, "matriz a 0x80");
    CHECK(keyboard_prev == (uint16_t)(0x0010 - 0x70), "keyboard_prev = contador - 0x70 (%04x)", keyboard_prev);

    /* ---- check_special_keys: sin Ctrl+Alt no hace nada, sea cual sea el resto */
    for (int mask = 0; mask < 4; mask++) {
        if (mask == 3) continue;
        for (int k = 0; k < 4; k++) {
            all_up();
            if (mask & 1) down(KEY_IDX_PAUSE);
            if (mask & 2) down(KEY_IDX_FIRE);
            if (k & 1) down(KEY_IDX_RIGHT);
            if (k & 2) down(KEY_IDX_LEFT);
            down(KEY_IDX_DEL);
            video_mode = 6; crtc_hsync_pos = 0; reboot_requested = false;
            check_special_keys();
            CHECK(video_mode == 6 && crtc_hsync_pos == 0 && !reboot_requested, "sin Ctrl+Alt (mask %d k %d) no actua", mask, k);
        }
    }

    /* ---- Ctrl+Alt+Del: reinicio; tiene prioridad sobre las flechas y no toca video_mode */
    all_up(); down(KEY_IDX_PAUSE); down(KEY_IDX_FIRE); down(KEY_IDX_DEL); down(KEY_IDX_RIGHT);
    video_mode = 6; crtc_hsync_pos = 0; reboot_requested = false;
    check_special_keys();
    CHECK(reboot_requested && video_mode == 6 && crtc_hsync_pos == 0, "Ctrl+Alt+Del reinicia");

    /* ---- Ctrl+Alt+Derecha: video_mode-- hasta 0; CRTC[2] = video_mode + 0x27 */
    all_up(); down(KEY_IDX_PAUSE); down(KEY_IDX_FIRE); down(KEY_IDX_RIGHT);
    video_mode = 6; reboot_requested = false;
    for (int want = 5; want >= 0; want--) {
        check_special_keys();
        CHECK(video_mode == want && crtc_hsync_pos == want + 0x27, "derecha -> %d (crtc %02x)", video_mode, crtc_hsync_pos);
    }
    crtc_hsync_pos = 0xee;
    check_special_keys();
    CHECK(video_mode == 0 && crtc_hsync_pos == 0xee, "derecha en 0: no baja y no escribe CRTC");
    CHECK(!reboot_requested, "derecha no reinicia");

    /* ---- Ctrl+Alt+Izquierda: video_mode++ hasta 7 */
    all_up(); down(KEY_IDX_PAUSE); down(KEY_IDX_FIRE); down(KEY_IDX_LEFT);
    video_mode = 0;
    for (int want = 1; want <= 7; want++) {
        check_special_keys();
        CHECK(video_mode == want && crtc_hsync_pos == want + 0x27, "izquierda -> %d (crtc %02x)", video_mode, crtc_hsync_pos);
    }
    crtc_hsync_pos = 0xee;
    check_special_keys();
    CHECK(video_mode == 7 && crtc_hsync_pos == 0xee, "izquierda en 7: tope y sin CRTC");

    /* ---- derecha e izquierda a la vez: gana derecha (se comprueba primero) */
    all_up(); down(KEY_IDX_PAUSE); down(KEY_IDX_FIRE); down(KEY_IDX_LEFT); down(KEY_IDX_RIGHT);
    video_mode = 4; check_special_keys();
    CHECK(video_mode == 3, "derecha tiene prioridad (%d)", video_mode);
    /* ---- ninguna flecha: nada */
    all_up(); down(KEY_IDX_PAUSE); down(KEY_IDX_FIRE); video_mode = 4; crtc_hsync_pos = 0xee;
    check_special_keys();
    CHECK(video_mode == 4 && crtc_hsync_pos == 0xee, "sin flechas no cambia");
    /* valor inicial de game_hw_init (6) corresponde al 0x2d estandar de CGA */
    CHECK(6 + 0x27 == 0x2d, "6 + 0x27 = 0x2d");

    /* ---- detect_video */
    memset(cga_mem, 0x55, sizeof cga_mem);
    bios_equipment = 0x0020; cga_ram_ok = true; bios_equipment_after = 0xaa;
    CHECK(detect_video() == 0 && bios_equipment_after == 0xaa && pix(0, 0) == 1, "color: no hace nada");
    bios_equipment = 0x0030; bios_set_cursor(0, 0);
    CHECK(detect_video() == 0, "mono + RAM ok");
    CHECK(text_at(0, 0, "Please turn on the color display.", 2), "mensaje 'Please turn on the color display.'");
    CHECK(bios_equipment_after == 0x10, "bits 4-5 -> 01 (%02x)", bios_equipment_after);
    bios_equipment = 0x00f3; detect_video();
    CHECK(bios_equipment_after == 0xd3, "and 0xcf / or 0x10 sobre 0xf3 -> 0xd3 (%02x)", bios_equipment_after);
    memset(cga_mem, 0x55, sizeof cga_mem); bios_equipment = 0x0030; cga_ram_ok = false; bios_set_cursor(5, 0);
    CHECK(detect_video() == 1, "RAM mala: devuelve 1");
    /* 47 caracteres: 40 en la fila 5 y los 7 restantes en la fila 6 (el cursor de la BIOS envuelve) */
    {
        const char *m = "This program requires a color/graphics adapter.";
        char first[41]; memcpy(first, m, 40); first[40] = 0;
        CHECK(text_at(5, 0, first, 2) && text_at(6, 0, m + 40, 2), "mensaje de error (envuelve a 40 columnas)");
    }

    if (fails) { printf("test_hardware: %d FALLOS\n", fails); return 1; }
    printf("test_hardware: OK\n");
    return 0;
}
