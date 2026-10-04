#!/usr/bin/env bash
# asm_label.sh <label> — offset DS de un label (usa /tmp/data_segment_labels.txt).
set -eu
[ $# -eq 1 ] || { echo "uso: $0 <label>" >&2; exit 2; }
grep -w -- "$1" /tmp/data_segment_labels.txt
