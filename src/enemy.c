#include "bios_clock.h"
#include "cat_state.h"
#include "sound.h"
#include "cga.h"
#include "level_collision.h"
#include "game_setup.h"
#include "enemy.h"
#include "alley.h"
#include "jump_gravity.h"
#include "gen/enemy_sprites_ex.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/* random() substitute — original calls a shared 16-bit LFSR (cga_random,
 * already ported in cga.c) and uses its low byte. Matches every other
 * "random()" call site in this port's convention. */
static uint8_t enemy_random_byte(void) {
    return (uint8_t)(cga_random() & 0xFF);
}

/* BIOS `int 0x1a` tick substitute — the original gates update_enemies to
 * roughly the real hardware's 18.2Hz timer tick (~54.9ms/tick). We
 * substitute a host monotonic clock read converted to the same units,
 * matching the pattern already used for cga.c's RNG seeding. */
static uint16_t read_bios_tick(void) {
    return bios_clock_read();
}

/* init_sound is NOT a sound.asm function despite the name — it lives in
 * enemy.asm (line 359) and is the enemy-state reset, which happens to end
 * by calling sound.asm's init_chase_sound. Ported for real here now that
 * init_chase_sound exists. silence_speaker comes from src/sound.c. */
void init_sound(void) {
    enemy_chasing        = 0;
    enemy_tick_counter   = 0;
    enemy_approach_timer = 0;
    enemy_exit_timer     = 0;
    enemy_active         = 0;
    enemy_y_pos          = 0xb1;
    init_chase_sound();
}

/* T76: el ASM copia el recorte a un scratch en DS (0xe) y pone enemy_sprite_ptr = 0xe
 * (enemy.asm L564-593). Aqui enemy_sprite_ptr sigue siendo el indice del frame, asi que el
 * scratch es propio y enemy_cropped dice que draw_enemy debe leerlo en lugar del frame
 * completo. update_enemy_sprite lo apaga (reasigna enemy_sprite_ptr, como el ASM). */
static uint8_t enemy_crop_buf[4 * 2 * 15];
static bool    enemy_cropped = false;

/* update_enemy_sprite — literal port. See PROGRESS.md §5o: this confirms
 * enemy_sprite_table's full intended layout (already partially verified
 * in §5i/§5k) — indices 0-3 (word-offsets 0,2,4,6) are the 2-frame×2-
 * direction walk cycle; indices 4-7 (word-offsets 8,10,12,14) are a
 * 4-frame, direction-independent "caught the cat" celebration animation. */
static void update_enemy_sprite(void) {
    uint8_t bl;
    if (enemy_active != 0) {
        enemy_anim_frame++;
        bl = (uint8_t)((enemy_anim_frame & 0x6) | 0x8);
    } else {
        enemy_anim_frame = (uint8_t)(enemy_anim_frame + 2);
        bl = (uint8_t)(enemy_anim_frame & 0x2);
        if (enemy_dir == 1) bl |= 0x4;
    }
    /* enemy_sprite_table[bl/2] would give the real DS pointer in the
     * original; we don't have enemy bitmap data extracted yet (§5i noted
     * this as future work), so enemy_sprite_ptr is kept as the raw word-
     * index for now rather than a real pointer — nothing currently reads
     * pixels through it. */
    enemy_sprite_ptr = bl;
    enemy_cropped = false;
}

/* update_enemy_viewport — port literal de enemy.asm L564-593 (T76, PROGRESS.md §6bn; antes §5p
 * lo simplificaba a "frame completo"). Revela el perro de forma gradual:
 *   cx = 0x0f04 - al        ancho (words) = 4 - al, alto 15 filas
 *   ah == 0xff (entra por la derecha): enemy_x = 0x120 + al*8, el recorte empieza en el borde
 *                           izquierdo del sprite (se ve su parte delantera);
 *   si no (entra por la izquierda): enemy_sprite_ptr += al*2 (se saltan `al` words de cada fila)
 *                           y enemy_x = 0 (se ve su parte trasera).
 * Luego copy_with_stride(si=sprite, di=0xe, ancho, 15 filas, al=4): filas de origen de 8 bytes
 * (4 words), destino compacto de (4-al)*2 bytes por fila. */
