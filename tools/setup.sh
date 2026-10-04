#!/usr/bin/env bash
# setup.sh — prepara el entorno de trabajo del port Alley Cat (T00).
# Uso: tools/setup.sh   (desde la raíz del port; clona el ASM como hermano si falta)
set -u
PORT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PARENT="$(dirname "$PORT_DIR")"
export ALLEYCAT_ASM="${ALLEYCAT_ASM:-$PARENT/alleycat-disassembly}"

if [ ! -f "$ALLEYCAT_ASM/cat.asm" ]; then
  echo "[setup] clonando alleycat-disassembly en $ALLEYCAT_ASM"
  git clone --depth 1 https://github.com/gmegidish/alleycat-disassembly.git "$ALLEYCAT_ASM" || exit 1
fi

echo "[setup] ALLEYCAT_ASM=$ALLEYCAT_ASM"
python3 "$PORT_DIR/tools/resolve_data_segment.py" || exit 1

if ! pkg-config --exists sdl2 2>/dev/null; then
  echo "[setup] intentando instalar libsdl2-dev..."
  if ! (apt-get install -y libsdl2-dev >/dev/null 2>&1 || sudo apt-get install -y libsdl2-dev >/dev/null 2>&1); then
    echo "[setup] AVISO: no se pudo instalar libsdl2-dev; usa 'cc -fsyntax-only -Iinclude src/<f>.c' para validar."
  fi
fi
echo "[setup] listo. Exporta: export ALLEYCAT_ASM=$ALLEYCAT_ASM"
