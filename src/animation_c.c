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
#include "game_setup.h"
#include "sound.h"
#include "sprite.h"
#include "gen/cat_gap1_sprites.h"
#include "gen/cat_alley_walk_frames.h"
#include <stdbool.h>

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

/* ---------------------------------------------------------------------------------------------------------------
 * T72 C2 — game_loop.asm L561-684 (lab_0ce7 .. lab_0e1f), PROGRESS.md §6bh.
 *
 *   lab_0ce7..lab_0d06  in_level_mode != 1: al = cat_y_bottom - anim_counter (borrow -> al=0, modo 1, counter 1)
 *   lab_0d06..lab_0d22  modo 1: al += anim_counter (8 bits, con vuelta); <=0xe6 ok; nivel 7: >=0xf8 -> cat_died
 *   lab_0d22..lab_0d4f  tope 0xe6: game_mode = 0; lab_0d29: fin de transicion (+ crash si at_platform)
 *   lab_0d4f..lab_0d73  cat_y_bottom/cat_y (-0x32, saturado a 0), cat_screen_pos, restore_alley_buffer
 *   lab_0d73..lab_0dc4  check_dog_collision / check_enemy_activate -> sprite_hidden = 1, ret
 *   lab_0d86..lab_0dac  sprite: auto_walk -> recoil[recoil_frame & 0xe]; si no vert_sprite
 *   lab_0dac..lab_0dca  recorte por arriba si cat_y_bottom < 0x32; lab_0dde..lab_0e16 recorte por abajo
 *   lab_0e1f            draw_alley_foreground
 * ------------------------------------------------------------------------------------------------------------- */
/* recoil_sprite_ptrs (DS 0x0fc2): verificados contra el segmento de datos resuelto. Las dims (0x0fd2) son
 * (alto << 8) | 3 y salen del propio cat_walk_frame_t (0xd03,0xc03,0xb03,0xa03,0xd03,0xa03,0xb03,0xc03). */
static const uint16_t recoil_ds_ptr[8] = { 0x0a2e, 0x0aca, 0x0bd2, 0x0b5a, 0x0a7c, 0x0b96, 0x0d5e, 0x0b12 };

static const cat_walk_frame_t *recoil_frame_at(unsigned idx) {
    /* los indices 2 y 6 reutilizan frames del ciclo del callejon (alley_walk_frames[0] = 0x0bd2, [6] = 0x0d5e) */
    if (recoil_sprite[idx].data == NULL) return &alley_walk_frames[recoil_pool_b_frame_index[idx]];
    return &recoil_sprite[idx];
}

