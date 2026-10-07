#ifndef PALETTE_H
#define PALETTE_H

#include <stdint.h>

/* enemy.asm L242-274 (T44, PROGRESS.md §6ao): set_palette / set_ega_palette y el modelo de los registros
 * de color que el original toca con INT 10h AH=0Bh / AX=1000h y `out 0x3d9`.
 *
 * Registro de seleccion de color CGA (puerto 0x3D9): bits 0-3 = color de fondo/borde (RGBI, 0..15; es el
 * color del indice 0), bit 4 = intensidad de los colores de primer plano, bit 5 = paleta (1 = cian/magenta/
 * blanco, 0 = verde/rojo/marron). BIOS INT 10h AH=0Bh (semantica IBM, NO sale del ASM): BH=0 -> bits 0-4 = BL&0x1f
 * (borra tambien la intensidad si BL=0); BH=1 -> bit 5 = (BL != 0). La BIOS guarda una copia del byte (0x465)
 * y la reescribe entera en cada llamada; el original ademas escribe 0x20 directo (entry.asm L60). Aqui hay un
 * solo byte: la copia y el registro coinciden salvo el instante entre `out 0x3d9` y la siguiente llamada BIOS,
 * y ese instante lo cierra el propio original (0x20 == el resultado de BH=0,BL=0 con paleta 1). */
extern uint8_t cga_color_select;          /* valor actual del puerto 0x3D9 */
/* PCjr (rom_id == 0xfd): registros de paleta EGA 0..15 (INT 10h AX=1000h, BL=registro, BH=valor RGBI). */
extern uint8_t ega_palette_reg[16];

void bios_color_select(uint8_t bh, uint8_t bl);   /* INT 10h AH=0Bh */
void set_ega_palette(uint8_t bl, uint8_t bh);     /* INT 10h AX=1000h */
/* set_palette: paleta del nivel `level_number` (tabla cga_palette_table DS 0x1853, o pcjr_palette_1/2/3 DS
 * 0x183b/0x1843/0x184b) y luego fondo/borde a negro (BH=0,BL=0). */
void set_palette(void);

/* Color 0xAARRGGBB del indice CGA 0..3 con el estado actual (lo usa video.c). */
uint32_t palette_rgb(unsigned idx);
/* Color del borde (T78): lo usa video.c para el fondo del renderer (franjas fuera de los 320x200). */
uint32_t palette_border_rgb(void);

#endif
