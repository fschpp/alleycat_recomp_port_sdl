#ifndef BIOS_CLOCK_H
#define BIOS_CLOCK_H
/* bios_clock.h — unico sustituto de `sub ah,ah / int 0x1a` (dx = contador de ticks BIOS, 18.2 Hz) para los modulos del
 * port que antes llevaban cada uno su copia estatica de clock_gettime (T74: el soak simulaba el reloj solo en algunos
 * y el perro, que lee el reloj real, parecia "clavado").
 *
 * Sin hook: reloj monotono del host a ~18.2 Hz (ms / 55, igual que las copias anteriores).
 * Con hook (tests/soak): devuelve lo que diga el hook. Los modulos con su propio override (l5/l6/l2/ua) lo conservan y
 * solo caen aqui cuando no esta puesto. */
#include <stdint.h>

extern uint16_t (*bios_clock_hook)(void);
uint16_t bios_clock_read(void);

#endif