static void update_enemy_viewport(uint8_t al, int8_t ah_dir_flag) {
    uint8_t cl = (uint8_t)(0x04 - al);
    enemy_sprite_dims = (uint16_t)(0x0f00 | cl);
    size_t skip_bytes = 0;
    if (ah_dir_flag == -1) {
        enemy_x = (uint16_t)(0x120 + (uint16_t)(al << 3));
    } else {
        skip_bytes = (size_t)al * 2u;
        enemy_x = 0;
    }
    uint8_t idx = (uint8_t)(enemy_sprite_ptr / 2);
    if (idx > 7) idx = 7;
    const cat_walk_frame_t *frame = &enemy_sprite_frames[idx];
    size_t row_bytes = (size_t)cl * 2u;
    for (size_t row = 0; row < 15 && row_bytes != 0; row++) {
        memcpy(&enemy_crop_buf[row * row_bytes], frame->data + row * 8u + skip_bytes, row_bytes);
    }
    enemy_cropped = true;
}

/* draw_enemy — the enemy_active==0 (normal walk/chase) path is ported
 * for real, using the now-fixed blit_transparent (with its restored
 * background-save side effect, §5p) and the extracted enemy sprite
 * bitmaps (§5p). The enemy_active!=0 ("caught the cat") path is NOT
 * ported — it saves/overwrites a hardcoded screen region ([es:0x1cbd])
 * not yet identified in this port, and is a fairly rare celebratory
 * state; documented rather than guessed. */
static void draw_enemy(void) {
    enemy_erase_dims = enemy_sprite_dims;
    if (enemy_active != 0) {
        /* TODO: "caught the cat" draw path — needs [0x1cbd] identified */
        return;
    }
    if (enemy_cropped) {
        blit_transparent(enemy_crop_buf, enemy_draw_addr, (uint8_t)(enemy_sprite_dims & 0xff),
                         (uint8_t)(enemy_sprite_dims >> 8), enemy_save_buf);
        return;
    }
    uint8_t idx = (uint8_t)(enemy_sprite_ptr / 2);
    if (idx > 7) idx = 7;
    const cat_walk_frame_t *frame = &enemy_sprite_frames[idx];
    blit_transparent(frame->data, enemy_draw_addr, frame->width_words, frame->height,
                      enemy_save_buf);
}

static void erase_enemy(void) {
    if (enemy_erase_dims == 0) return;
    uint8_t width_words = (uint8_t)(enemy_erase_dims & 0xff);
    uint8_t height = (uint8_t)(enemy_erase_dims >> 8);
    blit_to_cga((const uint8_t *)enemy_save_buf, enemy_draw_addr, width_words, height);
}

/* check_enemy_object_hit — literal port. Tests a thrown object against
 * the enemy's current hitbox using the already-verified
 * check_rect_collision. */
bool check_enemy_object_hit(void) {
    if (enemy_chasing == 0 && enemy_approach_timer == 0 && enemy_exit_timer == 0) {
        return false;
    }
    return check_rect_collision(thrown_obj_x, thrown_obj_y, 0x10, 0x1e,
                                 enemy_x, enemy_y_pos, 0x20, 0xf);
}

/* activate_enemy_chase — literal port. */
void activate_enemy_chase(void) {
    if (level_number == 6) {
        enemy_y_pos = cat_y;
        enemy_x = (uint16_t)cat_x;
    }

    uint16_t ax = (uint16_t)((enemy_x + (uint16_t)cat_x) >> 1);
    if (ax >= 0x118) ax = 0x117;
    enemy_x = ax;

    uint8_t bl;
    uint16_t dx;
    if (ax > 0xa0) {
        bl = 1;
        dx = (uint16_t)(ax - 0x9f);
    } else {
        bl = 0xff;
        dx = (uint16_t)(0xa1 - ax);
    }

    enemy_active = bl;
    enemy_chasing = 1;
    enemy_exit_timer = 0;
    enemy_chase_delay = (uint16_t)(dx >> 3);

    if (level_number == 6) {
        /* original does a special immediate draw here (with enemy_active
         * temporarily cleared) to show the enemy at its snapped level-6
         * position right away; skipped — draw_enemy is a stub for now. */
    }

    /* erase_enemy(); restore_alley_buffer(); -- both currently no-ops/stubs */

    uint16_t new_cat_x = (cat_x >= 0xa0) ? 0x122 : 0;
    cat_x = (int16_t)new_cat_x;

    if (level_number == 0) {
        setup_alley();
    }
}

