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
 * Con --video (STATE.BIN de build_dump_exe.py --iter ... --video; plan N4) cada registro lleva tambien la RAM de video B800:0000..3FFF:
 * se carga en cga_mem del port antes de la pasada y se compara cga_mem con el video del registro k+1 (un par con bytes de video distintos
 * cuenta como no coincidente, igual que una variable). Los *_save_buf dejan de necesitar --ignore.
 *
 * Con --level (STATE.BIN de build_dump_exe.py --interior): los registros son del bucle de un nivel interior (enganche en play_sound, tras
 * process_keyboard/poll_joystick) y una pasada es game_level_frame() del nivel del primer registro (level_number, DS:0004).
 *
 * Uso: iter_diff STATE.BIN [--video] [--level] [--ignore a,b,c] [--from K] [--to K] [--show N] */
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
#include "gen/ds_pool.h"
#include "iter_map.h"

/* ventanas de DS registradas por tools/orig_state/cave_iter.asm: A = 0000..2E3F, B = 5900..5B1F (estado de sonido) */
#ifdef FULLDS                                       /* build/iter_diff_full: STATE.BIN de build_dump_exe.py --interior (una ventana: todo DS:0000..712F) */
#define A_LEN 0x7130
#define B_BASE 0x7130
#define B_LEN 0
#else
#define A_LEN 0x2e40
#define B_BASE 0x5900
#define B_LEN 0x220
#endif
#define REC_LEN (A_LEN + B_LEN)                   /* bytes de un registro completo */
#define DUMP_LEN (B_BASE + B_LEN)                 /* imagen en memoria indexada por offset de DS (el hueco queda a 0) */
#define NGATES 5                                  /* portones de retrazo por pasada (update_animation, enemies, thrown, jump, falling) */

int8_t input_horizontal, input_vertical; bool input_fire;
uint8_t sound_enabled = 0; bool restart_game, show_attract, pause_requested;
static long spins;                                  /* esperas activas del port (play_crash_sound...): hacen avanzar el reloj falso */
void __wrap_speaker_spin_cycles(uint32_t cycles) { (void)cycles; spins++; }
/* tick BIOS: el original lo lee varias veces por pasada (animacion, lanzados, objetos, sonido...) y DOSBox puede cambiarlo A MITAD de la pasada
 * (cientos de iteraciones por tick): las primeras `sw` lecturas ven el tick `a` y las demas el tick `b` (sw grande = el tick no cambia). */
typedef struct { uint16_t a, b; int sw; } tk_t;
static tk_t cur_tk; static int nreads;
/* PIT falso: cuenta hacia atras 0x300 por lectura (como el original, que lo lee en esperas activas) y cada lectura cuenta como una espera. */
static uint16_t fake_pit;
uint16_t __wrap_read_pit_timer(void) { spins++; fake_pit = (uint16_t)(fake_pit - 0x300); return fake_pit; }
/* Una pasada normal casi no llama a speaker_spin_cycles; las esperas bloqueantes del original (crash de la muerte: espera 2 ticks BIOS)
 * necesitan que el tiempo avance, o el arnes cuelga: 1 tick por cada 200 esperas. */
static uint16_t tick_read(void) { return (uint16_t)((nreads++ < cur_tk.sw ? cur_tk.a : cur_tk.b) + spins / 200); }
uint16_t __wrap_read_bios_tick(void) { return tick_read(); }
void __wrap_poll_joystick(void) { }                 /* el registro ya trae el resultado de poll_joystick */

/* ITER_TRACE=k: traza (a stderr) los blits del intento que se reinforma para el par k (ver ITER_VBOX); solo diagnostico */
static int trace_on;
#define TR(name, dst, w, h) do { if (trace_on) fprintf(stderr, "    [%s dst=%04zx(fila %zu col %zu) w=%u h=%u desde %p]\n", name, (size_t)(dst), ((size_t)(dst) & 0x1fff) / 80 * 2 + ((size_t)(dst) >= 0x2000), ((size_t)(dst) & 0x1fff) % 80, (unsigned)(w), (unsigned)(h), __builtin_return_address(0)); } while (0)
extern void __real_blit_to_cga(const uint8_t *, size_t, uint8_t, uint8_t);
extern void __real_blit_masked(const uint8_t *, size_t, uint8_t, uint8_t, uint16_t *);
extern void __real_blit_transparent(const uint8_t *, size_t, uint8_t, uint8_t, uint16_t *);
extern void __real_save_from_cga(uint8_t *, size_t, uint8_t, uint8_t);
void __wrap_blit_to_cga(const uint8_t *a, size_t d, uint8_t w, uint8_t h) { TR("blit_to_cga", d, w, h); __real_blit_to_cga(a, d, w, h); }
void __wrap_blit_masked(const uint8_t *a, size_t d, uint8_t w, uint8_t h, uint16_t *b) { TR("blit_masked", d, w, h); __real_blit_masked(a, d, w, h, b); }
void __wrap_blit_transparent(const uint8_t *a, size_t d, uint8_t w, uint8_t h, uint16_t *b) { TR("blit_transparent", d, w, h); __real_blit_transparent(a, d, w, h, b); }
void __wrap_save_from_cga(uint8_t *a, size_t d, uint8_t w, uint8_t h) { TR("save_from_cga", d, w, h); __real_save_from_cga(a, d, w, h); }

