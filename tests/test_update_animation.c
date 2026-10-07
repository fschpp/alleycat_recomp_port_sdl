/* T74: update_animation() solo despacha. Se envuelven las 7 piezas y se comprueba, para cada salida de la entrada (T70)
 * y de C1 (T72), la secuencia exacta de llamadas y el argumento que recibe C2/D. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "update_animation.h"
#include "animation_entry.h"
#include "animation_c.h"
#include "animation_d.h"

/* input.c (SDL) no entra en TEST_SRC: se definen aqui las variables que enlazan los demas .c */
int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;

static ua_next_t entry_ret;
static ac_next_t c1_ret;
static char log_[64];
static int  log_n;

static void ev(char c) { log_[log_n++] = c; log_[log_n] = 0; }

ua_next_t __wrap_update_animation_entry(void) { ev('E'); return entry_ret; }
void __wrap_update_cat_movement(void)         { ev('M'); }
void __wrap_update_cat_dive(void)             { ev('V'); }
void __wrap_update_cat_frame(void)            { ev('F'); }
ac_next_t __wrap_update_animation_c1(void)    { ev('1'); return c1_ret; }
void __wrap_update_animation_c2(ac_next_t f)  { ev('2'); ev((char)('0' + f)); }
void __wrap_update_animation_d(ac_next_t f)   { ev('D'); ev((char)('0' + f)); }

static int fails;
static void run(const char *name, ua_next_t e, ac_next_t c1, const char *want) {
    entry_ret = e; c1_ret = c1; log_n = 0; log_[0] = 0;
    update_animation();
    if (strcmp(log_, want) != 0) {
        printf("FAIL %s: obtenido '%s', esperado '%s'\n", name, log_, want);
        fails++;
    }
}

int main(void) {
    run("entrada UA_RET",   UA_RET,   AC_RET, "E");
    run("nivel 2 normal",   UA_L09F6, AC_RET, "EMF");
    run("nivel 2 muerte",   UA_L0A86, AC_RET, "EVF");
    run("C1 -> ret",        UA_L0BAC, AC_RET,     "E1");
    {   /* los digitos son el valor del enum: se calculan, no se escriben a mano */
        char w[16];
        snprintf(w, sizeof w, "E12%c", (char)('0' + AC_L0CE7)); run("C1 -> C2 (lab_0ce7)", UA_L0BAC, AC_L0CE7, w);
        snprintf(w, sizeof w, "E12%c", (char)('0' + AC_L0D29)); run("C1 -> C2 (lab_0d29)", UA_L0BAC, AC_L0D29, w);
        snprintf(w, sizeof w, "E1D%c", (char)('0' + AC_L0E23)); run("C1 -> D (lab_0e23)",  UA_L0BAC, AC_L0E23, w);
        snprintf(w, sizeof w, "E1D%c", (char)('0' + AC_L0E78)); run("C1 -> D (lab_0e78)",  UA_L0BAC, AC_L0E78, w);
    }
    if (fails) { printf("test_update_animation: %d fallos\n", fails); return 1; }
    printf("test_update_animation: OK\n");
    return 0;
}
