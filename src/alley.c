/* alley.c — literal port of src/alley.asm's background-save/restore
 * pipeline, the window-event spawner, and the death animation.
 * See PROGRESS.md §6f. */

#include "alley.h"
#include "cat_state.h"
#include "cga.h"
#include "enemy.h"
#include "sound.h"
#include "gen/ds_pool.h"
#include "gen/death_sprite.h"
#include "gen/cat_gap1_sprites.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

static uint16_t read_bios_tick(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    return (uint16_t)(ms / 55);
}

static uint16_t ds_word(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

/* --- ds_pool offsets (tools/resolve_data_segment.py) ---
 *   window_sprite_top 0x0f92, span 16 = 8 word pointers. The code indexes
 *     it with `and bx,0xe`, i.e. exactly 8 entries. Zero slack.
 *   window_sprite_bot 0x0fa2, span 8 = 4 word pointers, indexed
 *     `and bx,0x6`, i.e. exactly 4 entries. Zero slack.
 * Both tables' extents are therefore confirmed by the masks that read
 * them, not assumed from the label boundary (§3/§7). */
#define DS_WINDOW_SPRITE_TOP 0x0f92
#define DS_WINDOW_SPRITE_BOT 0x0fa2

const uint8_t *cat_sprite_ptr  = NULL;
uint16_t       cat_sprite_dims = 0;

/* window_event_count (DS 0x056d) — the every-8th-attempt rate limiter. */
static uint8_t window_event_count = 0;

/* bp = 0x000e in handle_cat_death: a DS scratch area the original blits the
 * death sprite's covered background into, then blits straight back out of.
 * Modeled as a real array (project convention) sized for the sprite's own
 * 5 words x 18 rows. */
static uint16_t death_save_buf[5 * 0x12];

/* death_draw_pos (DS 0x0581) and anim_tick_delay (DS 0x057f) — only this
 * routine touches them, so they stay local rather than joining cat_state. */
static uint16_t death_draw_pos;
static uint16_t anim_tick_delay;

/* check_dog_collision (enemy.asm) — per §5o the literal name is misleading:
 * this is level-0's gravity-fall landing check, still genuinely unported.
 * Same documented "no collision" stub alley_movement.c has carried. */
static bool check_dog_collision(void) { return false; }

/* --- save_alley_buffer ---
 * `mov cx,[buffer_size] / call save_from_cga`: buffer_size is the CX dims
 * pair, not a byte count (§6f finding 1). */
void save_alley_buffer(void) {
    save_from_cga((uint8_t *)alley_save_buf, cat_draw_pos,
                  (uint8_t)(buffer_size & 0xff), (uint8_t)(buffer_size >> 8));
    sprite_hidden = 0;
}

/* --- restore_alley_buffer --- */
void restore_alley_buffer(void) {
    blit_to_cga((const uint8_t *)alley_save_buf, cat_draw_pos,
                (uint8_t)(buffer_size & 0xff), (uint8_t)(buffer_size >> 8));
}

/* --- draw_alley_foreground ---
 * The AND-blit that draws the cat as a black silhouette (§5b) AND saves the
 * background it covers into alley_save_buf in the same pass — which is why
 * blit_masked takes a mask_save buffer at all, and why buffer_size is set
 * from the sprite's own dims right here. */
void draw_alley_foreground(void) {
    if (cat_sprite_ptr == NULL) return;   /* nothing selected yet this frame */
    buffer_size   = cat_sprite_dims;
    sprite_hidden = 0;
    blit_masked(cat_sprite_ptr, cat_draw_pos,
                (uint8_t)(cat_sprite_dims & 0xff), (uint8_t)(cat_sprite_dims >> 8),
                alley_save_buf);
}

/* --- save_cat_background --- */
void save_cat_background(void) {
    cat_draw_pos = (uint16_t)calc_cga_addr(cat_y, (uint16_t)cat_x, NULL);
    save_alley_buffer();
}

/* --- spawn_window_event ---
 * Draws a random window (a top half at the cat's position and a bottom half
 * exactly 6 rows below it) while the cat is idle.
 *
 * The +0xf0 destination offset for the bottom half is not arbitrary: in
 * CGA's interleaved layout 6 rows is exactly 3 x 80 = 0xf0 bytes, so the
 * single 12-row (0xc02) restore below covers BOTH halves contiguously. That
 * also explains the two mask_save buffers: alley_save_buf (0x5fa) for rows
 * 0-5 and 0x612 = alley_save_buf + 24 bytes = +12 words for rows 6-11 —
 * adjacent with zero gap, which independently confirms the layout. */
void spawn_window_event(void) {
    scroll_speed     = 0x2;
    anim_accumulator = 0x8;

    if (buffer_size == 0xc02) {
        /* a window is already up: only re-roll every 8th attempt */
        window_event_count++;
        if ((window_event_count & 0x7) != 0) return;
    }

    /* lab_1087 */
    restore_alley_buffer();
    if (check_dog_collision()) return;
    if (check_enemy_activate()) return;

    uint16_t bx = (uint16_t)(cga_random() & 0xe);
    uint16_t si = ds_word((uint16_t)(DS_WINDOW_SPRITE_TOP + bx));
    uint16_t di = cat_draw_pos;
    buffer_size = 0xc02;
    blit_masked(&ds_pool[si], di, 0x02, 0x06, alley_save_buf);

    bx = (uint16_t)(cga_random() & 0x6);
    si = ds_word((uint16_t)(DS_WINDOW_SPRITE_BOT + bx));
    di = (uint16_t)(cat_draw_pos + 0xf0);
    blit_masked(&ds_pool[si], di, 0x02, 0x06, alley_save_buf + 12);

    sprite_hidden = 0;
}

/* --- enter_building ---
 * `mov ax,[enter_sprite_data]` loads the FIRST entry of that table, not the
 * table address — so the pose is enter_sprite[0], unlike setup_level's
 * enter_sprite[3] (§5f). */
void enter_building(void) {
    at_platform      = 0;
    in_level_mode    = 1;
    anim_counter     = 2;
    anim_step        = 1;
    anim_accumulator = 0xff;
    scroll_direction = 0;
    transitioning    = 1;
    vert_sprite      = &enter_sprite[0];
    game_mode        = 2;
}

/* --- handle_cat_death ---
 * Draws the falling-death pose, then blocks for 10 BIOS ticks (~550 ms)
 * playing play_falling_sound, then erases it and deducts a life. A genuine
 * one-shot animation, so the blocking wait is faithful (same reasoning as
 * §6c/§6d's cutscenes) — and now that sound.asm is ported (§6e) the falling
 * whistle it drives is actually audible. */
void handle_cat_death(void) {
    uint8_t  dl = cat_y;
    int32_t  cx = (int32_t)cat_x - 0xc;
    if (cx < 0) cx = 0;                 /* `jnc` / `sub cx,cx` */
    if (cx >= 0x10f) cx = 0x10e;

    death_draw_pos = (uint16_t)calc_cga_addr(dl, (uint16_t)cx, NULL);
    blit_transparent(death_sprite_frame.data, death_draw_pos,
                     death_sprite_frame.width_words, death_sprite_frame.height,
                     death_save_buf);

    anim_tick_delay = read_bios_tick();
    fall_sound_x = 0;
    fall_sound_y = 0;
    do {
        play_falling_sound();
    } while ((uint16_t)(read_bios_tick() - anim_tick_delay) < 0xa);
    silence_speaker();

    sprite_hidden = 0;
    blit_to_cga((const uint8_t *)death_save_buf, death_draw_pos,
                death_sprite_frame.width_words, death_sprite_frame.height);

    if (deduct_life != 0 && lives_count != 0) lives_count--;
}
