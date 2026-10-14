#!/usr/bin/env bash
# Genera el listado COMPLETO de NASM del desensamblado (nasm 2.16 corta el listado en los `loop short` y el jmp far).
# Uso: make_lst.sh <alleycat-disassembly> <salida_dir>   -> <salida_dir>/cat_full.lst y rebuilt.exe (md5 3e80bb5c82acd6b6f7cf6d414b745a8e = cat.exe)
set -e
SRC=${1:?uso: make_lst.sh alleycat-disassembly salida_dir}; OUT=${2:?}; mkdir -p "$OUT"; rm -rf "$OUT/dis"; cp -r "$SRC" "$OUT/dis"
cd "$OUT/dis"
sed -i 's/\bloop short /loop /; s/\bloope short /loope /; s/\bloopne short /loopne /; s/jmp word 0xf000:word 0xe05b/db 0xea,0x5b,0xe0,0x00,0xf0/' src/*.asm
nasm -f bin -l "$OUT/cat_full.lst" -o "$OUT/rebuilt.exe" cat.asm
md5sum "$OUT/rebuilt.exe"
