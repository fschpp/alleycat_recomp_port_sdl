/* wipe.c — enemy.asm animate_screen_wipe (L159-233, T45; PROGRESS.md §6ap).
 * En un .c aparte de transition.c para que los tests puedan envolver animate_screen_wipe con --wrap.
 *
 * Barrido: un rectangulo centrado en el gato (x alineada a 16, y = cat_y+8) que crece 32 px de ancho y
 * 16 filas por paso (16 px y 8 filas por lado) y se rellena con wipe_fill_pattern, hasta tocar los 4 bordes
 * de la pantalla (wipe_edge_flags == 0xf).
 *
 * Decision: BLOQUEANTE, igual que el original (no espera ticks, solo play_wipe_note por paso; en el XT el
 * relleno es lo que tarda). Es una animacion de duracion fija sin entrada del jugador. Como main.c solo
 * presenta el video entre pasadas del loop, se ofrece wipe_step_hook (NULL = nada): main.c lo apunta a
 * video_present + un retardo corto para que el barrido se vea. Sin hook, el efecto es instantaneo. */
#include <stdint.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "sound.h"

void (*wipe_step_hook)(void) = 0;

uint16_t wipe_width;      /* DS 0x1835 (word): ancho del rectangulo en pixeles */
uint16_t wipe_rect_x;     /* DS 0x1832 (word) */
uint8_t  wipe_rect_y;     /* DS 0x1834 */
uint8_t  wipe_height;     /* DS 0x1837 */
uint8_t  wipe_edge_flags; /* DS 0x1838: 1=izq 2=der 4=arriba 8=abajo */

/* calc_cga_addr del ASM: dl*0x28 (+ cga_row_table = 0x1fd8 si dl impar) + (cx>>2). Devuelve el offset en
 * cga_mem (el original puede pasarse de 0x4000 si dl >= 200; aqui se descarta ese relleno, ver abajo). */
static uint16_t wipe_addr(uint8_t dl, uint16_t cx) {
    uint16_t ax = (uint16_t)(dl * 0x28);
    if (dl & 1) ax = (uint16_t)(ax + 0x1fd8);
    return (uint16_t)(ax + (cx >> 2));
}

void animate_screen_wipe(void) {
    uint16_t cx, di, ax;
    uint8_t dl, bl;

    wipe_sound_start();                                  /* bare `ret` en el original */
    wipe_width = 0x1;
    wipe_height = 0x8;
    cx = cat_x;
    dl = cat_y;
    cx = (uint16_t)(cx + 0xc);
    cx &= 0xfff0;
    dl = (uint8_t)(dl + 0x8);
    wipe_edge_flags = 0x0;
lab_1c8c:
    play_wipe_note();
    wipe_rect_x = cx;
    wipe_rect_y = dl;
    di = wipe_addr(dl, cx);
    bl = wipe_height;
lab_1ca0:
    {
        /* rep stosw con cx = wipe_width>>3 palabras (shr cx,1 x3: los `shr cx,0x0` del ASM son `,1`) */
        uint16_t words = (uint16_t)(wipe_width >> 3);
        for (uint16_t i = 0; i < words; i++) {
            /* guarda: el original escribe en B800:di sin comprobar (di < 0x4000 siempre que dl+alto <= 200) */
            if ((uint32_t)di + 1u < CGA_MEM_SIZE) {
                cga_mem[di]     = (uint8_t)(wipe_fill_pattern & 0xff);
                cga_mem[di + 1] = (uint8_t)(wipe_fill_pattern >> 8);
            }
            di = (uint16_t)(di + 2);
        }
        /* mov cx,wipe_width ; shr cx,1 ; shr cx,1 ; and cx,0xfe ; sub di,cx  (= 2*words bytes) */
        di = (uint16_t)(di - ((wipe_width >> 2) & 0xfe));
        di ^= 0x2000;
        if (di & 0x2000) goto lab_1cca;                  /* test di,0x2000 / jnz */
        di = (uint16_t)(di + 0x50);
    }
lab_1cca:
    bl = (uint8_t)(bl - 1);
    if (bl != 0) goto lab_1ca0;
    if (wipe_step_hook) wipe_step_hook();
    if (wipe_edge_flags != 0xf) goto lab_1cd6;
    return;
lab_1cd6:
    wipe_width = (uint16_t)(wipe_width + 0x20);
    wipe_height = (uint8_t)(wipe_height + 0x10);
    cx = wipe_rect_x;
    dl = wipe_rect_y;
    if (cx >= 0x10) { cx = (uint16_t)(cx - 0x10); goto lab_1cf4; }   /* sub cx,0x10 ; jnb */
    cx = 0;
    wipe_edge_flags |= 0x1;
lab_1cf4:
    ax = (uint16_t)(wipe_width + cx);
    if (ax < 0x140) goto lab_1d0b;                       /* jb (sin signo) */
    wipe_width = (uint16_t)(0x140 - cx);
    wipe_edge_flags |= 0x2;
lab_1d0b:
    if (dl >= 0x8) { dl = (uint8_t)(dl - 0x8); goto lab_1d17; }      /* sub dl,8 ; jnb */
    dl = 0;
    wipe_edge_flags |= 0x4;
lab_1d17:
    {
        unsigned sum = (unsigned)wipe_height + dl;       /* add al,dl ; jb (carry de 8 bits) */
        if (sum > 0xff || (uint8_t)sum >= 0xc8) {
            wipe_height = (uint8_t)(0xc8 - dl);
            wipe_edge_flags |= 0x8;
        }
    }
    goto lab_1c8c;
}
