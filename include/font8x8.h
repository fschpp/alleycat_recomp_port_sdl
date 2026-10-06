#ifndef FONT8X8_H
#define FONT8X8_H
#include <stdint.h>
/* Glifos 8x8 ASCII 0..127; bit 0 = pixel izquierdo (ver src/font8x8.c). Sustituye a la fuente ROM de la BIOS. */
extern const uint8_t font8x8[128][8];
#endif
