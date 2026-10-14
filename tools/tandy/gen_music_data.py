#!/usr/bin/env python3
"""Genera src/gen_tandy_music.c: bytes DS:0x1010..0x1260 de la compilacion 3v (tabla de frecuencias, pistas, estado inicial de voces)."""
import sys
d = open(sys.argv[1], 'rb').read()[0x200:]
b = d[0x6740 + 0x1010: 0x6740 + 0x1260]
o = ['/* GENERADO por tools/tandy/gen_music_data.py desde la compilacion de 3 voces (DS:0x1010..0x125f). No editar. */',
     '#include <stdint.h>', 'const uint8_t tandy_ds_init[%d] = {' % len(b)]
for i in range(0, len(b), 16): o.append('    ' + ', '.join('0x%02x' % x for x in b[i:i+16]) + ',')
o.append('};'); print('\n'.join(o))