/* diagnostico (ITER_DEBUG=1): traza las llamadas a spawn_window_event de cada intento */
extern void __real_spawn_window_event(void);
void __wrap_spawn_window_event(void) {
    if (getenv("ITER_DEBUG")) fprintf(stderr, "  [spawn_window_event buffer_size=%x window_event_count=%u]\n", buffer_size, window_event_count);
    __real_spawn_window_event();
}

static uint16_t clk(void) { return tick_read(); }
static uint16_t pit_fn(void) { return 0x1234; }

/* retrazo: el bit i de vs_mask dice si el porton i-esimo (en orden de llamada) deja pasar (1) o no (0) */
static unsigned vs_mask; static int vs_idx; static const char *vs_site[8]; static bool vs_open[8];
static bool vs_hook(const char *site, bool need_retrace) {
    (void)need_retrace; int i = vs_idx++; bool open = i >= NGATES ? true : ((vs_mask >> i) & 1) != 0;
    if (i < 8) { vs_site[i] = site; vs_open[i] = open; }
    return open;
}

typedef struct { uint16_t n, tick; uint32_t pos, nd, vpos; } rec_t;
static bool use_video, use_level;                   /* --level: bucle de nivel interior (game_level_frame) */
                              /* --video: los registros llevan la RAM de video */
#define VID_LEN 0x4000
static uint8_t *file; static long flen; static rec_t *recs; static int nrecs;

static int load_file(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); return -1; }
    fseek(f, 0, SEEK_END); flen = ftell(f); fseek(f, 0, SEEK_SET);
    file = malloc(flen); if (fread(file, 1, flen, f) != (size_t)flen) { fclose(f); return -1; } fclose(f);
    recs = malloc(sizeof(rec_t) * (flen / 6 + 1));
    long i = 0;
    while (i + 6 <= flen) {
        rec_t r; r.n = file[i] | file[i + 1] << 8; r.tick = file[i + 2] | file[i + 3] << 8; r.nd = file[i + 4] | file[i + 5] << 8; r.pos = (uint32_t)i + 6;
        long len = r.nd == 0xFFFF ? REC_LEN : 4L * r.nd; r.vpos = 0;
        if (use_video) {
            long vp = i + 6 + len; long vl;
            if (r.nd == 0xFFFF) vl = VID_LEN; else { if (vp + 2 > flen) break; vl = 2 + 4L * (file[vp] | file[vp + 1] << 8); }
            r.vpos = (uint32_t)vp; len += vl;
        }
        if (i + 6 + len > flen) break;
        recs[nrecs++] = r; i += 6 + len;
    }
    return nrecs ? 0 : -1;
}

static void apply_vid(uint8_t *v, const rec_t *r) {
    if (r->nd == 0xFFFF) { memcpy(v, file + r->vpos, VID_LEN); return; }
    uint32_t nv = file[r->vpos] | file[r->vpos + 1] << 8;
    for (uint32_t k = 0; k < nv; k++) { const uint8_t *p = file + r->vpos + 2 + 4 * k; uint16_t off = p[0] | p[1] << 8; v[off] = p[2]; v[off + 1] = p[3]; }
}
static void apply(uint8_t *img, const rec_t *r) {
    if (r->nd == 0xFFFF) { memcpy(img, file + r->pos, A_LEN); memcpy(img + B_BASE, file + r->pos + A_LEN, B_LEN); return; }
    for (uint32_t k = 0; k < r->nd; k++) { const uint8_t *p = file + r->pos + 4 * k; uint16_t off = p[0] | p[1] << 8; img[off] = p[2]; img[off + 1] = p[3]; }
}

