#!/usr/bin/env python3
"""Compara la traza del original (STATE.BIN de catdump.exe) con la del port (build/port_trace) por tick (plan §3.5).

Uso: python3 tools/parity/compare.py --script STATE.BIN cat.lst   # imprime el guion de entrada del original para port_trace
     python3 tools/parity/compare.py STATE.BIN cat.lst port_trace.txt [--cols cat_x,scroll_speed,...] [--tol N]
Las variables se leen del original por nombre (mapa del listado de NASM). El tick n=1 es el primer registro de ambos.
Para cada columna informa: ticks comparados, ticks iguales, primera divergencia (n, original, port) y la diferencia maxima.
Corta la comparacion en el primer 'evento aleatorio' (perro activo, enemy_active != 0, en cualquiera de los dos lados), porque
el azar depende del numero de iteraciones por tick, que no coincide entre DOSBox y el arnes (ver PLAN §9).
Salida: codigo 0 si todas las columnas coinciden en el tramo comparado."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'orig_state'))
import read_state as rs

PORT_COLS = ['n', 'cat_x', 'cat_y', 'level_number', 'scroll_speed', 'scroll_direction', 'in_level_mode',
             'input_horizontal', 'current_floor', 'at_platform', 'enemy_active', 'enemy_x', 'enemy_y_pos',
             'enemy_chasing', 'enemy_approach_timer']
SIGNED = {'scroll_direction', 'in_level_mode', 'input_horizontal'}
DEFAULT = ['cat_x', 'cat_y', 'level_number', 'scroll_speed', 'scroll_direction', 'in_level_mode']


def s8(v): return v - 256 if v > 127 else v


def emit_script(state_bin, lst):
    """Entrada del original por tick -> guion de port_trace ('h:a-b:v v:a-b:v'): el port consume las mismas teclas."""
    m = rs.build_map(lst); out = []
    for var, k in (('input_horizontal', 'h'), ('input_vertical', 'v')):
        run = None
        for n, tick, ds in rs.records(state_bin):
            v = s8(rs.get(m, var, ds))
            if run and run[2] == v and run[1] == n - 1: run[1] = n
            else:
                if run and run[2] != 0: out.append(f'{k}:{run[0]}-{run[1]}:{run[2]}')
                run = [n, n, v]
        if run and run[2] != 0: out.append(f'{k}:{run[0]}-{run[1]}:{run[2]}')
    return ' '.join(out)


def main():
    if len(sys.argv) == 4 and sys.argv[1] == '--script':
        print(emit_script(sys.argv[2], sys.argv[3])); return 0
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    opts = dict(zip([x for x in sys.argv[1:] if x.startswith('--')], [None] * 9))
    cols, tol = DEFAULT, 0
    if '--cols' in sys.argv: cols = sys.argv[sys.argv.index('--cols') + 1].split(','); a.remove(sys.argv[sys.argv.index('--cols') + 1])
    if '--tol' in sys.argv: tol = int(sys.argv[sys.argv.index('--tol') + 1]); a.remove(sys.argv[sys.argv.index('--tol') + 1])
    if len(a) != 3: sys.exit(__doc__)
    m = rs.build_map(a[1])
    orig = {}
    for n, tick, ds in rs.records(a[0]):
        row = {c: rs.get(m, c, ds) for c in set(cols) | {'enemy_active', 'enemy_chasing', 'enemy_approach_timer'}}
        for c in SIGNED & set(row): row[c] = s8(row[c])
        orig[n] = row
    port = {}
    for line in open(a[2]):
        if line.startswith('#') or line.startswith('n '): continue
        f = line.split()
        if len(f) == len(PORT_COLS): port[int(f[0])] = dict(zip(PORT_COLS[1:], map(int, f[1:])))
    ev = None
    common = sorted(set(orig) & set(port))
    res = {c: dict(n=0, eq=0, first=None, maxd=0) for c in cols}
    for n in common:
        o, p = orig[n], port[n]
        if o['enemy_active'] or p['enemy_active']:
            ev = (n, o, p); break
        for c in cols:
            r = res[c]; r['n'] += 1; d = abs(o[c] - p[c])
            if d <= tol: r['eq'] += 1
            else:
                r['maxd'] = max(r['maxd'], d)
                if r['first'] is None: r['first'] = (n, o[c], p[c])
    last = n if ev else (common[-1] if common else 0)
    print(f'ticks comunes: {len(common)}; comparados hasta n={last - 1 if ev else last}' + (f'; evento aleatorio en n={ev[0]} (original: active={ev[1]["enemy_active"]} chasing={ev[1]["enemy_chasing"]} appr={ev[1]["enemy_approach_timer"]}; port: active={ev[2]["enemy_active"]} chasing={ev[2]["enemy_chasing"]} appr={ev[2]["enemy_approach_timer"]})' if ev else ''))
    bad = 0
    for c in cols:
        r = res[c]
        st = 'IGUAL' if r['eq'] == r['n'] else f'DIFIERE primera n={r["first"][0]} orig={r["first"][1]} port={r["first"][2]} (max dif {r["maxd"]})'
        bad += r['eq'] != r['n']
        print(f'  {c:18s} {r["eq"]}/{r["n"]}  {st}')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
