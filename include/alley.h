#ifndef ALLEY_H
#define ALLEY_H

#include <stdint.h>

/* Literal port of src/alley.asm's background-persistence, window-event and
 * death routines. See PROGRESS.md §6f.
 *
 * These four functions are the "dirty rectangle" pipeline the whole alley
 * scene is built on, and they had been inert stubs in four different files
 * since §5f/§5h:
 *
 *   save_cat_background()  — recompute cat_draw_pos from cat_x/cat_y, then
 *   save_alley_buffer()    — copy that rectangle of screen into alley_save_buf
 *   draw_alley_foreground()— AND-blit the cat over it (saving what it covers)
 *   restore_alley_buffer() — put the saved background back, erasing the cat
 *
 * IMPORTANT: buffer_size is NOT a byte count — it is a packed dims word
 * (high byte = height in rows, low byte = width in WORDS), passed straight
 * through as the original's CX to save_from_cga/blit_to_cga. See §6f
 * finding 1. */

void save_alley_buffer(void);
void restore_alley_buffer(void);
void draw_alley_foreground(void);
void save_cat_background(void);

/* spawn_window_event — the "window state machine" that three separate
 * TODOs have been waiting on. It is much smaller than its reputation: on an
 * idle cat it restores the background and blits a random top+bottom window
 * sprite pair at the cat's position, rate-limited to every 8th attempt once
 * a window is already up. */
void spawn_window_event(void);

void enter_building(void);
void handle_cat_death(void);

/* The cat sprite draw_alley_foreground renders. The original keeps a DS
 * offset in cat_sprite_data; this port keeps a real pointer alongside the
 * original's packed dims word, matching the project's "real backing arrays,
 * not DOS scratch-RAM offsets" convention. */
extern const uint8_t *cat_sprite_ptr;
extern uint16_t       cat_sprite_dims;

#endif
