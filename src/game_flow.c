/* game_flow.c — entry.asm L27-143 (T40). Port literal con goto; ver PROGRESS.md §6ak.
 *
 * Mapa ASM -> C (lo que ya existia y lo que no):
 *   clear_screen / render_sprites          alley_drawing.c
 *   setup_alley / setup_level              game_setup.c
 *   init_sound                             enemy.c          (no es de sound.asm)
 *   init_player / reset_jump               fall_object.c
 *   init_objects                           init_cycle_objects (cycle_objects.c)
 *   draw_score / draw_high_score           draw_high_score_display / draw_current_score
 *                                          (nombres cruzados en el original, tareas.md 0.5)
 *   init_music / silence_speaker           sound.c
 *   clear_score / clear_high_score / update_high_score   score.c
 *   set_palette (T44), show_title_screen (T52), show_attract_mode (T54): stubs en flow_stubs.c
 *   detect_video, read_rom_id, install_handlers, init_bios_data: SDL los reemplaza (ver game_hw_init)
 */
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "cga.h"
#include "cat_state.h"
#include "input.h"
#include "game_flow.h"
#include "game_setup.h"
#include "alley_drawing.h"
#include "fall_object.h"
#include "cycle_objects.h"
#include "score.h"
#include "sound.h"
#include "enemy.h"
#include "level7_epilogue.h"
#include "alley_movement.h"
#include "jump_gravity.h"
#include "throw.h"

uint16_t (*game_tick_fn)(void) = NULL;
uint16_t (*pit_counter_fn)(void) = NULL;

static uint16_t game_read_tick(void) {
    if (game_tick_fn) return game_tick_fn();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms * 182 / 10000);          /* 18.2 Hz, como el tick BIOS */
}

void read_pit_counter(void) {
    uint16_t ax;
    if (pit_counter_fn) {
        ax = pit_counter_fn();
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ax = (uint16_t)(ts.tv_nsec / 1000);       /* sustituto del contador del PIT */
    }
    if (ax == 0) ax = 0xfa59;                      /* cmp ax,0 / jnz / mov ax,0xfa59 */
    rng_seed = ax;
}

/* entry L28-L63 (antes de lab_0065): lo que hace cada llamada de hardware y que valor
 * fijo usa el port.
 *   detect_video      -> no-op (SDL crea la ventana CGA 320x200).
 *   read_rom_id       -> rom_id queda en 0xff (PC/XT, no PCjr; cat_state.c).
 *   install_handlers  -> no-op (SDL reemplaza INT 9/8 y el teclado; input.c).
 *   init_bios_data    -> no-op (limpia el buffer de teclas 0x6b7 y keyboard_prev).
 *   pause_screen_addr = keyboard_counter+0x240 -> no modelado: solo lo usa la pausa (T55).
 *   int 0x10 modo 4, luego video_mode = 4 si rom_id==0xfd, si no 6 -> con rom_id=0xff: 6.
 *   int 0x10 ah=0xb bx=0x101 (paleta 1) y out 0x3d9,0x20 (solo no-PCjr) -> no-ops:
 *     video_init() ya deja la paleta CGA 1; set_palette() (T44) es un stub. */
static void game_hw_init(void) {
    video_mode = 0x4;
    diff_icon_idx = 0x0;                 /* difficulty_counter */
    use_joystick = 0x0;
    video_mode = (rom_id == 0xfd) ? 0x4 : 0x6;
    round_counter = 0x0;
    level_number = 0x0;
    set_palette();
    read_pit_counter();
    clear_high_score();
    clear_score();
    attract_shown = 0x0;
    last_level = 0xffff;                 /* no previous level */
    prev_level = 0xffff;
    sound_enabled = 0xff;                /* sound on */
}

void game_flow_run(gf_entry_t from) {
    switch (from) {
    case GF_ENTRY:    goto entry;
    case GF_LAB_0081: goto lab_0081;
    case GF_LAB_00A3: goto lab_00a3;
    case GF_LAB_00AE: goto lab_00ae;
    case GF_LAB_00F3: goto lab_00f3;
    }

entry:
    game_hw_init();

    /* ===================== Title screen / game over ===================== */
lab_0081:
    update_high_score();
    difficulty_level = 0x0;
    level_number = 0x0;
    set_palette();
    silence_speaker();
    show_title_screen();
    silence_speaker();
    if (attract_shown != 0x0) goto lab_00ae;       /* attract ya mostrado -> empezar */

    /* ===================== Attract mode (demo) ===================== */
lab_00a3:
    silence_speaker();
    show_attract_mode();
    attract_shown = 0x1;

    /* ===================== New game setup ===================== */
lab_00ae:
    difficulty_level = diff_icon_idx;              /* mov ax,[difficulty_counter] */
    lives_count = 0x3;                             /* 3 lives */
    clear_score();
    level_number = 0x0;
    set_palette();
    silence_speaker();
    game_timer = 0x0;
    start_tick = game_read_tick();                 /* int 0x1a (dx = parte baja) */
    l7_completion_counter = 0x0;                   /* elapsed_ticks = [0x414] */
    force_level7 = 0x0;
    start_in_level = 0x0;
    show_attract = false;
    restart_game = false;
    silence_speaker();

    /* ===================== Alley setup ===================== */
lab_00f3:
    if (lives_count == 0x0) goto lab_0081;         /* game over? */
    if (restart_game) goto lab_00ae;               /* el jugador pidio reiniciar */
    if (show_attract) goto lab_00a3;               /* timeout -> attract */
    clear_screen();
    render_sprites();
    lives_display = 0xff;                          /* force lives redraw */
    silence_speaker();
    level_number = 0x0;
    if (start_in_level == 0x0) goto lab_0137;      /* returning from a level? */
    setup_level();
    game_mode = 0x2;                               /* level mode */
    anim_counter = 0x1;
    anim_step = 0x20;
    goto lab_0140;
lab_0137:
    cat_x = 0x0;                                   /* start at left edge */
    setup_alley();
lab_0140:
    init_sound();
    init_player();
    reset_jump();
    init_cycle_objects();                          /* init_objects */
    draw_high_score_display();                     /* draw_score (nombres cruzados) */
    draw_current_score();                          /* draw_high_score */
    init_music();
    /* lab_0155: loop del callejon -> T41. */
}

