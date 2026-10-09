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

/* Modelo del retrazo vertical (check_vsync = bit 3 del puerto 0x3DA, `and al,8`). El original solo avanza ciertas rutinas
 * cuando el retrazo esta (o no esta) activo; el port no tiene CGA real y las trata como "listas".
 *   vsync_gate(sitio, necesita_retrazo) -> true = continuar. Sin gancho siempre continua (comportamiento por defecto).
 *   necesita_retrazo = true : el original sigue solo si hay retrazo   (`call check_vsync / jz ret`)
 *   necesita_retrazo = false: el original sigue solo si NO hay retrazo (`call check_vsync / jnz ret`)
 * Con gancho (tools/parity/iter_diff.c) se prueba cada resultado posible para explicar los pares de iteraciones del
 * original en los que una rutina no avanzo porque el retrazo no estaba en ese instante. */
#include <stdbool.h>
extern bool (*vsync_hook)(const char *site, bool need_retrace);
bool vsync_gate(const char *site, bool need_retrace);

#endif
