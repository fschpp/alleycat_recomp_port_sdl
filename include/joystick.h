#ifndef JOYSTICK_H
#define JOYSTICK_H
#include <stdint.h>

/* poll_joystick / decode_joystick_axis (input.asm L4-89). T60, PROGRESS.md §6bc.
 * El puerto de juegos (0x201) se modela con dos hooks; NULL = sin joystick conectado. */

/* in al,0x201: bits 0/1 = ejes X/Y (1 = capacitor cargando), bit 4 = boton (0 = pulsado). NULL -> 0xff.
 * Es el mismo hook que usan ui.c (detect_joystick, wait_for_input). */
/* extern uint8_t (*joy_port_fn)(void);   -- declarado en ui.h */
/* out 0x201,al: dispara el monoestable de los ejes (el valor escrito se ignora). NULL = no-op. */
extern void (*joy_fire_fn)(void);

extern uint16_t joy_timer;       /* DS 0x069c: lectura del PIT canal 0 al disparar */
extern uint8_t  joy_pending;     /* DS 0x069e: bit0 = falta X, bit1 = falta Y */
extern uint16_t joy_last_tick;   /* DS 0x069f: ultimo tick BIOS en que se sondeo (limite: cada 2 ticks) */

/* poll_joystick: como mucho cada 2 ticks BIOS. Sin use_joystick lee el teclado (input_poll) y espera un cambio del
 * PIT; con use_joystick lee el boton (joy_button = bit 4), dispara el monoestable y mide cuanto tarda cada eje en
 * bajar, dejando input_horizontal / input_vertical en -1/0/1 (0xff si el eje no responde). */
void poll_joystick(void);
/* decode_joystick_axis: bl segun el tiempo transcurrido desde joy_timer (en cuentas del PIT): <= 0x506 -> 0xff,
 * <= 0xa1a -> 0, mas -> 1. Conserva el resto de registros. */
uint8_t decode_joystick_axis(void);
#endif
