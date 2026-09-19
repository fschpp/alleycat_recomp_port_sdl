#include "cat_state.h"
#include "sound.h"
#include "cga.h"
#include "level_collision.h"
#include "game_setup.h"
#include "enemy.h"
#include "gen/enemy_sprites_ex.h"
#include <stdint.h>
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
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms / 55); /* ~18.2 ticks/sec */
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
}

/* update_enemy_viewport — literal port of the addressing/positioning
 * half. NOT ported (documented simplification, see PROGRESS.md §5p): the
 * original also adds the `al` parameter (a countdown value, e.g.
 * enemy_approach_timer) as a byte offset directly into the sprite
 * bitmap data before a partial copy_with_stride — a "partial reveal"
 * effect where the dog appears to slide/emerge gradually during its
 * approach animation. This port always shows the full, uncropped frame
 * for approach/exit states instead of that partial-reveal nuance. */
static void update_enemy_viewport(uint8_t al, int8_t ah_dir_flag) {
    /* original subtracts al from the width field here for the
     * partial-reveal crop effect — not ported (see above), so dims
     * always stay the full frame size to match what draw_enemy actually
     * draws. al is still used below for enemy_x positioning, which IS
     * ported faithfully. */
    enemy_sprite_dims = 0x0f04;
    if (ah_dir_flag == -1) {
        uint16_t ax = (uint16_t)(al << 3);
        ax = (uint16_t)(ax + 0x120);
        enemy_x = ax;
    } else {
        enemy_x = 0;
    }
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
static void activate_enemy_chase(void) {
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
    /* original also checks byte [0x558] (an unnamed flag — likely
     * "cat is mid-jump/airborne" or similar; not yet identified/ported,
     * treated as always 0/false here, matching this port's existing
     * simplifications elsewhere for unidentified single-byte flags). */

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
 * - byte [0x558] (an unidentified single-byte flag checked in several
 *   spawn/chase gates) is treated as always 0, consistent with this
 *   port's existing handling of other not-yet-identified flags. */
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
    /* flag [0x558] == 0 always */
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
    /* flag [0x558] == 0 always */
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
    /* flag [0x558] == 0 -> jz taken -> lab_1fef */
    goto lab_1fef;

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
