#!/usr/bin/env python3
"""Genera build/iter_map.c: tabla {etiqueta del original -> offset en DS, tamano, direccion del global del port}.

Une dos fuentes por NOMBRE:
  - el listado de NASM del desensamblado (etiquetas de datos y su offset en DS; el tamano es la distancia a la
    siguiente etiqueta, asi que es una COTA SUPERIOR: se copia min(tamano original, tamano del global del port));
  - `nm -S` de los .o del port (globales externos de datos con su tamano).
Las etiquetas sin global del mismo nombre en el port no se mapean (se informan con --list-unmapped).

Uso: python3 tools/parity/gen_iter_map.py cat.lst nm.txt build/iter_map.c [--list-unmapped]
  nm.txt: salida de `nm -S --defined-only *.o` (se filtran los simbolos B/D)."""
import os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'orig_state'))
import read_state as rs


def port_symbols(nm_path):
    syms = {}
    for l in open(nm_path):
        f = l.split()
        if len(f) == 4 and f[2] in ('B', 'D'): syms[f[3]] = int(f[1], 16)
    return syms


# Globales que define el propio arnes (tools/parity/iter_diff.c) en lugar de src/ (input.c usa SDL y no se enlaza): se cargan
# desde el registro igualmente (entrada ya procesada, sonido, banderas de reinicio/atraccion). Tamano 1.
# Ventanas de DS que registra tools/orig_state/cave_iter.asm (A: juego; B: estado de sonido)
A_LEN, B_BASE, B_LEN = 0x2e40, 0x5900, 0x220
FULL = '--full' in sys.argv                      # modo FULLDS: una ventana DS:0000..712F (build_dump_exe.py --interior)
if FULL: A_LEN, B_BASE, B_LEN = 0x7130, 0x7130, 0
# Globales del port que NO se pueden cargar bytes-a-bytes desde el registro: datos de solo lectura (escribirlos da SIGSEGV) y punteros
# (el original guarda un offset DS de 16 bits; el port un puntero real).
# obj_save_buf: en el original es una tabla de 3 PUNTEROS (DS:1f59) a buffers de 32 bytes en DS:1ed0..; el port tiene los pixeles en obj_save_buf[3][16].
# load_state (iter_diff.c) los carga desde los buffers reales.
EXCLUDE = {'death_sprite', 'gravity_cur_sprite', 'obj_save_buf','l5_obj_save_buf'}
# Estado del port con otro nombre (o `static` en origen, ahora con enlace externo) que corresponde a una etiqueta del original: port -> etiqueta.
ALIASES = {
    # nivel 1/4/6: objeto lanzado (escoba) de level_objects.c (direcciones de los comentarios del port)
    'l1_last_col': 0x32b6, 'l1_anim_frame': 0x327a, 'l1_dir_x': 0x3280, 'l1_dir_y': 0x3281, 'l1_draw_addr': 0x3282, 'l1_next_draw_addr': 0x3284,
    'l1_undrawn': 0x3286, 'l1_saved_x': 0x32ef, 'l1_saved_y': 0x32f1, 'l1_last_tick': 0x328c, 'l1_col_index': 0x328a, 'l1_noise_counter': 0x32ea,
    'l1_level_start_tick': 0x410, 'l1_noise_accum': 0x32eb, 'l1_bias_mag': 0x32ec, 'l1_bias_x': 0x32ed, 'l1_bias_y': 0x32ee,
    'cupid_bg_save': 'dat_70cc',      # nivel 7: fondo bajo el cupido (cupid_draw.c)
    'l6_tracker_save': 'dat_43a0',    # nivel 6: fondo bajo el rastreador (3 palabras x 10 filas)
    'l6_obj_flag': 'dat_4441',        # nivel 6: 12 palabras
    'l6_tile_type': 'dat_44c4',       # nivel 6: 12 bytes
}
HARNESS = ['sound_enabled', 'input_horizontal', 'input_vertical', 'restart_game', 'show_attract']


def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    if len(a) != 3: sys.exit(__doc__)
    m = rs.build_map(a[0], merge_dat=True); ps = port_symbols(a[1]); rows = []; unm = []
    # globales del port con nombre lN_dat_XXXX: son la etiqueta dat_XXXX del original (DS:XXXX); se mapean por direccion
    for nm_ in list(ps):
        mm = re.match(r'^l\d_dat_([0-9a-f]{4})$', nm_)
        if mm and nm_ not in m: m[nm_] = (int(mm.group(1), 16), ps[nm_])
    for h in HARNESS: ps.setdefault(h, 1)
    for pn, on in ALIASES.items():
        if pn in ps and isinstance(on, int): m[pn] = (on, ps[pn])
        elif pn in ps and on in m: m[pn] = m[on]
    for name, (off, osz) in sorted(m.items(), key=lambda kv: kv[1][0]):
        if not (off < A_LEN or B_BASE <= off < B_BASE + B_LEN): continue   # fuera de las ventanas registradas
        if name not in ps or name in EXCLUDE: unm.append(name); continue
        n = min(osz, ps[name], (A_LEN - off) if off < A_LEN else (B_BASE + B_LEN - off))
        if n > 0: rows.append((name, off, n))
    with open(a[2], 'w') as o:
        o.write('/* generado por tools/parity/gen_iter_map.py: NO editar */\n#include <stdint.h>\n#include "iter_map.h"\n')
        for name, _, _ in rows: o.write(f'extern char {name};\n')
        o.write('const iter_var_t iter_vars[] = {\n')
        for name, off, n in rows: o.write(f'    {{"{name}", 0x{off:04x}, {n}, &{name}}},\n')
        o.write('};\n')
        o.write(f'const int iter_nvars = {len(rows)};\n')
    print(f'{a[2]}: {len(rows)} variables mapeadas, {len(unm)} etiquetas sin global homonimo en el port')
    if '--list-unmapped' in sys.argv: print(' '.join(unm))


if __name__ == '__main__':
    main()
