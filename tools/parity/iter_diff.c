/* Diferencial por iteracion (plan PARIDAD §9 "Siguiente" (1)).
 * Lee el STATE.BIN de la cueva por iteracion (tools/orig_state/cave_iter.asm: registro completo + deltas), y para cada par
 * de iteraciones consecutivas (k, k+1) del ORIGINAL:
 *   1. carga el estado del registro k en los globales del port (tabla generada: tools/parity/gen_iter_map.py),
 *   2. ejecuta UNA pasada del bucle del callejon del port (game_alley_frame),
 *   3. compara los mismos globales con el registro k+1 del original.
 * No depende del azar ni del ritmo: cada paso parte del estado exacto del original (incluido rng_seed y los ticks).
 * Los registros se toman en el mismo punto del bucle (tras process_keyboard/poll_joystick, antes de update_animation),
 * asi que input_process_keys/poll_joystick quedan anulados (las variables de entrada vienen del registro).
 *
 * Lo que el registro NO contiene y por eso se modela:
 *  - Retrazo vertical (puerto 0x3DA): el original solo avanza el perro, la caida y la animacion (nivel 2/quieto) CON retrazo,
 *    y los objetos lanzados y el salto SIN retrazo. El port los trata como "siempre listos" (vsync_gate). Si el paso exacto
 *    no coincide, se prueban todos los resultados posibles de los portones (de menos a mas cerrados) y el par se clasifica
 *    "explicado por retrazo" si alguno coincide.
 *  - Tick BIOS: si el del registro k+1 difiere del de k, el original pudo leer cualquiera dentro de la iteracion: se prueban ambos.
 * Clasificacion de cada par: EXACTO (el port, tal cual, da el estado k+1), RETRAZO (coincide con algun patron de retrazo),
 * DIVERGE (ningun intento coincide: se informa el intento con menos variables distintas).
 *
 * Uso: iter_diff STATE.BIN [--ignore a,b,c] [--from K] [--to K] [--show N] */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cga.h"
#include "cat_state.h"
#include "game_flow.h"
#include "animation_entry.h"
#include "score.h"
#include "bios_clock.h"
#include "iter_map.h"

/* ventanas de DS registradas por tools/orig_state/cave_iter.asm: A = 0000..2BFF, B = 5900..5B1F (estado de sonido) */
#define A_LEN 0x2c00
#define B_BASE 0x5900
#define B_LEN 0x220
#define REC_LEN (A_LEN + B_LEN)                   /* bytes de un registro completo */
#define DUMP_LEN (B_BASE + B_LEN)                 /* imagen en memoria indexada por offset de DS (el hueco queda a 0) */
#define NGATES 5                                  /* portones de retrazo por pasada (update_animation, enemies, thrown, jump, falling) */

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
void __wrap_speaker_spin_cycles(uint32_t cycles) { (void)cycles; }
static uint16_t cur_tick;                            /* tick BIOS del registro: lo ve todo el codigo (sonido, enemigos...) */
uint16_t __wrap_read_bios_tick(void) { return cur_tick; }
void __wrap_poll_joystick(void) { }                 /* el registro ya trae el resultado de poll_joystick */

static uint16_t clk(void) { return cur_tick; }
static uint16_t pit_fn(void) { return 0x1234; }

/* retrazo: el bit i de vs_mask dice si el porton i-esimo (en orden de llamada) deja pasar (1) o no (0) */
static unsigned vs_mask; static int vs_idx; static const char *vs_site[8]; static bool vs_open[8];
static bool vs_hook(const char *site, bool need_retrace) {
    (void)need_retrace; int i = vs_idx++; bool open = i >= NGATES ? true : ((vs_mask >> i) & 1) != 0;
    if (i < 8) { vs_site[i] = site; vs_open[i] = open; }
    return open;
}

typedef struct { uint16_t n, tick; uint32_t pos, nd; } rec_t;
static uint8_t *file; static long flen; static rec_t *recs; static int nrecs;

static int load_file(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); return -1; }
    fseek(f, 0, SEEK_END); flen = ftell(f); fseek(f, 0, SEEK_SET);
    file = malloc(flen); if (fread(file, 1, flen, f) != (size_t)flen) { fclose(f); return -1; } fclose(f);
    recs = malloc(sizeof(rec_t) * (flen / 6 + 1));
    long i = 0;
    while (i + 6 <= flen) {
        rec_t r; r.n = file[i] | file[i + 1] << 8; r.tick = file[i + 2] | file[i + 3] << 8; r.nd = file[i + 4] | file[i + 5] << 8; r.pos = (uint32_t)i + 6;
        long len = r.nd == 0xFFFF ? REC_LEN : 4L * r.nd;
        if (i + 6 + len > flen) break;
        recs[nrecs++] = r; i += 6 + len;
    }
    return nrecs ? 0 : -1;
}

