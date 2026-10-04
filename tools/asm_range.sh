#!/usr/bin/env bash
# asm_range.sh <archivo> <ini> <fin> — imprime un rango del ASM con números de línea.
# <archivo> es relativo a src/ del repo ASM (ej: level_physics.asm).
set -eu
[ $# -eq 3 ] || { echo "uso: $0 <archivo> <ini> <fin>" >&2; exit 2; }
ASM="${ALLEYCAT_ASM:-$(cd "$(dirname "$0")/../.." && pwd)/alleycat-disassembly}"
f="$ASM/src/$1"; [ -f "$f" ] || f="$ASM/$1"
sed -n "$2,$3p" "$f" | awk -v s="$2" '{printf "%5d  %s\n", s+NR-1, $0}'
