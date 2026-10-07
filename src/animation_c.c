/* animation_c.c — literal port de game_loop.asm L441-560 (T72 C1), PROGRESS.md §6bg.
 *
 * Mapa lab_XXXX -> C:
 *   lab_0bac..lab_0bd2  perro / enemigo activo / cuenta atras de entry_delay      -> aqui
 *   lab_0bd3..lab_0c1b  paso de entrada (entry_steps): update_walk_frame + update_viewport + dibujo -> aqui
 *   lab_0c1c..lab_0c67  at_platform: pose fija 0x9da, vuelta a lab_0e78            -> aqui
 *   lab_0c6a..lab_0c90  in_level_mode == 0 -> lab_0e23; si no update_scroll y rebote -> aqui
 *   lab_0c90..lab_0cc1  anim_accumulator/anim_step -> anim_counter / in_level_mode  -> aqui
 *   lab_0cc1..lab_0ce7  transitioning / transition_timer / check_level_collision    -> aqui
 */
#include "animation_c.h"
#include "alley.h"
#include "cat_state.h"
#include "cga.h"
#include "enemy.h"
#include "level_collision.h"
#include "movement.h"
#include "gen/ds_pool.h"

#include <stdint.h>
#include <stddef.h>

ac_next_t update_animation_c1(void) {
    /* lab_0bac */
    if (check_dog_collision()) return AC_RET;              /* call / jnc lab_0bb2 / lab_0bb1: ret */
    if (enemy_active != 0x0) return AC_RET;                /* lab_0bb2 */
    if (entry_steps == 0x0) goto lab_0c1c;
    if (entry_delay == 0x0) goto lab_0bd3;
    if (enemy_chasing == 0x0) entry_delay--;               /* el perro persiguiendo congela la cuenta atras */
    return AC_RET;                                         /* lab_0bd2 */

lab_0bd3:
    entry_steps--;
    if (entry_steps != 0x0) goto lab_0be5;
    scroll_speed = 0x8;
    update_scroll();
    goto lab_0c1c;

lab_0be5: {
    const cat_walk_frame_t *frame = update_walk_frame();   /* su BX (`mov [cat_sprite_data],bx`) lo pisa update_viewport */
    update_viewport(entry_steps, (uint8_t)scroll_direction, frame);
    if (entry_steps != 0x2) restore_alley_buffer();        /* lab_0c00 */
    if (check_dog_collision()) return AC_RET;              /* jc lab_0c1b */
    if (check_enemy_activate()) return AC_RET;
    cat_draw_pos = (uint16_t)calc_cga_addr(cat_y, (uint16_t)cat_x, NULL);
    draw_alley_foreground();
    return AC_RET;                                         /* lab_0c1b */
}

lab_0c1c:
    if (at_platform < 0x1) goto lab_0c6a;                  /* cmp at_platform,1 / jc */
    if (at_platform != 0x1) goto lab_0c5f;                 /* jnz */
    at_platform++;                                         /* 1 -> 2 */
    scroll_speed = 0x6;
    cat_screen_pos = (uint16_t)calc_cga_addr(cat_y, (uint16_t)cat_x, NULL);
    restore_alley_buffer();
    if (check_dog_collision()) return AC_RET;              /* jc lab_0c66 */
    if (check_enemy_activate()) return AC_RET;
    cat_draw_pos = cat_screen_pos;
    cat_sprite_dims = 0xe03;                               /* 3 words x 0xe filas */
    cat_sprite_data = 0x9da;                               /* inmediato: pose fija de subida/entrada, DS 0x09da */
    cat_sprite_ptr = &ds_pool[0x09da];
    draw_alley_foreground();
lab_0c5f:
    if (input_vertical != 0x0) return AC_L0E78;            /* lab_0c67: jmp near lab_0e78 */
    return AC_RET;                                         /* lab_0c66 */

lab_0c6a:
    if (in_level_mode != 0x0) goto lab_0c74;
    return AC_L0E23;                                       /* jmp near lab_0e23 */

lab_0c74:
    if (!update_scroll()) goto lab_0c90;                   /* jnc: sin choque con el borde */
    scroll_direction = 0x0;
    anim_counter = 0x2;
    in_level_mode = 0x1;
    transition_timer = 0x0;
    goto lab_0cc1;

lab_0c90: {
    uint8_t al = anim_step;
    uint8_t acc = anim_accumulator;
    anim_accumulator = (uint8_t)(acc - al);                /* sub byte [anim_accumulator],al */
    if (acc >= al) goto lab_0cc1;                          /* jnc: sin prestamo */
    if (in_level_mode == 0x1) goto lab_0cb6;
    if (anim_counter <= 0x1) goto lab_0cae;                /* jbe (sin signo) */
    anim_counter--;
    goto lab_0cc1;
lab_0cae:
    in_level_mode = 0x1;
    goto lab_0cc1;
lab_0cb6:
    if (anim_counter >= 0x4) goto lab_0cc1;                /* jnc */
    anim_counter++;
}

lab_0cc1:
    if (transitioning != 0x0) goto lab_0ce7;
    if (transition_timer == 0x0) goto lab_0cd5;
    transition_timer--;
    if (transition_timer != 0x0) goto lab_0ce7;
lab_0cd5:
    if (in_level_mode != 0x1) goto lab_0ce7;
    if (!check_level_collision()) goto lab_0ce7;           /* jnc lab_0ce7 (CF = piso solido) */
    return AC_L0D29;                                       /* mov al,[cat_y_bottom] / jmp lab_0d29 */

lab_0ce7:
    return AC_L0CE7;
}
