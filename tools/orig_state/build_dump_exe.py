#!/usr/bin/env python3
"""Genera una copia de cat.exe que vuelca el segmento de datos del juego a STATE.BIN (en el directorio
actual de DOS) una vez por tick BIOS. Ver PLAN_PARIDAD_1A1.md §3.3.

Uso: python3 tools/orig_state/build_dump_exe.py /ruta/cat.exe /ruta/salida/catdump.exe [--iter [TICKS [MAXREC [TRIG]]]] [--video] [--interior] [--start-level N]
  sin opcion : un registro por tick BIOS (cave.asm).
  --iter     : un registro (delta) por ITERACION del bucle desde la iteracion anterior a la aparicion del perro, durante
               TICKS ticks BIOS (por defecto 40) o MAXREC registros (por defecto 30000) (cave_iter.asm; se lee con
               read_iter.py; diferencial por iteracion, plan §9).
               TRIG = direccion DS (decimal o 0x..) del byte cuyo primer valor != 0 dispara el registro (por defecto 0x1cb8 =
               enemy_active; 0x6b8 = key_up, 0x6b9 = key_right: graba saltos/ventanas con teclas, plan §9 B4.12/13).
  --video    : (solo con --iter) registra tambien la RAM de video CGA B800:0000..3FFF (N4: comparar imagenes, plan §9); el formato de
               STATE.BIN cambia (ver cave_iter.asm) y se lee con read_iter.py --video / iter_diff --video.
  --interior : (solo con --iter; implica registrar TODO DS:0000..712F en una ventana, `iter_diff --level`) engancha la llamada a play_sound de los 7 bucles de nivel interior (en vez del bucle del callejon): una pasada
               del original = game_level_frame() del port (plan §9 "bucles interiores"). Usar TRIG = level_number (DS) o cualquier byte != 0.
  --start-level N : (N = 1..7) el juego arranca directo en el nivel N (y vuelve a entrar en el al salir): parchea lab_00f3 -> lab_01b7, fuerza la ruta
               force_level7 y apunta la entrada 7 de la tabla de saltos al manejador del nivel N (DOSBOX_SANDBOX.md §5.1).
Requiere nasm. Sirve para el cat.exe de 55067 bytes. El .exe original NO se versiona."""
import os, re, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CS_FILE_BASE = 0x7430                    # offset de archivo de CS:0000 (header 0x200 + CS 0x723 * 16)
SITE_PAT = re.compile(rb'\xe8..\xe8..\xe8..\x80\x3e..\x00\x75.\xfe\x06..\xf6\x06..\x03\x75', re.S)


