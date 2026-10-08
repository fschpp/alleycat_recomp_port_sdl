/* T80 — las ventanas del callejon no se abrian (PC real): update_cat_jump (level_physics.asm L386-547) tenia dos saltos
 * condicionales invertidos y nunca llegaba a animar la apertura (jump_anim_counter se quedaba en 0x1d):
 *   (1) lab_19cd..lab_19e1: `call check_trashcan_near / jnc lab_19e1 / inc [cycle_active] / ret`: CON carry marca cycle_active y
 *       sale; SIN carry sigue con el lanzamiento y el arco. El port hacia lo contrario.
 *   (2) lab_19f0: `sub dx,[jump_toss_tick] / cmp dx,[jump_toss_delay] / jnc lab_1a76`: pasada la pausa del vertice (dx >= delay)
 *       sigue el arco; mientras dura (dx < delay) lanza el proyectil una vez. El port tenia el `<` al reves.
 *   (3) lab_1a54: `mov [gravity_cur_dims],ax` con la tabla gravity_sprite_dims_tbl (02 09 / 02 06 / 02 0c / 02 0c) no se hacia: el
 *       proyectil se guardaba/restauraba con dims 0 y check_dog_collision usaba alto 0.
 * Reloj BIOS simulado con bios_clock_hook. Ver PROGRESS.md §6br. */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "enemy.h"
#include "jump_gravity.h"
#include "bios_clock.h"
#include "sound.h"
#include "gen/enemy_verified_sprites.h"

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
bool audio_init(void) { return false; } void audio_shutdown(void) {}

static uint16_t fake_tick;
static uint16_t hook(void) { return fake_tick; }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* Estado base: gato lejos de la ventana 0 (x=24, y=24), sin proyectil, un tick de `update_cat_jump` por llamada. */
static void base(void) {
    cga_init(); bios_clock_hook = hook; fake_tick = 100;
    cat_x = 0x100; cat_y = 0x48; level_number = 0; difficulty_level = 0; game_mode = 0; force_level7 = 0;
    gravity_y = 0; transitioning = 0; in_level_mode = 0;
    jump_tick_delay = 1; jump_x = 24; jump_y = 24; jump_spawn_param = 8;
    obj_x[0] = obj_x[1] = obj_x[2] = 0x12c;
    jump_anim_counter = 0; cycle_active = 0;
}

/* update_cat_jump solo actua cada 13.a llamada (jump_tick_delay): forzar el tick para ejercitarlo. */
static void step(void) { jump_tick_delay = 1; update_cat_jump(); }

static unsigned cga_sum(void) { unsigned s = 0; for (size_t i = 0; i < CGA_MEM_SIZE; i++) s += cga_mem[i]; return s; }