/* vert_sprite (pose de subida/entrada/recoil): el original guarda vert_sprite_dims (DS 0x567) y vert_sprite_data (DS 0x569) como palabras; el port un
 * puntero a cat_walk_frame_t que no esta en el registro. Se sintetiza uno con los mismos datos y dimensiones (los bytes de ds_pool son los del .exe, B1). */
static cat_walk_frame_t vs_frame;
static void load_state(const uint8_t *img) {
    for (int v = 0; v < iter_nvars; v++) memcpy(iter_vars[v].addr, img + iter_vars[v].off, iter_vars[v].len);
    uint16_t vdims = img[0x567] | img[0x568] << 8, vdata = img[0x569] | img[0x56a] << 8;
    vs_frame.data = vdata < DS_POOL_SIZE ? &ds_pool[vdata] : ds_pool; vs_frame.width_words = (uint8_t)vdims; vs_frame.height = (uint8_t)(vdims >> 8);
    vert_sprite = &vs_frame;
    /* nivel 7: l7_save_buf_ptrs (DS:45a8) = 7 punteros a los slots de 12 bytes (l7_cat_x..l7_cat_delay) que load/save_l7_slot intercambian */
    { extern void l7_slot_load_raw(int, const uint8_t *);
      for (int sl = 0; sl < 7; sl++) { uint16_t pt = img[0x45a8 + 2 * sl] | img[0x45a9 + 2 * sl] << 8; if (pt && pt + 12u <= DUMP_LEN) l7_slot_load_raw(sl, img + pt); } }
    /* l5_obj_save_buf: 4 punteros DS (0x3ec2) a buffers de 48 bytes (niveles 4/5) */
    { extern uint8_t *l5_obj_save_buf[4];
      for (int sl = 0; sl < 4; sl++) { uint16_t pt = img[0x3ec2 + 2 * sl] | img[0x3ec3 + 2 * sl] << 8; if (pt && pt + 48u <= DUMP_LEN) memcpy(l5_obj_save_buf[sl], img + pt, 48); } }
    /* obj_save_buf: tabla de 3 punteros DS (0x1f59) a buffers de 32 bytes (pixeles guardados de los objetos del callejon) */
    for (int sl = 0; sl < 3; sl++) { uint16_t pt = img[0x1f59 + 2 * sl] | img[0x1f5a + 2 * sl] << 8; if (pt + 32u <= DUMP_LEN) memcpy(obj_save_buf[sl], img + pt, 32); }
}

/* a: el original no cambio la variable y el port si; b: el original la cambio y el port no; c: ambos la cambiaron distinto */
typedef struct { long bad, first_k, a, b, c; char o[40], p[40], pre[40]; bool ign; } stat_t;
static stat_t st[2048]; static unsigned char pair_cls[2048];

static void hex(char *dst, const uint8_t *b, int n) { int m = n > 8 ? 8 : n; for (int i = 0; i < m; i++) sprintf(dst + 2 * i, "%02x", b[i]); if (n > 8) strcat(dst, ".."); }

