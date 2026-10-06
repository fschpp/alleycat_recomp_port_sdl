#ifndef BIOS_TEXT_H
#define BIOS_TEXT_H
#include <stdint.h>

/* Modelo del texto de la BIOS en modo grafico CGA 4 (40x25 celdas de 8x8 px). NO sale del ASM: es el comportamiento
 * de INT 10h que el original usa sin implementarlo. Aqui: AH=02h (fijar cursor) y AH=0Eh (teletype con color BL).
 * Celda (fila, col): 8 filas de pixel, 2 bytes por fila. Los pixeles encendidos del glifo toman el color BL&3 y los
 * apagados se ESCRIBEN a 0 (la BIOS de IBM sobrescribe la celda; no leyo el fondo). Es una suposicion documentada
 * en PROGRESS.md §6as: el ASM no lo fija. El teletype avanza el cursor una columna; no hace scroll ni procesa
 * controles (los textos del bonus no los usan). */
void bios_set_cursor(uint8_t row, uint8_t col);          /* INT 10h AH=02h (DH=row, DL=col, BH=0) */
void bios_teletype(uint8_t ch, uint8_t color);           /* INT 10h AH=0Eh (AL=char, BL=color) */
uint8_t bios_cursor_row(void);
uint8_t bios_cursor_col(void);

#endif