int main(void) {
    /* A. ventana lejos de los patrulleros (param >= 8): el arco avanza un paso y dibuja una linea. */
    base(); jump_anim_counter = 0x1c;
    unsigned s0 = cga_sum();
    update_cat_jump();
    CHECK(jump_anim_counter == 0x1b, "sin patrullero cerca el contador no avanza (0x%02x, esperado 0x1b)", jump_anim_counter);
    CHECK(cycle_active == 0, "cycle_active=%u sin patrullero cerca", cycle_active);
    (void)s0;

    /* B. patrullero bajo la ventana (param 0 -> obj_x[1]): solo cycle_active=1 y sale, el contador no se toca. */
    base(); jump_anim_counter = 0x1c; jump_spawn_param = 0; obj_x[1] = jump_x;
    update_cat_jump();
    CHECK(jump_anim_counter == 0x1c, "con patrullero cerca el arco no deberia avanzar (0x%02x)", jump_anim_counter);
    CHECK(cycle_active == 1, "con patrullero cerca cycle_active deberia ser 1");
    base(); jump_anim_counter = 0x1c; jump_spawn_param = 4; obj_x[2] = jump_x;      /* bit 2 -> obj_x[2] */
    update_cat_jump();
    CHECK(jump_anim_counter == 0x1c && cycle_active == 1, "param 4 / obj_x[2]: contador 0x%02x cycle_active %u", jump_anim_counter, cycle_active);

    /* C. vertice (contador 0xf): mientras dt < delay lanza UNA vez y espera; con dt >= delay sigue el arco. */
    base(); jump_anim_counter = 0x0f; jump_toss_tick = fake_tick; jump_toss_delay = 5; jump_toss_remaining = 1;
    step();
    CHECK(gravity_y == jump_y, "no lanzo el proyectil en el vertice (gravity_y=%u)", gravity_y);
    CHECK(jump_toss_remaining == 0 && deduct_life == 1, "remaining=%u deduct_life=%u", jump_toss_remaining, deduct_life);
    CHECK(jump_anim_counter == 0x0f, "el contador deberia esperar en 0xf durante la pausa (0x%02x)", jump_anim_counter);
    CHECK(gravity_cur_sprite != NULL, "gravity_cur_sprite sin asignar");
    {
        static const uint16_t dims[4] = {0x0902, 0x0602, 0x0c02, 0x0c02};   /* gravity_sprite_dims_tbl: alto<<8 | ancho en words */
        bool ok = false;
        for (int i = 0; i < 4; i++) if (gravity_cur_sprite == &gravity_sprite[i] && gravity_cur_dims == dims[i]) ok = true;
        CHECK(ok, "gravity_cur_dims = 0x%04x no corresponde al sprite elegido", gravity_cur_dims);
    }
    gravity_y = 0; fake_tick += 2;                                   /* proyectil consumido, dt = 2 < 5: sigue la pausa */
    step();
    CHECK(jump_anim_counter == 0x0f, "durante la pausa (dt<delay) el contador no debe avanzar (0x%02x)", jump_anim_counter);
    CHECK(gravity_y == 0, "no debe lanzar dos veces (remaining=0)");
    fake_tick += 3;                                                  /* dt = 5 >= delay: sigue el arco */
    step();
    CHECK(jump_anim_counter == 0x0e, "pasada la pausa el arco deberia seguir (0x%02x)", jump_anim_counter);

    /* E. entrar al nivel: el gato SALTANDO (in_level_mode == 1) contra la ventana abierta (contador 5..0x18, cat_y < 0x60)
     * marca cat_died (DS 0x551) y game_alley_frame llama a game_death_handler -> select_next_level. */
    base(); jump_anim_counter = 0x10; jump_toss_tick = fake_tick; jump_toss_delay = 5; jump_toss_remaining = 0;
    cat_x = jump_x; cat_y = jump_y; in_level_mode = 1; cat_died = 0;
    step();
    CHECK(cat_died == 1, "saltar a la ventana abierta no marca cat_died");
    base(); jump_anim_counter = 0x1c; cat_x = jump_x; cat_y = jump_y; in_level_mode = 1; cat_died = 0;   /* recien abierta (> 0x18) */
    step();
    CHECK(cat_died == 0, "con el contador > 0x18 la ventana aun no cuenta como entrada");
    base(); jump_anim_counter = 0x10; cat_x = jump_x; cat_y = jump_y; in_level_mode = 0; cat_died = 0;     /* sin saltar */
    step();
    CHECK(cat_died == 0, "sin saltar no deberia entrar");

    /* D. ciclo completo: cada apertura termina (contador 0x1d -> 0) y se vuelve a abrir otra ventana. Simulacion larga. */
    base();
    int completes = 0, tosses = 0, lines = 0; uint8_t prev = 0;
    for (long call = 0; call < 200000; call++) {
        if ((call & 1) == 0) fake_tick++;
        uint8_t before = jump_anim_counter; uint8_t gy = gravity_y;
        update_cat_jump();
        if (before != 0 && jump_anim_counter == 0 && prev) completes++;
        if (gy == 0 && gravity_y != 0) { tosses++; gravity_y = 0; }
        if (before != 0 && before != jump_anim_counter) lines++;
        prev = jump_anim_counter;
    }
    CHECK(completes >= 100, "solo %d aperturas completas en 200000 llamadas (el bug daba 0)", completes);
    CHECK(tosses >= 100, "solo %d lanzamientos", tosses);
    CHECK(lines >= 1000, "solo %d lineas dibujadas", lines);

    if (fails == 0) printf("test_window_open: OK (%d aperturas completas, %d lanzamientos, %d lineas)\n", completes, tosses, lines);
    return fails != 0;
}
