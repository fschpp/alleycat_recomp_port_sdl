/* animation_d.c — literal port de game_loop.asm L685-817 (T72/T73), PROGRESS.md §6bi.
 *
 * Mapa lab_XXXX -> C (todo en update_animation_d):
 *   lab_0e23..lab_0e31  nivel 7 salta el limite cat_y; resto: cat_y >= 0xb4 -> lab_0e78
 *   lab_0e31..lab_0e43  check_level_collision: sin suelo -> scroll_direction = 0, modo 1, lab_0eb1
 *   lab_0e43..lab_0e78  nivel 0 con suelo: check_jump_collision (salto forzado del gato golpeado) / jump_hit = 0
 *   lab_0e78..lab_0e91  prev_scroll_dir, scroll_direction = input_horizontal, in_level_mode = input_vertical
 *   lab_0e91..lab_0ec9  modo 1: cat_y < 0xb4 -> lab_0eb1; si no vuelve al callejon (lab_0f34)
 *   lab_0eb1            bajada: ah = 1, al = 0x20, transition_timer = 8, game_mode 1 -> 0
 *   lab_0ec9..lab_0ef1  subida (modo -1): scroll_speed - 2, anim_step = ((speed ^ 0xf) << 4), game_mode 1 -> 2
 *   lab_0ef1..lab_0f33  anim_step/counter/accumulator, at_platform = 0, vert_sprite, l3_platform_id, catch sound (NO dibuja)
 *   lab_0f34..lab_0f45  update_footprint salvo niveles 0 y 7
 *   lab_0f45..lab_0f63  update_scroll, cat_screen_pos, idle -> spawn_window_event
 *   lab_0f63..lab_0f86  update_walk_frame, restore, perro/enemigo, dims 0xb03, dibujo
 */
#include "animation_d.h"
#include "alley.h"
#include "alley_movement.h"
#include "cat_state.h"
#include "cga.h"
#include "enemy.h"
#include "fall_object.h"
#include "input.h"
#include "level_collision.h"
#include "movement.h"
#include "sound.h"
#include "gen/cat_alley_walk_frames.h"
#include "level_objects.h"

#include <stdint.h>
#include <stddef.h>

/* walk_frame_table (DS 0x0f7a) verificada contra el segmento de datos: el orden es el de alley_walk_frames[] */
static const uint16_t walk_frame_ds[12] = { 0x0bd2, 0x0c14, 0x0c98, 0x0c56, 0x0cda, 0x0d1c,
                                            0x0d5e, 0x0da0, 0x0e24, 0x0de2, 0x0e66, 0x0ea8 };

void update_animation_d(ac_next_t from) {
    uint8_t al, ah;

    if (from == AC_L0E78) goto lab_0e78;
    if (from != AC_L0E23) return;

    /* lab_0e23 */
    if (level_number == 0x7) goto lab_0e31;
    if ((uint8_t)cat_y >= 0xb4) goto lab_0e78;             /* cmp / jnc */
lab_0e31:
    if (check_level_collision()) goto lab_0e43;            /* jc: CF = suelo solido */
    scroll_direction = 0x0;
    in_level_mode = 0x1;
    goto lab_0eb1;                                         /* salta el chequeo de cat_y de lab_0e91 */

lab_0e43:
    if (level_number != 0x0) goto lab_0e78;
    if (!check_jump_collision()) {                         /* jc lab_0e56 */
        jump_hit = 0x0;
        goto lab_0e78;
    }
    /* lab_0e56: el proyectil golpea al gato: salto forzado con direccion al azar */
    if (jump_hit == 0x0) play_death_melody();
    input_vertical = 0x1;
    jump_hit = 0x1;
    {
        uint8_t dl = (uint8_t)((uint8_t)cga_random() & 0x1);   /* call random / and dl,1 */
        if (dl == 0x0) dl = 0xff;
        input_horizontal = (int8_t)dl;
    }

lab_0e78:
    prev_scroll_dir = scroll_direction;
    scroll_direction = input_horizontal;
    in_level_mode = input_vertical;
    if (in_level_mode == 0x0) goto lab_0f34;

    /* lab_0e91 */
    if (in_level_mode != 0x1) goto lab_0ec9;
    if ((uint8_t)cat_y < 0xb4) goto lab_0eb1;              /* jc */
    in_level_mode = 0x0;
    auto_walk = 0x0;
    input_vertical = 0x0;
    goto lab_0f34;

lab_0eb1:
    ah = 0x1;
    al = 0x20;
    transition_timer = 0x8;
    if (game_mode == 0x1) game_mode = 0x0;
    goto lab_0ef1;

lab_0ec9: {
    uint16_t ax = scroll_speed;
    uint8_t bl = (uint8_t)ax;
    transition_timer = 0x0;
    al = bl;
    if (al > 0x2) al = (uint8_t)(al - 0x2);                /* cmp al,2 / jbe */
    scroll_speed = (uint16_t)((ax & 0xff00) | al);         /* mov [scroll_speed],ax: ah se conserva */
    ah = 0x8;
    al = (uint8_t)(bl ^ 0xf);                              /* el AL original (antes de restar 2) */
    al = (uint8_t)(al << 4);
    if (game_mode == 0x1) game_mode++;
}

lab_0ef1: {
    uint8_t bl;
    anim_step = al;
    anim_counter = ah;
    anim_accumulator = 0x1;
    at_platform = 0x0;
    bl = (uint8_t)((uint8_t)(scroll_direction + 1) << 1);
    if (in_level_mode != -1) bl = (uint8_t)(bl + 0x6);     /* cmp [in_level_mode],0xff / jz */
    /* climb_sprite_ptrs/dims[bx]: el puntero real vive en vert_sprite (ver cat_state.h) */
    vert_sprite = select_vertical_sprite((uint8_t)(bl >> 1));
    l3_platform_id = 0x0;
    if (door_contact != 0x0) play_catch_sound();
    return;                                                /* lab_0f33: aqui NO se dibuja (lo hace C2 en el siguiente frame) */
}

lab_0f34:
    if (level_number != 0x0 && level_number != 0x7) update_footprint();
    update_scroll();
    cat_screen_pos = (uint16_t)calc_cga_addr(cat_y, (uint16_t)cat_x, NULL);
    if (((uint8_t)scroll_direction | (uint8_t)in_level_mode) == 0x0) {
        spawn_window_event();                              /* gato quieto: solo ventana */
        return;
    }

    /* lab_0f63 */
    {
        const cat_walk_frame_t *frame = update_walk_frame();
        cat_sprite_data = walk_frame_ds[frame - alley_walk_frames];    /* mov [cat_sprite_data],bx: nada lo pisa aqui */
        restore_alley_buffer();
        if (check_dog_collision()) return;                 /* jc lab_0f86 */
        if (check_enemy_activate()) return;
        cat_draw_pos = cat_screen_pos;
        cat_sprite_ptr = frame->data;
        cat_sprite_dims = 0xb03;                           /* inmediato: 3 words x 11 filas */
        draw_alley_foreground();
    }
}
