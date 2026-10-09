/* Tabla de variables (generada en build/iter_map.c por tools/parity/gen_iter_map.py) para tools/parity/iter_diff.c. */
#ifndef ITER_MAP_H
#define ITER_MAP_H
#include <stdint.h>

typedef struct {
    const char *name;     /* etiqueta del original == nombre del global del port */
    uint16_t off;         /* offset en DS del original */
    uint16_t len;         /* bytes comparables: min(tamano en el original, tamano del global del port) */
    void *addr;           /* direccion del global del port */
} iter_var_t;

extern const iter_var_t iter_vars[];
extern const int iter_nvars;
#endif
