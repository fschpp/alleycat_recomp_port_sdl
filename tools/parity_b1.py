#!/usr/bin/env python3
"""Paridad B1 (datos) contra el cat.exe original — ver PLAN_PARIDAD_1A1.md.

Comprueba, sin ejecutar nada:
  1. ds_pool (src/gen_ds_pool.c) == segmento de datos del .exe, byte a byte.
  2. Cada tabla/sprite extraido aparte (src/gen_*.c) aparece como bloque contiguo en ese segmento.
  3. (opcional, --asm DIR) las dimensiones (ancho,alto) de las llamadas a blit del port que usan
     &ds_pool[...] coinciden con el `mov cx,0xHHWW` del ASM original (clon de gmegidish/alleycat-disassembly).
     Cobertura parcial: solo llamadas con direccion y dimensiones resolubles estaticamente.

Uso: python3 tools/parity_b1.py /ruta/cat.exe [--asm /ruta/alleycat-disassembly/src]
El .exe es material del juego original: NO se versiona. Sale con codigo 1 si hay diferencias."""
import argparse, glob, os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DS_FILE_BASE = 0x300          # offset de archivo del segmento de datos en este cat.exe (55067 bytes)


def load_pool():
    c = open(os.path.join(ROOT, 'src/gen_ds_pool.c')).read()
    body = c[c.index('{', c.index('ds_pool[DS_POOL_SIZE]')):]
    return bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', body))


def check_relocs(exe, lo, hi):
    h = struct.unpack('<14H', exe[:28]); bad = []
    for i in range(h[3]):
        o, s = struct.unpack('<HH', exe[h[12] + 4 * i: h[12] + 4 * i + 4])
        fo = 0x200 + s * 16 + o
        if lo <= fo < hi: bad.append(fo)
    return bad


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('exe'); ap.add_argument('--asm')
    a = ap.parse_args()
    exe = open(a.exe, 'rb').read(); fails = 0
    pool = load_pool(); seg = exe[DS_FILE_BASE: DS_FILE_BASE + len(pool)]
    if len(exe) != 55067: print(f'AVISO: el .exe mide {len(exe)} bytes (se probo con 55067); revisa DS_FILE_BASE')
    rel = check_relocs(exe, DS_FILE_BASE, DS_FILE_BASE + len(pool))
    print(f'[1] relocaciones MZ dentro del segmento de datos: {len(rel)}')
    diffs = [i for i in range(len(pool)) if i >= len(seg) or seg[i] != pool[i]]
    print(f'[1] ds_pool vs .exe: {len(pool)} bytes comparados, {len(diffs)} diferentes')
    for i in diffs[:20]: print(f'      0x{i:04x}: exe={seg[i]:02x} pool={pool[i]:02x}')
    fails += len(diffs)
    tot = 0; miss = []
    for f in sorted(glob.glob(os.path.join(ROOT, 'src/gen_*.c'))):
        if f.endswith('gen_ds_pool.c'): continue
        s = open(f).read()
        for m in re.finditer(r'const\s+uint8_t\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}', s):
            nums = re.findall(r'0[xX][0-9a-fA-F]+|\d+', m.group(2))
            if not nums: continue
            arr = bytes(int(n, 16) if n.lower().startswith('0x') else int(n) for n in nums); tot += 1
            if seg.count(arr) < 1: miss.append((os.path.basename(f), m.group(1), len(arr)))
    print(f'[2] tablas gen_*.c: {tot} analizadas, {len(miss)} ausentes del segmento de datos')
    for x in miss: print('      FALTA', x)
    fails += len(miss)
    if a.asm:
        asm = {}
        for f in glob.glob(os.path.join(a.asm, '*.asm')):
            L = open(f).read().split('\n')
            for i, l in enumerate(L):
                if not re.search(r'call\s+(blit_\w+|save_from_cga|restore_to_cga)', l): continue
                si = cx = None
                for w in reversed(L[max(0, i - 8):i]):
                    if si is None:
                        q = re.search(r'mov\s+si,\s*(?:dat_|0x)([0-9a-fA-F]{4})\b', w)
                        if q: si = int(q.group(1), 16)
                    if cx is None:
                        q = re.search(r'mov\s+cx,\s*0x([0-9a-fA-F]{3,4})\b', w)
                        if q: cx = int(q.group(1), 16)
                if si is not None and cx is not None: asm.setdefault(si, set()).add((cx & 0xff, cx >> 8))
        defs = {}
        for f in glob.glob(os.path.join(ROOT, 'src/*.c')) + glob.glob(os.path.join(ROOT, 'include/*.h')):
            for m in re.finditer(r'#define\s+(\w+)\s+(0x[0-9a-fA-F]+)', open(f).read()): defs[m.group(1)] = int(m.group(2), 16)
        v = lambda t: int(t, 16) if t.strip().startswith('0x') else (int(t) if t.strip().isdigit() else defs.get(t.strip()))
        n = res = bad = 0
        for f in glob.glob(os.path.join(ROOT, 'src/*.c')):
            s = open(f).read()
            for m in re.finditer(r'(blit_to_cga|blit_transparent|save_from_cga)\s*\(\s*(?:\(const uint8_t \*\))?&ds_pool\[([^\]]+)\]\s*,\s*[^,]+,\s*([^,\)]+),\s*([^,\)]+)', s):
                n += 1; ad, w, h = v(m.group(2)), v(m.group(3)), v(m.group(4))
                if None in (ad, w, h) or ad not in asm: continue
                res += 1
                if (w, h) not in asm[ad]:
                    bad += 1; print(f'      DIMENSIONES DISTINTAS {os.path.basename(f)} 0x{ad:04x} port={(w, h)} asm={sorted(asm[ad])}')
        print(f'[3] llamadas con ds_pool[...]: {n}; comprobables contra el ASM: {res}; discrepancias: {bad}')
        fails += bad
    print('RESULTADO:', 'OK' if not fails else f'{fails} diferencias'); return 1 if fails else 0


sys.exit(main())