static void apply(uint8_t *img, const rec_t *r) {
    if (r->nd == 0xFFFF) { memcpy(img, file + r->pos, A_LEN); memcpy(img + B_BASE, file + r->pos + A_LEN, B_LEN); return; }
    for (uint32_t k = 0; k < r->nd; k++) { const uint8_t *p = file + r->pos + 4 * k; uint16_t off = p[0] | p[1] << 8; img[off] = p[2]; img[off + 1] = p[3]; }
}

static void load_state(const uint8_t *img) { for (int v = 0; v < iter_nvars; v++) memcpy(iter_vars[v].addr, img + iter_vars[v].off, iter_vars[v].len); }

/* a: el original no cambio la variable y el port si; b: el original la cambio y el port no; c: ambos la cambiaron distinto */
typedef struct { long bad, first_k, a, b, c; char o[40], p[40], pre[40]; bool ign; } stat_t;
static stat_t st[2048]; static unsigned char pair_cls[2048];

static void hex(char *dst, const uint8_t *b, int n) { int m = n > 8 ? 8 : n; for (int i = 0; i < m; i++) sprintf(dst + 2 * i, "%02x", b[i]); if (n > 8) strcat(dst, ".."); }

static int last_left;                               /* la pasada salio del callejon */
/* una pasada desde img_k; devuelve cuantas variables (no ignoradas) difieren de img_n; si rep, acumula estadisticas */
static int attempt(const uint8_t *img_k, const uint8_t *img_n, uint16_t tick, unsigned mask, bool hook, long k, bool rep) {
    vs_mask = mask; vs_idx = 0; vsync_hook = hook ? vs_hook : NULL;
    load_state(img_k); cur_tick = tick;
    gf_next_t nx = game_alley_frame(); last_left = nx != GF_STAY;
    int bad = 0;
    if (rep) memset(pair_cls, 0, sizeof pair_cls);
    for (int v = 0; v < iter_nvars; v++) {
        const iter_var_t *iv = &iter_vars[v];
        if (memcmp(iv->addr, img_n + iv->off, iv->len) == 0) continue;
        if (!st[v].ign) bad++;
        if (rep) {
            bool orig_chg = memcmp(img_k + iv->off, img_n + iv->off, iv->len) != 0;
            bool port_chg = memcmp(img_k + iv->off, iv->addr, iv->len) != 0;
            int cls = (!orig_chg && port_chg) ? 1 : (orig_chg && !port_chg) ? 2 : 3;
            pair_cls[v] = (unsigned char)cls;
            if (st[v].bad++ == 0) { st[v].first_k = k; hex(st[v].o, img_n + iv->off, iv->len); hex(st[v].p, iv->addr, iv->len); hex(st[v].pre, img_k + iv->off, iv->len); }
            if (cls == 1) st[v].a++; else if (cls == 2) st[v].b++; else st[v].c++;
        }
    }
    return bad;
}

