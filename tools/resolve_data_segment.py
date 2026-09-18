#!/usr/bin/env python3
"""
resolve_data_segment.py — walks cat.asm's .data section (DS, 0x7130 bytes)
as ONE linear byte stream, following %include directives, recording every
label's DS-relative offset. This lets us resolve pointer-table entries
(like walk_sprite_ptrs' absolute DS offsets) to their real byte location,
instead of trusting data/*.asm's own (sometimes wrong) label boundaries.
"""
import re
import sys
import os

ROOT = "/home/claude/work/alleycat-disassembly/alleycat-disassembly-main"

LABEL_RE = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*):\s*$')
INCLUDE_RE = re.compile(r'%include\s+"([^"]+)"')
DB_RE = re.compile(r'\bdb\s+((?:0x[0-9a-fA-F]{2}\s*,?\s*)+)')
DW_RE = re.compile(r'\bdw\s+((?:0x[0-9a-fA-F]{1,4}\s*,?\s*)+)')
BYTE_RE = re.compile(r'0x([0-9a-fA-F]{2})')
WORD_RE = re.compile(r'0x([0-9a-fA-F]{1,4})')
TIMES_RE = re.compile(r'times\s+(\S+)\s+db\s+0')

def process_file(path, out_bytes, labels, base_dir):
    with open(path) as f:
        lines = f.readlines()
    for line in lines:
        stripped = line.strip()
        m = INCLUDE_RE.search(stripped)
        if m:
            inc_path = os.path.join(base_dir, m.group(1))
            process_file(inc_path, out_bytes, labels, base_dir)
            continue
        m = LABEL_RE.match(stripped)
        if m:
            labels.setdefault(m.group(1), len(out_bytes))
            continue
        m = DB_RE.search(stripped)
        if m:
            for b in BYTE_RE.findall(m.group(1)):
                out_bytes.append(int(b, 16))
            continue
        m = DW_RE.search(stripped)
        if m and 'db' not in stripped.split(';')[0].split('dw')[0]:
            for w in WORD_RE.findall(m.group(1)):
                val = int(w, 16)
                out_bytes.append(val & 0xFF)
                out_bytes.append((val >> 8) & 0xFF)
            continue
        m = TIMES_RE.search(stripped)
        if m:
            # only used for header padding, not present in .data section,
            # but handle defensively
            pass

def main():
    out_bytes = bytearray()
    labels = {}
    # only process from data_start: to code_start: — do this by slicing
    # cat.asm itself first (data/*.asm are then pulled in via %include)
    with open(os.path.join(ROOT, "cat.asm")) as f:
        full = f.readlines()
    start = next(i for i, l in enumerate(full) if l.strip() == "data_start:")
    end = next(i for i, l in enumerate(full) if l.strip() == "code_start:")
    data_slice = full[start:end]

    tmp_path = "/tmp/_cat_data_slice.asm"
    with open(tmp_path, "w") as f:
        f.writelines(data_slice)

    process_file(tmp_path, out_bytes, labels, os.path.join(ROOT))

    print(f"Total data segment bytes reconstructed: {len(out_bytes)} (expected 0x7130 = {0x7130})")
    print(f"Total labels found: {len(labels)}")

    # Save for reuse
    with open("/tmp/data_segment.bin", "wb") as f:
        f.write(out_bytes)
    with open("/tmp/data_segment_labels.txt", "w") as f:
        for name, off in sorted(labels.items(), key=lambda kv: kv[1]):
            f.write(f"{off:#06x}\t{name}\n")

    return out_bytes, labels

if __name__ == "__main__":
    main()
