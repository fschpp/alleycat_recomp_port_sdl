#!/usr/bin/env python3
"""Lee el STATE.BIN de la cueva por iteracion (cave_iter.asm): un registro completo y despues deltas.

    python3 tools/orig_state/read_iter.py STATE.BIN cat.lst var1 var2 ...   # una fila por iteracion
    import read_iter; for n, tick, ds, changed in read_iter.records(path): ...   # ds se reutiliza (no guardar la referencia)

Cada registro: [contador:2][tick:2][nd:2]; nd == 0xFFFF -> ventana A + ventana B completas; si no, nd pares [offset:2][palabra:2]."""
import struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import read_state as rs

A_LEN, B_BASE, B_LEN = 0x2e40, 0x5900, 0x220      # ventanas de DS (cave_iter.asm)
IMG_LEN = B_BASE + B_LEN                           # la imagen se indexa por offset de DS (el hueco queda a 0)
REC_LEN = A_LEN + B_LEN


def records(path):
    d = open(path, 'rb').read(); i = 0; ds = bytearray(IMG_LEN)
    while i + 6 <= len(d):
        n, tick, nd = struct.unpack_from('<3H', d, i); i += 6
        if nd == 0xFFFF:
            if i + REC_LEN > len(d): break
            ds[:A_LEN] = d[i:i + A_LEN]; ds[B_BASE:B_BASE + B_LEN] = d[i + A_LEN:i + REC_LEN]; i += REC_LEN; changed = list(range(0, IMG_LEN, 2))
        else:
            if i + 4 * nd > len(d): break
            changed = []
            for k in range(nd):
                off, w = struct.unpack_from('<HH', d, i + 4 * k); ds[off:off + 2] = struct.pack('<H', w); changed.append(off)
            i += 4 * nd
        yield n, tick, ds, changed


if __name__ == '__main__':
    if len(sys.argv) < 4: sys.exit(__doc__)
    m = rs.build_map(sys.argv[2]); names = sys.argv[3:]
    print('n tick ' + ' '.join(names))
    for n, tick, ds, ch in records(sys.argv[1]):
        print(n, tick, *[rs.get(m, v, ds) for v in names])
