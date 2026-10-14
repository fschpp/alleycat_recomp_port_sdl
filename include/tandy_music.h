#ifndef TANDY_MUSIC_H
#define TANDY_MUSIC_H
#include <stdint.h>
#include <stdbool.h>

/* Motor de musica de 3 voces del PCjr/Tandy (SN76496), traduccion literal de la
 * compilacion de 3 voces de Alley Cat (CS:0x16a0..0x1829). Ver docs/TANDY_MOTOR.md.
 * Todas las escrituras al chip (out 0xC0) salen por tandy_port_hook / sn76496_write. */
extern void (*tandy_port_hook)(uint16_t port, uint8_t value);  /* opcional: traza (tests) */
void tandy_music_init(uint16_t bios_tick);   /* CS:0x16a0 (con DS:[0] != 0) */
void tandy_music_set_playing(bool on);       /* DS:[1] */
void tandy_music_tick(uint16_t bios_tick);   /* CS:0x173e */
void tandy_music_stop(void);                 /* CS:0x171f */
bool tandy_music_toggle(void);              /* not byte [1]; devuelve el nuevo estado */
uint16_t tandy_music_start_tick(void);       /* DS:[0x1012] */
void tandy_music_silence(void);              /* CS:0x1732 */
#endif