/* check_enemy_activate — literal port. */
bool check_enemy_activate(void) {
    if (enemy_active != 0) return false;
    if (enemy_chasing == 0 && enemy_approach_timer == 0 && enemy_exit_timer == 0) return false;
    if ((uint8_t)cat_y < 0xa3) return false;
    if (entry_steps != 0) return false;           /* cmp byte [0x558],0 / jnz lab_2134: [0x558] = entry_steps (T77) */

    uint16_t ax = (uint16_t)(enemy_x + 0x20);
    if (ax < (uint16_t)cat_x) return false;
    ax = (uint16_t)((ax >= 0x38) ? (ax - 0x38) : 0);
    if (ax > (uint16_t)cat_x) return false;

    activate_enemy_chase();
    return true;
}

/* update_enemies — literal, label-for-label port of the main per-tick dog
 * AI dispatcher (game_loop.asm ~lab_1e7a through lab_2021). Rewritten
 * from scratch as a strict goto translation after a first attempt
 * (which tried to "understand and restructure" the flow) turned out to
 * conflate two different branches and miss one entirely — see
 * PROGRESS.md §5o for the full account. Every label below corresponds
 * 1:1 to the original's.
 *
 * Known simplifications:
 * - `int 0x1a` (BIOS tick) substituted with a host clock read
 *   (read_bios_tick), same pattern as cga.c's RNG seeding.
 * - `check_vsync` (busy-wait for vertical retrace) has no meaningful
 *   equivalent in this port's render loop, which is already externally
 *   paced by SDL_Delay — treated as always "ready."
 * - byte [0x558] es `entry_steps` (DS 0x558, contador del paso de entrada de update_animation);
 *   T77 lo cablea en los 4 sitios donde el ASM lo lee por direccion cruda (spawn, persecucion,
 *   salida y check_enemy_activate): mientras el gato esta entrando el perro no aparece/ataca. */
