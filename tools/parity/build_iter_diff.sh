#!/usr/bin/env bash
# Compila build/iter_diff (diferencial por iteracion). Desde la raiz del port. Necesita el listado de NASM del desensamblado:
#   nasm -f bin -l /tmp/dos/cat.lst -o /tmp/dos/junk.exe <alleycat-disassembly>/cat.asm 2>/dev/null
# (con nasm reciente el .exe sale vacio pero el listado se genera; ver tools/orig_state/README.md)
# Uso: tools/parity/build_iter_diff.sh /tmp/dos/cat.lst
set -e
LST=${1:?uso: build_iter_diff.sh cat.lst}
mkdir -p build/iter_obj
SRCS=$(ls src/*.c | grep -v -E 'src/(main|video|audio|input)\.c')
CFLAGS="${EXTRA_CFLAGS} -O1 -D_POSIX_C_SOURCE=199309L -Iinclude -Isrc -Itools/parity"
for f in $SRCS; do cc $CFLAGS -c "$f" -o "build/iter_obj/$(basename "$f" .c).o"; done
nm -S --defined-only build/iter_obj/*.o > build/iter_obj/nm.txt
WRAP=--wrap=speaker_spin_cycles,--wrap=read_pit_timer,--wrap=read_bios_tick,--wrap=poll_joystick,--wrap=spawn_window_event,--wrap=blit_to_cga,--wrap=blit_masked,--wrap=blit_transparent,--wrap=save_from_cga
# 1) callejon (ventanas A+B): build/iter_diff
python3 tools/parity/gen_iter_map.py "$LST" build/iter_obj/nm.txt build/iter_map.c
cc $CFLAGS -c build/iter_map.c -o build/iter_obj/zz_iter_map.o -Wno-builtin-declaration-mismatch
cc $CFLAGS -no-pie -Wl,$WRAP tools/parity/iter_diff.c build/iter_obj/*.o -o build/iter_diff
echo "build/iter_diff listo"
# 2) niveles interiores (STATE.BIN de --interior: todo DS): build/iter_diff_full
python3 tools/parity/gen_iter_map.py "$LST" build/iter_obj/nm.txt build/iter_map_full.c --full
cc $CFLAGS -c build/iter_map_full.c -o build/iter_obj/zz_iter_map.o -Wno-builtin-declaration-mismatch
cc $CFLAGS -DFULLDS -no-pie -Wl,$WRAP tools/parity/iter_diff.c build/iter_obj/*.o -o build/iter_diff_full
cc $CFLAGS -c build/iter_map.c -o build/iter_obj/zz_iter_map.o -Wno-builtin-declaration-mismatch
echo "build/iter_diff_full listo"
