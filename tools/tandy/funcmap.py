#!/usr/bin/env python3
"""Mapa de etiquetas de CODIGO de nuestro cat.exe -> offset de CS en la compilacion de 3 voces (6B65D8B3), a partir del bsdiff 791CE265.DIF.
Las direcciones del listado son offsets de ARCHIVO de nuestro cat.exe (cabecera MZ incluida). 3v_CS = offset de archivo - 0x200.
Uso: funcmap.py cat.lst 791CE265.DIF [regex]   (imprime  etiqueta  cs_nuestro  cs_3v  [bytes iguales?])
Los bloques 'copia' del bsdiff dicen que new[np..np+x) ~ old[op..op+x); las direcciones absolutas relocalizadas difieren en unos bytes."""
import bz2, struct, sys, re
def offtin(b):
    y = int.from_bytes(b, 'little')
    return -(y & ~(1 << 63)) if y & (1 << 63) else y
def blocks(dif):
    pat = open(dif, 'rb').read(); cl, dl, nl = struct.unpack('<qqq', pat[8:32]); ctrl = bz2.decompress(pat[32:32 + cl])
    np = op = cp = 0; m = []
    while np < nl:
        x, y, z = (offtin(ctrl[cp + 8 * i:cp + 8 * i + 8]) for i in range(3)); cp += 24
        if x: m.append((op, np, x))
        np += x + y; op += x + z
    return m
def code_labels(lst):
    """Etiquetas de CODIGO: las que siguen a `code_start:` en el listado (seccion .code, vstart=0 -> offset de CS; offset de archivo = 0x7430 + CS (cs=0x723 parrafos tras la cabecera))."""
    out, pend, incode = {}, None, False
    for l in open(lst).read().split('\n'):
        m = re.match(r'\s*\d+\s+(?:<\d+>\s+)?([A-Za-z_]\w*):\s*$', l)
        if m:
            pend = m.group(1)
            if pend == 'code_start': incode = True
            continue
        m = re.match(r'\s*\d+\s+([0-9A-F]{8})\s+[0-9A-F]', l)
        if m and pend:
            if incode: out[pend] = int(m.group(1), 16)
            pend = None
    return out
if __name__ == '__main__':
    lst, dif = sys.argv[1], sys.argv[2]; rx = re.compile(sys.argv[3]) if len(sys.argv) > 3 else None
    m = blocks(dif)
    def o2n(o):
        for a, b, x in m:
            if a <= o < a + x: return b + (o - a)
    for n, a in sorted(code_labels(lst).items(), key=lambda t: t[1]):
        if rx and not rx.search(n): continue
        r = o2n(0x7430 + a)
        print('%-28s CS %04x  3v_archivo %s  3v_CS %s' % (n, a, ('%05x' % r) if r is not None else '--', ('%04x' % (r - 0x200)) if r is not None else '--'))