void update_animation_c2(ac_next_t from) {
    uint8_t al, bl, bh;
    uint16_t ax_data = 0;                      /* `ax` de lab_0da8 como offset DS, cuando se conoce */
    const uint8_t *ptr;

    if (from != AC_L0CE7 && from != AC_L0D29) return;
    al = cat_y_bottom;
    if (from == AC_L0D29) goto lab_0d29;

    /* lab_0ce7 */
    if (in_level_mode == 0x1) goto lab_0d06;
    {
        uint8_t sub = anim_counter;
        bool borrow = al < sub;
        al = (uint8_t)(al - sub);
        if (!borrow) goto lab_0d4f;                        /* jnc */
        al = 0;                                            /* db 0x2a,0xc0: sub al,al */
        in_level_mode = 0x1;
        anim_counter = 0x1;
        goto lab_0d4f;
    }

lab_0d06:
    al = (uint8_t)(al + anim_counter);                     /* add al,[anim_counter]: sin acarreo a 16 bits */
    if (al <= 0xe6) goto lab_0d4f;                         /* cmp al,0xe6 / jbe */
    if (level_number != 0x7) goto lab_0d22;
    if (al < 0xf8) goto lab_0d4f;                          /* cmp al,0xf8 / jc */
    al = 0xf8;
    cat_died = 0x1;
    goto lab_0d4f;

lab_0d22:
    al = 0xe6;
    game_mode = 0x0;

lab_0d29:
    in_level_mode = 0x0;
    auto_walk = 0x0;
    scroll_speed = 0x2;
    transition_timer = 0x0;
    transitioning = 0x0;
    if (at_platform != 0x0) play_crash_sound();            /* push ax / call / pop ax: al se conserva */

lab_0d4f:
    cat_y_bottom = al;
    cat_y = (al < 0x32) ? 0 : (uint8_t)(al - 0x32);        /* sub al,0x32 / jnc / sub al,al */
    cat_screen_pos = (uint16_t)calc_cga_addr(cat_y, (uint16_t)cat_x, NULL);
    if (sprite_hidden == 0x0) restore_alley_buffer();
    if (check_dog_collision()) goto lab_0dc4;
    if (check_enemy_activate()) goto lab_0dc4;
    cat_draw_pos = cat_screen_pos;

    if (auto_walk != 0x0) {
        const cat_walk_frame_t *r;
        recoil_frame = (uint16_t)(recoil_frame + 2);
        r = recoil_frame_at((recoil_frame & 0xe) >> 1);
        ptr = r->data;
        ax_data = recoil_ds_ptr[(recoil_frame & 0xe) >> 1];
        bl = r->width_words;
        bh = r->height;
    } else {
        ptr = (vert_sprite != NULL) ? vert_sprite->data : NULL;
        bl = (vert_sprite != NULL) ? vert_sprite->width_words : 0;
        bh = (vert_sprite != NULL) ? vert_sprite->height : 0;
    }
    /* lab_0da8 */
    cat_sprite_ptr = ptr;
    if (auto_walk != 0x0) cat_sprite_data = ax_data;       /* con vert_sprite el offset DS no existe en el port (puntero real) */
    cat_sprite_dims = (uint16_t)((bh << 8) | bl);

    al = (uint8_t)(0x32 - cat_y_bottom);
    if (cat_y_bottom >= 0x32) goto lab_0dde;               /* jz / jc */
    /* El ASM gasta 0x168 vueltas de `loop` aqui: retardo puro, sin efecto en el port. */
    {
        bool cf_or_zero = (bh <= al);                      /* sub bh,al / jz lab_0dc4 / jnc lab_0dca */
        if (cf_or_zero) goto lab_0dc4;
        bh = (uint8_t)(bh - al);
        cat_sprite_dims = (uint16_t)((bh << 8) | bl);
        /* mul ah con ah = 2*bl: filas recortadas * bytes por fila. El ASM suma el offset a vert_sprite_data AUNQUE el
         * sprite elegido sea el de retroceso (auto_walk); se conserva esa particularidad del original. */
        {
            const uint8_t *base = (vert_sprite != NULL) ? vert_sprite->data : NULL;
            cat_sprite_ptr = (base != NULL) ? base + (unsigned)al * (2u * bl) : NULL;
        }
        goto lab_0e1f;
    }

lab_0dde:
    if (level_number != 0x7) goto lab_0dee;
    if (cat_y < 0xbb) goto lab_0e1f;                       /* sub al,0xbb / jc */
    al = (uint8_t)(cat_y - 0xbb);
    goto lab_0dfc;

lab_0dee:
    if (game_mode != 0x2) goto lab_0e1f;
    if (cat_y < 0x5e) goto lab_0e1f;
    al = (uint8_t)(cat_y - 0x5e);

lab_0dfc:
    if (bh <= al) goto lab_0e02;                           /* sub bh,al: cero o prestamo -> lab_0e02 */
    bh = (uint8_t)(bh - al);                               /* lab_0e16 */
    cat_sprite_dims = (uint16_t)((bh << 8) | bl);
    anim_counter = 0x2;
    goto lab_0e1f;

lab_0e02:
    if (level_number != 0x7) goto lab_0e0f;
    cat_died = 0x1;
    return;
lab_0e0f:
    setup_alley();
    play_hiss_sound();
    return;

lab_0dc4:
    sprite_hidden = 0x1;
    return;

lab_0e1f:
    draw_alley_foreground();
}
