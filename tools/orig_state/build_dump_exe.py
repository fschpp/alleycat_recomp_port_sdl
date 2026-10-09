#!/usr/bin/env python3
"""Genera una copia de cat.exe que vuelca el segmento de datos del juego a STATE.BIN (en el directorio
actual de DOS) una vez por tick BIOS. Ver PLAN_PARIDAD_1A1.md §3.3.

Uso: python3 tools/orig_state/build_dump_exe.py /ruta/cat.exe /ruta/salida/catdump.exe [--iter [TICKS [MAXREC]]]
  sin opcion : un registro por tick BIOS (cave.asm).
  --iter     : un registro (delta) por ITERACION del bucle desde la iteracion anterior a la aparicion del perro, durante
               TICKS ticks BIOS (por defecto 40) o MAXREC registros (por defecto 30000) (cave_iter.asm; se lee con
               read_iter.py; diferencial por iteracion, plan §9).
Requiere nasm. Sirve para el cat.exe de 55067 bytes. El .exe original NO se versiona."""
import os, re, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CS_FILE_BASE = 0x7430                    # offset de archivo de CS:0000 (header 0x200 + CS 0x723 * 16)
SITE_PAT = re.compile(rb'\xe8..\xe8..\xe8..\x80\x3e..\x00\x75.\xfe\x06..\xf6\x06..\x03\x75', re.S)


def main(src, dst, iter_mode=False, ticklim=40, maxrec=30000):
    d = bytearray(open(src, 'rb').read())
    if len(d) != 55067: sys.exit(f'cat.exe inesperado ({len(d)} bytes, se esperaban 55067)')
    ms = list(SITE_PAT.finditer(bytes(d)))
    if len(ms) != 1: sys.exit(f'patron del bucle principal: {len(ms)} coincidencias (se esperaba 1)')
    p = ms[0].start() + 3                # 2.o call del bucle del callejon = update_animation
    site = p - CS_FILE_BASE
    target = site + 3 + struct.unpack('<h', d[p + 1:p + 3])[0]
    cave_cs = len(d) - CS_FILE_BASE
    asm = open(os.path.join(HERE, 'cave_iter.asm' if iter_mode else 'cave.asm')).read().replace('CAVE_ORG', hex(cave_cs)).replace('ORIG_TARGET', hex(target)).replace('CAVE_MAXREC', str(maxrec)).replace('CAVE_TICKLIM', str(ticklim))
    with tempfile.TemporaryDirectory() as t:
        open(f'{t}/c.asm', 'w').write(asm)
        subprocess.check_call(['nasm', '-f', 'bin', f'{t}/c.asm', '-o', f'{t}/c.bin'])
        cave = open(f'{t}/c.bin', 'rb').read()
    d[p + 1:p + 3] = struct.pack('<h', cave_cs - (site + 3))
    d += cave
    d[2:4] = struct.pack('<H', len(d) % 512); d[4:6] = struct.pack('<H', (len(d) + 511) // 512)
    open(dst, 'wb').write(d)
    print(f'{dst}: enganche cs:{site:04x} -> update_animation cs:{target:04x}; cueva cs:{cave_cs:04x} ({len(cave)} B)')


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) < 2 or (len(a) > 2 and a[2] != '--iter'): sys.exit(__doc__)
    main(a[0], a[1], len(a) > 2, int(a[3]) if len(a) > 3 else 40, int(a[4]) if len(a) > 4 else 30000)
