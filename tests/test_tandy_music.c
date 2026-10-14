/* Compara las escrituras al SN76496 del port con las del codigo original ejecutado en unicorn
 * (tools/tandy/oracle.py -> fichero de traza "tick port value"). Uso: test_tandy_music trace.txt */
#include <stdio.h>
#include <stdlib.h>
#include "tandy_music.h"
static uint16_t cur_tick; static FILE *ref; static long n, bad;
static void hook(uint16_t port, uint8_t v) {
    unsigned t, p, rv; 
    if (fscanf(ref, "%u %u %u", &t, &p, &rv) != 3) { printf("traza de referencia agotada\n"); exit(1); }
    n++;
    if (t != cur_tick || p != port || rv != v) { if (bad++ < 10) printf("DIFF #%ld: port t=%u %02x/%02x ref t=%u %02x/%02x\n", n, cur_tick, port, v, t, p, rv); }
}
int main(int argc, char **argv) {
    ref = fopen(argc > 1 ? argv[1] : "tests/data/tandy_trace_3000.txt", "r"); if (!ref) return 2;
    unsigned ticks; if (fscanf(ref, "%u", &ticks) != 1) return 2;
    tandy_port_hook = hook; cur_tick = 0;
    tandy_music_init(0);
    for (unsigned t = 1; t <= ticks; t++) { cur_tick = (uint16_t)t; tandy_music_tick(cur_tick); }
    unsigned a, b, c; int extra = fscanf(ref, "%u %u %u", &a, &b, &c) == 3;
    printf("%ld escrituras comparadas, %ld diferencias%s\n", n, bad, extra ? " (REF TIENE MAS)" : "");
    return bad || extra;
}