void update_enemies(void) {
    uint16_t ax = 0; /* shared across the movement-application tail
                       * (lab_1fab through lab_1fef), matching how the
                       * original just keeps reusing the AX register
                       * across those jumps — C's goto can't pass
                       * parameters, so this is hoisted to function scope
                       * instead. */
    uint16_t tick_now = read_bios_tick();
    uint16_t dx = (uint16_t)(tick_now - enemy_last_tick);
    uint16_t threshold = (uint16_t)((enemy_tick_counter & 1) + 1);
    if (dx < threshold) return; /* jnc NOT taken: dx < threshold -> lab_1e7a (return) */

    /* lab_1e7b: check_vsync always "ready" in this port */
    enemy_last_tick = tick_now;
    enemy_tick_counter++;

    if (enemy_exit_timer == 0) goto lab_1ee2;
    enemy_exit_timer--;
    if (enemy_exit_timer != 0) goto lab_1ec9;

    silence_speaker();
    if (enemy_active == 0) goto lab_1ec2;
    if (level_number == 0) goto lab_1eb7;
    object_hit = 0xdd;
    cat_x = 0xa0;
    cat_y = 0x60;
    return;

lab_1eb7:
    if (lives_count == 0) goto lab_1ec2;
    lives_count--;

lab_1ec2:
    erase_enemy();
    init_sound();
    return;

lab_1ec9: {
    update_enemy_sprite();
    uint8_t al = (uint8_t)(0x04 - enemy_exit_timer);
    int8_t ah = (enemy_dir == -1) ? 1 : -1;
    update_enemy_viewport(al, ah);
    goto lab_1ffb;
}

lab_1ee2:
    if (enemy_active == 0) goto lab_1f0c;
    {
        uint8_t dl = (uint8_t)enemy_active;
        if (enemy_chase_delay != 0) {
            enemy_chase_delay--;
            uint8_t r = enemy_random_byte();
            dl = (uint8_t)(r & 0x1);
            if (dl == 0) dl = 0xff;
        }
        enemy_dir = (int8_t)dl;
    }
    ax = enemy_x;
    goto lab_1fab;

lab_1f0c:
    if (enemy_chasing != 0) goto lab_1f75;
    if (enemy_approach_timer != 0) goto lab_1f57;
    if (fall_hit != 0) goto lab_1f3d;
    if ((uint8_t)cat_y < 0xb4) goto lab_1f3c; /* jc */
    if (entry_steps != 0) goto lab_1f3c;          /* cmp byte [0x558],0 / jnz lab_1f3c */
    {
        uint8_t r = enemy_random_byte();
        if (r < enemy_spawn_chance[difficulty_level & 7]) goto lab_1f3d; /* jc */
    }
lab_1f3c:
    return;

lab_1f3d: {
    uint8_t al = 1;
    walk_note_index = 0;
    if ((uint16_t)cat_x < 0xa0) al = 0xff;
    enemy_dir = (int8_t)al;
    enemy_approach_timer = 4;
}
lab_1f57:
    enemy_approach_timer--;
    if (enemy_approach_timer != 0) goto lab_1f65;
    enemy_chasing = 1;
    goto lab_1f75;

lab_1f65:
    update_enemy_sprite();
    update_enemy_viewport(enemy_approach_timer, enemy_dir);
    goto lab_1ffb;

lab_1f75:
    fall_hit = 0;
    ax = enemy_x;
    if ((uint8_t)cat_y < 0xb4) goto lab_1fab;
    if (entry_steps != 0) goto lab_1fab;          /* cmp byte [0x558],0 / jnz lab_1fab */
    {
        uint8_t r = enemy_random_byte();
        if (r > enemy_chase_chance[difficulty_level & 7]) goto lab_1fab; /* ja */
        if (ax > (uint16_t)cat_x) { enemy_dir = -1; goto lab_1fab; }
        enemy_dir = 1;
    }
    goto lab_1fab;

lab_1fab: {
    uint8_t dir_byte = (uint8_t)enemy_dir;
    if (dir_byte < 1) goto lab_1fef;      /* unsigned: only enemy_dir==0 */
    if (dir_byte == 1) goto lab_1fe2;
    /* enemy_dir == 0xff (-1) */
    if (ax >= 8) { ax = (uint16_t)(ax - 8); goto lab_1fef; }
    ax = 0;
    goto lab_1fbb;
}

lab_1fbb:
    if (enemy_active == 0) goto lab_1fcc;
    if (enemy_chase_delay != 0) goto lab_1fef;
    goto lab_1fda;

lab_1fcc:
    if ((uint8_t)cat_y < 0xb4) goto lab_1fda;
    if (entry_steps == 0) goto lab_1fef;          /* cmp byte [0x558],0 / jz lab_1fef; si no, cae a lab_1fda */

lab_1fda:
    enemy_exit_timer = 4;
    goto lab_1fef;

lab_1fe2:
    ax = (uint16_t)(ax + 8);
    if (ax < 0x11e) goto lab_1fef;
    ax = 0x11e;
    goto lab_1fbb;

lab_1fef:
    enemy_x = ax;
    update_enemy_sprite();
    enemy_sprite_dims = 0xf04;

lab_1ffb:
    enemy_next_addr = (uint16_t)calc_cga_addr(enemy_y_pos, enemy_x, NULL);
    if (enemy_approach_timer == 3) goto lab_2013;
    erase_enemy();
lab_2013:
    if (check_enemy_activate()) goto lab_2021; /* jc */
    enemy_draw_addr = enemy_next_addr;
    draw_enemy();
lab_2021:
    return;
}

/* check_dog_collision — literal port of enemy.asm L69-107 (T18, PROGRESS.md §6o). Pese al nombre
 * es la comprobación de aterrizaje del proyectil (gravity_*) sobre el gato del nivel 0 (§5o).
 * Devuelve el carry del original: true si el proyectil ya está sobre el gato (o ya estaba
 * capturado, dog_catch_flag != 0), false si no aplica o no hay solape. */
bool check_dog_collision(void) {
    if (level_number != 0x0) return false;              /* lab_1be1: clc */
    if (gravity_y == 0x0) return false;
    /* ax=gravity_x, dl=gravity_y, si=0x10, cl=alto (byte alto de gravity_cur_dims tras
     * `xchg ch,cl`; ch se pisa con 0xe); bx=cat_x, dh=cat_y, di=0x18, ch=0xe. */
    bool hit = check_rect_collision((int16_t)gravity_x, gravity_y, 0x10,
                                    (uint8_t)(gravity_cur_dims >> 8),
                                    (uint16_t)cat_x, cat_y, 0x18, 0xe);
    if (!hit) return false;                             /* jnc lab_1be2 (clc) */
    restore_alley_buffer();
    restore_gravity_bg();
    enter_building();
    if (dog_catch_flag == 0x0) {
        dog_catch_flag = 0x1;
        handle_cat_death();
        gravity_drift_dir = (gravity_drift_dir == 0xff) ? 0x1 : 0xff;
        gravity_h_speed = 0x60;
        gravity_frame = 0x1;
        at_platform = 0x0;
    }
    return true;                                        /* lab_1bdf: stc */
}
