#ifndef ALLEY_H
#define ALLEY_H

#include <stdint.h>
#include "sprite.h"

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

/* update_viewport (alley.asm L1-29, T17) — recorta el sprite de caminata de 3 words x 11 filas
 * cuando el gato entra/sale del borde del callejón. Entradas del original: al = entry_steps,
 * ah = scroll_direction (0xff = derecha); el original recibe el sprite en cat_sprite_data (bx de
 * update_walk_frame), aquí se pasa como `frame`. Efecto: copia (3-al) words x 11 filas (saltando
 * al words a la izquierda si ah != 0xff) al scratch DS 0x000e, deja cat_sprite_ptr = scratch,
 * cat_sprite_dims = 0x0b00|(3-al), cat_sprite_data = 0xe y cat_x = al*8+0x128 (ah==0xff) o 0. */
void update_viewport(uint8_t al, uint8_t ah, const cat_walk_frame_t *frame);

#endif
