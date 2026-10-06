/* ui.c — helpers de texto y espera de ui.asm (T50). Ver include/ui.h y PROGRESS.md §6au. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "ui.h"
#include "cat_state.h"
#include "cga.h"
#include "bios_text.h"
#include "gen/ds_pool.h"

uint16_t keyboard_counter;                 /* DS 0x0693 */
uint16_t title_joy_offset;                 /* DS 0x6d8f */
void (*ui_wait_hook)(void);
uint8_t (*joy_port_fn)(void);

#define DS_TEXT_PTRS  0x6d37               /* attract_icon_sprite_a: punteros a cadenas (word) */
#define DS_TEXT_CURS  0x6d63               /* attract_icon_sprite_b: cursor (word, dh = fila) */

static uint16_t ui_ds_word(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

/* print_string (L203-214): lodsb / cmp al,0 / jz fin / teletype (bl=2, ah=0xe) / loop. */
const uint8_t *print_string(const uint8_t *si) {
lab_5e2d:
    {
        uint8_t al = *si++;                                /* lodsb */
        if (al == 0x0) goto lab_5e3a;                      /* cmp al,0 / jz */
        bios_teletype(al, 0x2);                            /* mov bl,2 / mov ah,0xe / int 0x10 */
    }
    goto lab_5e2d;
lab_5e3a:
    return si;
}

/* set_cursor (L237-242): dl=0, bh=dl=0, ah=2, int 0x10: cursor a (dh, 0). */
void set_cursor(uint8_t dh) {
    bios_set_cursor(dh, 0x0);
}

/* wait_for_input (L383-397). Con joystick: repite `in al,0x201 / and al,0x10 / jnz` hasta que el bit 4 sea 0.
 * Sin joystick: toma keyboard_counter y espera a que cambie (la ISR de INT 9 lo incrementa). El port llama a
 * ui_wait_hook en cada vuelta para que main.c bombee SDL (si no, la ventana se congelaria). */
void wait_for_input(void) {
    uint16_t ax;
    if (use_joystick == 0x0) goto lab_5fa7;                /* cmp byte [use_joystick],0 / jz */
lab_5f9e:
    if (ui_wait_hook) ui_wait_hook();
    {
        uint8_t al = joy_port_fn ? joy_port_fn() : 0xff;   /* mov dx,0x201 / in al,dx */
        if ((al & 0x10) != 0) goto lab_5f9e;               /* and al,0x10 / jnz */
    }
    return;
lab_5fa7:
    ax = keyboard_counter;                                 /* mov ax,[keyboard_counter] */
lab_5faa:
    if (ui_wait_hook) ui_wait_hook();
    if (ax == keyboard_counter) goto lab_5faa;             /* cmp ax,[keyboard_counter] / jz */
}

/* display_text_line (L405-413): dx = word [title_joy_offset + attract_icon_sprite_b] (set_cursor usa dh = fila),
 * si = word [bx + attract_icon_sprite_a]; title_joy_offset += 2 entre medias; print_string. */
void display_text_line(void) {
    uint16_t bx = title_joy_offset;
    uint16_t dx = ui_ds_word((uint16_t)(bx + DS_TEXT_CURS));
    set_cursor((uint8_t)(dx >> 8));                        /* dl se pone a 0 dentro de set_cursor */
    bx = title_joy_offset;
    title_joy_offset = (uint16_t)(title_joy_offset + 0x2);
    print_string(&ds_pool[ui_ds_word((uint16_t)(bx + DS_TEXT_PTRS))]);
}

/* clear_cga (L418-429): rep stosw 0xfa0 words (8000 bytes) en 0xb800:0 y en 0xb800:0x2000. */
void clear_cga(void) {
    memset(&cga_mem[0], 0, 0xfa0 * 2);
    memset(&cga_mem[0x2000], 0, 0xfa0 * 2);
}
