#!/usr/bin/env python3
"""Lee STATE.BIN (volcado de catdump.exe) y lo expone por nombre de variable.

Mapa de variables: se obtiene del listado de NASM del desensamblado (gmegidish/alleycat-disassembly):
    nasm -f bin -l cat.lst -o cat_rebuilt.exe cat.asm     # nasm reciente da 3 errores de tamano de operando; el listado se genera igual (cat_rebuilt.exe queda vacio)
    python3 tools/orig_state/read_state.py STATE.BIN cat.lst cat_x cat_y lives_count

Cada registro: [contador:2][tick:2][DS:0000..1FFF]. Las variables viven en DS:0000-1FFF (p. ej. lives_count=0x1f80)."""
import json, re, sys

REC = 4 + 0x2000


def build_map(lst_path):
    labels, pend = [], None
    for l in open(lst_path).read().split('\n'):
        m = re.match(r'\s*\d+\s+([A-Za-z_]\w*):\s*$', l)
        if m: pend = m.group(1); continue
        m = re.match(r'\s*\d+\s+([0-9A-F]{8})\s+[0-9A-F]', l)
        if m and pend:
            a = int(m.group(1), 16)
            if a < 0x7130: labels.append((pend, a))
            pend = None
    labels.sort(key=lambda x: x[1])
    return {n: (a, (labels[i + 1][1] if i + 1 < len(labels) else 0x7130) - a) for i, (n, a) in enumerate(labels)}


def records(path):
    d = open(path, 'rb').read()
    for i in range(len(d) // REC):
        yield (d[i * REC] | d[i * REC + 1] << 8, d[i * REC + 2] | d[i * REC + 3] << 8, d[i * REC + 4:(i + 1) * REC])


def get(m, name, ds):
    a, s = m[name]; s = 1 if s == 1 else 2
    return int.from_bytes(ds[a:a + s], 'little')


if __name__ == '__main__':
    if len(sys.argv) < 4: sys.exit(__doc__)
    m = build_map(sys.argv[2]); names = sys.argv[3:]
    print('n tick ' + ' '.join(names))
    for n, tick, ds in records(sys.argv[1]):
        print(n, tick, *[get(m, v, ds) for v in names])
