#ifndef SN76496_H
#define SN76496_H
#include <stdint.h>
#include <stddef.h>
/* Emulador del TI SN76496 (PCjr/Tandy, puerto 0xC0): 3 tonos + ruido, 4 bits de atenuacion.
 * Reloj 3.579545 MHz, divisor /32. SDL-free (testeable). */
void   sn76496_reset(void);
void   sn76496_write(uint8_t value);
/* Suma (con saturacion) `frames` muestras S16 mono a 44100 Hz sobre out. */
void   sn76496_mix(int16_t *out, size_t frames);
#endif