static const uint8_t *vid_k, *vid_n;                 /* video del registro k y k+1 (NULL sin --video) */
static long vdiff_pairs, vdiff_bytes_max, vdiff_first_k = -1; static int vdiff_off[64], vdiff_nshow;
static long repr_hits;                              /* variables que difieren solo por representacion (ver attempt) */
static int last_left;                               /* la pasada salio del callejon */
/* una pasada desde img_k; devuelve cuantas variables (no ignoradas) difieren de img_n; si rep, acumula estadisticas */
static int attempt(const uint8_t *img_k, const uint8_t *img_n, tk_t tk, unsigned mask, bool hook, long k, bool rep) {
    if (getenv("ITER_DEBUG") && rep == false && k == atol(getenv("ITER_DEBUG"))) fprintf(stderr, "intento mask=%x hook=%d tick=%u->%u sw=%d\n", mask, hook, tk.a, tk.b, tk.sw);
    vs_mask = mask; vs_idx = 0; vsync_hook = hook ? vs_hook : NULL;
    if (vid_k) memcpy(cga_mem, vid_k, VID_LEN);
    trace_on = rep && getenv("ITER_TRACE") != NULL;
    load_state(img_k); cur_tk = tk; nreads = 0; spins = 0;
    if (use_level) last_left = game_level_frame() != GL_STAY; else last_left = game_alley_frame() != GF_STAY;
    int bad = 0;
    if (rep) memset(pair_cls, 0, sizeof pair_cls);
    if (vid_n) {
        long nb = 0; int first[64], nf = 0;
        for (int o = 0; o < VID_LEN; o++) if (cga_mem[o] != vid_n[o]) { if (nf < 64) first[nf++] = o; nb++; }
        if (nb) {
            bad++;
            if (rep) { if (vdiff_pairs++ == 0) vdiff_first_k = k; if (nb > vdiff_bytes_max) vdiff_bytes_max = nb; vdiff_nshow = nf; memcpy(vdiff_off, first, sizeof first); }
        }
    }
    for (int v = 0; v < iter_nvars; v++) {
        const iter_var_t *iv = &iter_vars[v];
        if (memcmp(iv->addr, img_n + iv->off, iv->len) == 0) continue;
        /* representacion (no logica): con vert_sprite el original pone cat_sprite_data = vert_sprite_data (offset DS 0x569, p. ej. las poses de
         * subida/entrada) y el port conserva el valor anterior porque guarda un puntero real en vert_sprite (animation_c.c:206). */
        if (!strcmp(iv->name, "cat_sprite_data") && memcmp(img_n + iv->off, img_n + 0x569, 2) == 0 && memcmp(iv->addr, img_k + iv->off, 2) == 0) { repr_hits++; continue; }
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
    if (getenv("ITER_DEBUG") && !rep && k == atol(getenv("ITER_DEBUG"))) {
        fprintf(stderr, "  -> %d variables distintas:", bad);
        for (int v = 0; v < iter_nvars; v++) if (!st[v].ign && memcmp(iter_vars[v].addr, img_n + iter_vars[v].off, iter_vars[v].len)) fprintf(stderr, " %s", iter_vars[v].name);
        fprintf(stderr, "\n");
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
        else if (!strcmp(argv[i], "--video")) use_video = true;
        else if (!strcmp(argv[i], "--level")) use_level = true;
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
    static uint8_t img[DUMP_LEN], nxt[DUMP_LEN], vimg[VID_LEN], vnxt[VID_LEN];
    apply(img, &recs[0]); if (use_video) apply_vid(vimg, &recs[0]);
    if (use_level) {                                  /* init del nivel del primer registro (despacho + init, como lab_0238) */
        level_number = img[4] | img[5] << 8; fprintf(stderr, "nivel %u\n", level_number);
        if (!game_level_enter()) { fprintf(stderr, "game_level_enter fallo\n"); return 2; }
    }
    long pairs = 0, exact = 0, vsync = 0, diverge = 0, amb[3] = {0, 0, 0}, left = 0; int shown = 0;
    long site_closed[NGATES * 4]; const char *site_name[NGATES * 4]; int nsite = 0; memset(site_closed, 0, sizeof site_closed);
    if (to < 0 || to > nrecs - 2) to = nrecs - 2;
    for (long k = 0; k <= to; k++) {
        memcpy(nxt, img, DUMP_LEN); apply(nxt, &recs[k + 1]);
        if (use_video) { memcpy(vnxt, vimg, VID_LEN); apply_vid(vnxt, &recs[k + 1]); vid_k = vimg; vid_n = vnxt; }
        if (k >= from) {
            pairs++;
            /* tick BIOS: el original pudo leerlo en cualquier momento de la iteracion; si entre los dos registros pasaron varios ticks (la E/S de la
             * cueva a veces se alarga) se prueban todos los intermedios (hasta 8). */
            tk_t ticks[48]; int nt = 0; ticks[nt++] = (tk_t){ recs[k].tick, recs[k].tick, 0 };
            if (recs[k + 1].tick != recs[k].tick) {
                unsigned span = (uint16_t)(recs[k + 1].tick - recs[k].tick);
                if (span == 1) for (int sw = 0; sw <= 24; sw++) ticks[nt++] = (tk_t){ recs[k].tick, recs[k + 1].tick, sw };   /* el tick cambia tras `sw` lecturas */
                else { if (span > 8) span = 8; for (unsigned d = 1; d <= span; d++) ticks[nt++] = (tk_t){ (uint16_t)(recs[k].tick + d), (uint16_t)(recs[k].tick + d), 0 }; }
            }
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
            amb[nt > 1]++;                            /* amb[0]: tick sin ambiguedad; amb[1]: con ambiguedad */
            if (cls == 0) exact++; else if (cls == 1) vsync++;
            else {
                diverge++;
                attempt(img, nxt, ticks[best_t], best_mask, best_hook, k, true);   /* informar el mejor intento */
                left += last_left;
                if (shown < show) {
                    shown++; printf("par k=%ld (tick %u -> %u) diverge (mejor intento: tick %s, retrazo %s):", k, recs[k].tick, recs[k + 1].tick, best_t ? "k+1/mitad" : "k", best_hook ? "con patron" : "sin modelo");
                    for (int v = 0; v < iter_nvars; v++) if (!st[v].ign && pair_cls[v]) { char a[40] = "", b[40] = "", c[40] = ""; hex(a, img + iter_vars[v].off, iter_vars[v].len); hex(b, nxt + iter_vars[v].off, iter_vars[v].len); hex(c, iter_vars[v].addr, iter_vars[v].len);
                        printf(" %s[%c %s>%s|port %s]", iter_vars[v].name, "-ABC"[pair_cls[v]], a, b, c); }
                    if (vid_n && vdiff_pairs) { printf(" VIDEO[primeros bytes distintos:"); for (int q = 0; q < vdiff_nshow; q++) printf(" %04x(orig %02x|port %02x)", vdiff_off[q], vnxt[vdiff_off[q]], cga_mem[vdiff_off[q]]); printf("]"); }
                    printf("\n");
                    if (vid_n && getenv("ITER_VBOX")) {   /* caja de diferencias: filas x bytes de [video k | original k+1 | port k+1] */
                        int r0 = 999, r1 = -1, c0 = 999, c1 = -1;
                        for (int o = 0; o < VID_LEN; o++) if (cga_mem[o] != vnxt[o]) { int r = ((o & 0x1fff) / 80) * 2 + (o >= 0x2000), c = (o & 0x1fff) % 80; if (r < r0) r0 = r; if (r > r1) r1 = r; if (c < c0) c0 = c; if (c > c1) c1 = c; }
                        if (r1 >= 0) { c0 = c0 > 1 ? c0 - 1 : 0; c1 = c1 < 78 ? c1 + 1 : 79; printf("  caja filas %d..%d, bytes %d..%d (x=%d..%d)\n  fila | video k | original k+1 | port k+1\n", r0, r1, c0, c1, c0 * 4, c1 * 4 + 3);
                            for (int r = r0 > 0 ? r0 - 1 : 0; r <= r1 + 1 && r < 200; r++) { int base = (r & 1) * 0x2000 + (r >> 1) * 80;
                                printf("  %3d |", r); for (int c = c0; c <= c1; c++) printf(" %02x", vimg[base + c]); printf(" |");
                                for (int c = c0; c <= c1; c++) printf(" %02x", vnxt[base + c]); printf(" |");
                                for (int c = c0; c <= c1; c++) printf(" %02x%s", cga_mem[base + c], cga_mem[base + c] != vnxt[base + c] ? "*" : " "); printf("\n"); } }
                    }
                }
            }
        }
        memcpy(img, nxt, DUMP_LEN); if (use_video) memcpy(vimg, vnxt, VID_LEN);
    }
    printf("registros: %d; pares comparados: %ld (con tick ambiguo: %ld)\n", nrecs, pairs, amb[1]);
    printf("  EXACTO  (el port, tal cual, da el estado siguiente): %ld\n", exact);
    printf("  RETRAZO (coincide con algun patron de retrazo 0x3DA): %ld\n", vsync);
    printf("  DIVERGE (ningun intento coincide)                   : %ld\n", diverge);
    if (nsite) { printf("  portones cerrados en los pares RETRAZO:"); for (int s = 0; s < nsite; s++) printf(" %s=%ld", site_name[s], site_closed[s]); printf("\n"); }
    if (left) printf("  pasadas que salieron del callejon (muerte/nivel): %ld\n", left);
    printf("variables mapeadas: %d\n", iter_nvars);
    if (use_video) printf("video (N4): pares DIVERGE con video distinto (mejor intento): %ld; max bytes distintos en un par: %ld; primer k: %ld\n", vdiff_pairs, vdiff_bytes_max, vdiff_first_k);
    if (diverge) {
        printf("clases (solo pares DIVERGE): A = el original no cambio la variable y el port si; B = el original la cambio y el port no; C = ambos la cambiaron distinto\n");
        printf("%-24s %6s %5s %5s %5s %8s  %-18s %-18s %-18s\n", "variable", "pares", "A", "B", "C", "primer k", "antes(orig)", "despues(orig)", "despues(port)");
        for (int v = 0; v < iter_nvars; v++)
            if (st[v].bad) printf("%-24s %6ld %5ld %5ld %5ld %8ld  %-18s %-18s %-18s%s\n", iter_vars[v].name, st[v].bad, st[v].a, st[v].b, st[v].c, st[v].first_k, st[v].pre, st[v].o, st[v].p, st[v].ign ? "  (ignorada)" : "");
    }
    return diverge ? 1 : 0;
}