def main(src, dst, iter_mode=False, ticklim=40, maxrec=30000, trig=0x1cb8, video=False, interior=False, start_level=0):
    d = bytearray(open(src, 'rb').read())
    if len(d) != 55067: sys.exit(f'cat.exe inesperado ({len(d)} bytes, se esperaban 55067)')
    if interior:
        # los 7 bucles de nivel: call process_keyboard / call poll_joystick / call play_sound (tres e8 seguidos con los mismos destinos absolutos)
        trip = {}
        for q in range(len(d) - 9):
            if d[q] == 0xe8 and d[q + 3] == 0xe8 and d[q + 6] == 0xe8:
                t = tuple(q + 3 * k - CS_FILE_BASE + 3 + struct.unpack('<h', d[q + 3 * k + 1:q + 3 * k + 3])[0] for k in range(3))
                trip.setdefault(t, []).append(q)
        c = [(t, v) for t, v in trip.items() if len(v) == 7 and t[1] == 0x1200 and t[0] == 0x1338]   # process_keyboard=1338, poll_joystick=1200
        if len(c) != 1: sys.exit(f'bucles de nivel: {len(c)} candidatos (se esperaba 1 con 7 sitios)')
        sites_p = [q + 6 for q in c[0][1]]; target = c[0][0][2]            # 3.er call = play_sound
        p = sites_p[0]; site = p - CS_FILE_BASE
    else:
        ms = list(SITE_PAT.finditer(bytes(d)))
        if len(ms) != 1: sys.exit(f'patron del bucle principal: {len(ms)} coincidencias (se esperaba 1)')
        p = ms[0].start() + 3                # 2.o call del bucle del callejon = update_animation
        sites_p = [p]; site = p - CS_FILE_BASE
        target = site + 3 + struct.unpack('<h', d[p + 1:p + 3])[0]
    cave_cs = len(d) - CS_FILE_BASE
    asm = open(os.path.join(HERE, 'cave_iter.asm' if iter_mode else 'cave.asm')).read().replace('CAVE_ORG', hex(cave_cs)).replace('ORIG_TARGET', hex(target)).replace('CAVE_MAXREC', str(maxrec)).replace('CAVE_TICKLIM', str(ticklim)).replace('CAVE_TRIG', hex(trig))
    with tempfile.TemporaryDirectory() as t:
        if video: asm = '%define VIDEO\n' + asm
        if interior: asm = '%define INTERIOR\n%define FULLDS\n' + asm
        open(f'{t}/c.asm', 'w').write(asm)
        subprocess.check_call(['nasm', '-f', 'bin', f'{t}/c.asm', '-o', f'{t}/c.bin'])
        cave = open(f'{t}/c.bin', 'rb').read()
    for q in sites_p: d[q + 1:q + 3] = struct.pack('<h', cave_cs - (q - CS_FILE_BASE + 3))
    if start_level:
        base = CS_FILE_BASE
        handlers = {1: 0x3e2, 2: 0x459, 3: 0x394, 4: 0x349, 5: 0x2fe, 6: 0x2aa, 7: 0x260}
        tab = bytes.fromhex('e203e2035904940349' '03fe02aa026002')
        t = bytes(d).find(tab)
        if t < 0 or bytes(d).count(tab) != 1: sys.exit('tabla de saltos de niveles no encontrada')
        d[t + 14:t + 16] = struct.pack('<H', handlers[start_level])             # entrada del nivel 7 -> manejador del nivel N
        a1 = base + 0x1b7
        f = bytes(d).find(bytes.fromhex('803e1804' '0074 0e'.replace(' ', '')), a1)
        if f < 0 or f - a1 > 64: sys.exit('cmp [force_level7] no encontrado')
        d[f + 5:f + 7] = b'\x90\x90'                                            # siempre la ruta force_level7
        d[base + 0xf3:base + 0xf3 + 3] = b'\xe9' + struct.pack('<h', 0x1b7 - (0xf3 + 3))
    d += cave
    d[2:4] = struct.pack('<H', len(d) % 512); d[4:6] = struct.pack('<H', (len(d) + 511) // 512)
    open(dst, 'wb').write(d)
    print(f'{dst}{" (con video)" if video else ""}{" (interior)" if interior else ""}{f" (nivel {start_level})" if start_level else ""}: {len(sites_p)} enganche(s), primero cs:{site:04x} -> cs:{target:04x}; cueva cs:{cave_cs:04x} ({len(cave)} B)')


if __name__ == '__main__':
    a = sys.argv[1:]
    video = '--video' in a; a = [x for x in a if x != '--video']
    interior = '--interior' in a; a = [x for x in a if x != '--interior']
    start_level = 0
    if '--start-level' in a:
        i = a.index('--start-level'); start_level = int(a[i + 1]); del a[i:i + 2]
    if (video or interior) and len(a) < 3: sys.exit('--video/--interior requieren --iter')
    if len(a) < 2 or (len(a) > 2 and a[2] != '--iter'): sys.exit(__doc__)
    main(a[0], a[1], len(a) > 2, int(a[3]) if len(a) > 3 else 40, int(a[4]) if len(a) > 4 else 30000, int(a[5], 0) if len(a) > 5 else 0x1cb8, video, interior, start_level)