void game_start(void) {
    game_flow_run(GF_ENTRY);
}

/* ===================== T41: entry.asm L144-236 ===================== */

/* level_pool (DS 0x421, 12 bytes) y level_pool_hard (DS 0x42d, 5 bytes utiles; el resto del
 * `db` son ceros que el ASM nunca indexa: dx se rechaza si >= 5). Verificados contra
 * /tmp/data_segment.bin y cat.asm L158-161. Los niveles 2 y 7 no salen de ninguna tabla
 * (el 7 solo por force_level7). */
static const uint8_t level_pool[12]      = { 4, 4, 1, 1, 3, 3, 5, 5, 6, 6, 4, 1 };
static const uint8_t level_pool_hard[5]  = { 1, 3, 4, 5, 6 };

uint16_t select_next_level(void) {
    uint16_t ax, dx, bx;
lab_01e5:
    dx = cga_random();                              /* call random -> dx = rng_seed */
    if ((dx & 0xa0) == 0) goto lab_020a;            /* test dl,0xa0 / jz: usar la tabla "hard" */
    bx = (uint16_t)(difficulty_level & 0x3);
    if (bx == 0x3) goto lab_020a;                   /* dificultad 3 no tiene tabla */
    bx = (uint16_t)(bx << 2);                       /* mov cl,2 / shl bx,cl */
    dx &= 0x3;
    bx = (uint16_t)(bx + dx);
    ax = level_pool[bx];                            /* maximo 2*4+3 = 11 */
    goto lab_021c;
lab_020a:
    dx = cga_random();
    dx &= 0x7;
    if (dx >= 0x5) goto lab_020a;                   /* cmp dx,5 / jnc: rechaza 5,6,7 */
    ax = level_pool_hard[dx];
lab_021c:
    if (ax != last_level) goto lab_022a;            /* cmp ax,[last_level] / jnz (ax,last_level son words) */
    if (ax == prev_level) goto lab_01e5;            /* igual a los dos anteriores -> re-sortear */
lab_022a:
    level_number = (int16_t)ax;
    prev_level = last_level;                        /* shift: prev = last */
    last_level = ax;                                /*        last = new  */
    return ax;
}

void game_death_handler(void) {
    /* lab_01b7 */
    game_tick = game_read_tick();                   /* sub ah,ah / int 0x1a -> dx */
    saved_cat_x = cat_x;                            /* guardar posicion para el respawn */
    saved_cat_y = cat_y;
    start_in_level = 0x1;
    if (force_level7 == 0x0) goto lab_01e5;         /* escena de amor forzada? */
    force_level7 = 0x0;
    level_number = 0x7;
    return;                                         /* jmp lab_0238 (no toca last/prev_level) */
lab_01e5:
    select_next_level();
}

gf_next_t game_alley_frame(void) {
    /* lab_0155 */
    if (lives_count == 0x0) return GF_TO_0081;      /* game over */
    input_process_keys();                           /* lab_015f: process_keyboard */
    if (show_attract) return GF_TO_00A3;            /* timeout -> attract */
    if (restart_game) return GF_TO_00AE;            /* restart */
    /* lab_0176: poll_joystick -> TODO(T60), el joystick no esta portado */
    immune_flag = 0;                                /* heredado del loop de main.c (T19) */
    update_alley_movement();                        /* update_animation (auditoria T70-T73) */
    update_enemies();
    if (enemy_active == 0x0) {
        frame_counter++;                            /* sin enemigo: throttle, fisica solo cada 4.o frame */
        if ((frame_counter & 0x3) != 0) return GF_STAY;
    }
    /* lab_0191 */
    play_sound();
    update_thrown_objects();
    update_cat_jump();
    apply_cat_gravity();
    animate_falling();
    update_cycle_objects();                         /* cycle_animations */
    draw_lives();
    if (cat_died == 0x0) return GF_STAY;            /* sigue vivo -> lab_0155 */
    if (lives_count == 0x0) return GF_TO_0081;      /* game over */
    game_death_handler();                           /* lab_01b7 */
    return GF_TO_0238;
}
