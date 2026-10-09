#!/usr/bin/env bash
# Compila build/port_trace (traza del port por tick BIOS simulado, sin SDL). Desde la raiz del port.
set -e
mkdir -p build
cc -O1 -D_POSIX_C_SOURCE=199309L -Iinclude -Wl,--wrap=speaker_spin_cycles,--wrap=read_bios_tick \
   tools/parity/port_trace.c $(ls src/*.c | grep -v -E 'src/(main|video|audio|input)\.c') -o build/port_trace
