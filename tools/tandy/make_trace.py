#!/usr/bin/env python3
"""Genera la traza de referencia (N ticks) ejecutando el motor original. Uso: make_trace.py exe N > trace.txt"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from oracle import Oracle
o = Oracle(sys.argv[1]); N = int(sys.argv[2]); o.ds_write(0, b'\xff'); o.ds_write(1, b'\x01')
o.call(0x16a0); rows = o.take()
for t in range(1, N + 1):
    o.tick = t; o.call(0x173e); rows += o.take()
print(N)
for k, a, b, t in rows:
    if k == 'out': print(t, a, b)