static int popc(unsigned m) { int c = 0; while (m) { c += m & 1; m >>= 1; } return c; }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: %s STATE.BIN [--ignore a,b,c] [--from K] [--to K] [--show N]\n", argv[0]); return 2; }
    long from = 0, to = -1; int show = 0; const char *ign = "";
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--ignore") && i + 1 < argc) ign = argv[++i];
        else if (!strcmp(argv[i], "--from") && i + 1 < argc) from = atol(argv[++i]);
        else if (!strcmp(argv[i], "--to") && i + 1 < argc) to = atol(argv[++i]);
        else if (!strcmp(argv[i], "--show") && i + 1 < argc) show = atoi(argv[++i]);
    }
    if (load_file(argv[1])) { fprintf(stderr, "no se pudo leer %s\n", argv[1]); return 2; }
    if (recs[0].nd != 0xFFFF) { fprintf(stderr, "el primer registro no es completo\n"); return 2; }
    if (iter_nvars > 2048) return 2;
    for (int v = 0; v < iter_nvars; v++) {
        char pat[80]; snprintf(pat, sizeof pat, ",%s,", iter_vars[v].name); char lst[2048]; snprintf(lst, sizeof lst, ",%s,", ign);
        st[v].ign = strstr(lst, pat) != NULL;
    }
    cga_init();
    bios_clock_hook = clk; game_tick_fn = clk; pit_counter_fn = pit_fn; ua_tick_override = -1;
    game_flow_run(GF_LAB_00AE);                       /* nueva partida -> callejon listo (tablas y fondo inicializados) */
    static uint8_t img[DUMP_LEN], nxt[DUMP_LEN];
    apply(img, &recs[0]);
    long pairs = 0, exact = 0, vsync = 0, diverge = 0, amb[3] = {0, 0, 0}, left = 0; int shown = 0;
    long site_closed[NGATES * 4]; const char *site_name[NGATES * 4]; int nsite = 0; memset(site_closed, 0, sizeof site_closed);
    if (to < 0 || to > nrecs - 2) to = nrecs - 2;
    for (long k = 0; k <= to; k++) {
        memcpy(nxt, img, DUMP_LEN); apply(nxt, &recs[k + 1]);
        if (k >= from) {
            pairs++;
            uint16_t ticks[2] = { recs[k].tick, recs[k + 1].tick }; int nt = ticks[0] != ticks[1] ? 2 : 1;
            int cls = 2;                              /* 0 exacto, 1 retrazo, 2 diverge */
            int best = 1 << 30; unsigned best_mask = 0; int best_t = 0; bool best_hook = false;
            for (int t = 0; t < nt && cls == 2; t++) {
                int bad = attempt(img, nxt, ticks[t], 0, false, k, false);
                if (!bad) cls = 0;
                else if (bad < best) { best = bad; best_mask = 0; best_t = t; best_hook = false; }
            }
            /* retrazo: menos portones cerrados primero */
            for (int closed = 1; closed <= NGATES && cls == 2; closed++)
                for (unsigned m = 0; m < (1u << NGATES) && cls == 2; m++) {
                    if (NGATES - popc(m) != closed) continue;
                    for (int t = 0; t < nt && cls == 2; t++) {
                        int bad = attempt(img, nxt, ticks[t], m, true, k, false);
                        if (!bad) {
                            cls = 1;
                            for (int g = 0; g < vs_idx && g < 8; g++) if (!vs_open[g]) {
                                int s; for (s = 0; s < nsite; s++) if (site_name[s] == vs_site[g]) break;
                                if (s == nsite) { site_name[nsite] = vs_site[g]; nsite++; }
                                site_closed[s]++;
                            }
                        } else if (bad < best) { best = bad; best_mask = m; best_t = t; best_hook = true; }
                    }
                }
            amb[nt - 1]++;                            /* amb[0]: tick sin ambiguedad; amb[1]: con ambiguedad */
            if (cls == 0) exact++; else if (cls == 1) vsync++;
            else {
                diverge++;
                attempt(img, nxt, ticks[best_t], best_mask, best_hook, k, true);   /* informar el mejor intento */
                left += last_left;
                if (shown < show) {
                    shown++; printf("par k=%ld (tick %u -> %u) diverge (mejor intento: tick %s, retrazo %s):", k, ticks[0], ticks[1], best_t ? "k+1" : "k", best_hook ? "con patron" : "sin modelo");
                    for (int v = 0; v < iter_nvars; v++) if (!st[v].ign && pair_cls[v]) { char a[40] = "", b[40] = "", c[40] = ""; hex(a, img + iter_vars[v].off, iter_vars[v].len); hex(b, nxt + iter_vars[v].off, iter_vars[v].len); hex(c, iter_vars[v].addr, iter_vars[v].len);
                        printf(" %s[%c %s>%s|port %s]", iter_vars[v].name, "-ABC"[pair_cls[v]], a, b, c); }
                    printf("\n");
                }
            }
        }
        memcpy(img, nxt, DUMP_LEN);
    }
    printf("registros: %d; pares comparados: %ld (con tick ambiguo: %ld)\n", nrecs, pairs, amb[1]);
    printf("  EXACTO  (el port, tal cual, da el estado siguiente): %ld\n", exact);
    printf("  RETRAZO (coincide con algun patron de retrazo 0x3DA): %ld\n", vsync);
    printf("  DIVERGE (ningun intento coincide)                   : %ld\n", diverge);
    if (nsite) { printf("  portones cerrados en los pares RETRAZO:"); for (int s = 0; s < nsite; s++) printf(" %s=%ld", site_name[s], site_closed[s]); printf("\n"); }
    if (left) printf("  pasadas que salieron del callejon (muerte/nivel): %ld\n", left);
    printf("variables mapeadas: %d\n", iter_nvars);
    if (diverge) {
        printf("clases (solo pares DIVERGE): A = el original no cambio la variable y el port si; B = el original la cambio y el port no; C = ambos la cambiaron distinto\n");
        printf("%-24s %6s %5s %5s %5s %8s  %-18s %-18s %-18s\n", "variable", "pares", "A", "B", "C", "primer k", "antes(orig)", "despues(orig)", "despues(port)");
        for (int v = 0; v < iter_nvars; v++)
            if (st[v].bad) printf("%-24s %6ld %5ld %5ld %5ld %8ld  %-18s %-18s %-18s%s\n", iter_vars[v].name, st[v].bad, st[v].a, st[v].b, st[v].c, st[v].first_k, st[v].pre, st[v].o, st[v].p, st[v].ign ? "  (ignorada)" : "");
    }
    return diverge ? 1 : 0;
}
