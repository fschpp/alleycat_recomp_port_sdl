#!/usr/bin/env python3
"""check_niveles.py — T75: checklist de cableado por nivel.

Compara, para el callejon (nivel 0) y los niveles 1-7, la secuencia de `call` de
`entry.asm` (init, loop/update, flags de salida) contra las llamadas de
`src/game_flow.c`, aplicando la tabla de equivalencias de tareas.md §0.5.

Uso:
    tools/check_niveles.py            # imprime las diferencias; exit 1 si hay celdas vacias
    tools/check_niveles.py --emit     # imprime ademas las tablas markdown (docs/NIVELES.md)

ALLEYCAT_ASM apunta al clon de alleycat-disassembly (default ../alleycat-disassembly).
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASM_DIR = os.environ.get("ALLEYCAT_ASM", os.path.join(os.path.dirname(ROOT), "alleycat-disassembly"))
ENTRY = os.path.join(ASM_DIR, "src", "entry.asm")
GAME_FLOW = os.path.join(ROOT, "src", "game_flow.c")

# ASM -> C (tareas.md §0.5 + los nombres que game_flow.c documenta en su cabecera).
# None = el ASM lo llama pero no hace nada (documentado, no es una celda vacia).
MAP = {
    "process_keyboard": "input_process_keys",
    "cycle_animations": "update_cycle_objects",
    "init_objects": "init_cycle_objects",
    "draw_score": "draw_high_score_display",       # nombres cruzados en el original (score.h)
    "draw_high_score": "draw_current_score",
    "level6_stubs": None,                           # `ret` desnudo (level6.h, T29)
}
NOTE = {
    "level6_stubs": "`ret` desnudo (T29): no hay funcion C",
    "process_keyboard": "`keyboard.c: process_keyboard` la llama `input.c`/`input_process_keys` (T61)",
}

# Bloques de entry.asm: (etiqueta inicial, etiqueta del loop). El init va de la primera
# a la segunda; el loop de la segunda hasta la primera mencion de lab_0427 (salida).
LEVELS = {
    7: ("lab_0260", "lab_027e"),
    6: ("lab_02aa", "lab_02c5"),
    5: ("lab_02fe", "lab_0319"),
    4: ("lab_0349", "lab_0364"),
    3: ("lab_0394", "lab_03b2"),
    1: ("lab_03e2", "lab_03fa"),      # niveles 0 y 1 comparten bloque (jump table)
    2: ("lab_0459", "lab_0478"),
}
FLAG_RE = re.compile(r"^\s*(?:mov|or)\s+al,\[(\w+)\]")
CALL_RE = re.compile(r"^\s*call\s+(\w+)")


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read().split("\n")


def asm_label_line(lines, lab):
    for i, ln in enumerate(lines):
        if re.match(r"^%s:" % re.escape(lab), ln):
            return i
    raise SystemExit("etiqueta %s no encontrada en entry.asm" % lab)


def asm_calls(lines, a, b):
    out = []
    for i in range(a, b):
        m = CALL_RE.match(lines[i])
        if m:
            out.append((m.group(1), i + 1))
    return out


def asm_loop_end(lines, start):
    for i in range(start, len(lines)):
        if "lab_0427" in lines[i]:
            return i
    raise SystemExit("no hay salida lab_0427 tras L%d" % start)


def asm_flags(lines, a, b):
    out = []
    for i in range(a, b):
        m = FLAG_RE.match(lines[i])
        if m and m.group(1) not in out:
            out.append(m.group(1))
    return out


# ---------------------------------------------------------------- lado C
def c_text():
    with open(GAME_FLOW, encoding="utf-8") as f:
        return f.read()


def c_calls(chunk):
    names = []
    for m in re.finditer(r"(?<![\w.>])([A-Za-z_]\w*)\(\)", chunk):
        names.append(m.group(1))
    return names


def c_function_body(text, name):
    m = re.search(r"static void %s\(void\) \{(.*?)\n\}" % name, text, re.S)
    return m.group(1) if m else ""


def c_init_calls(text, lab):
    i = text.index("\n%s:" % lab)
    j = text.index("return true;", i)
    names = c_calls(text[i:j])
    out = []
    for n in names:
        if n == "level2_init":
            out += c_calls(c_function_body(text, "level2_init"))
        else:
            out.append(n)
    return out


def c_loop(text, lvl):
    case = {1: "case 1:", 7: "case 7:", 6: "case 6:", 5: "case 5:", 4: "case 4:", 3: "case 3:",
            2: "case 2:"}[lvl]
    i = text.index("gl_next_t game_level_frame(void)")
    i = text.index(case, i)
    nxt = [text.find(c, i + 1) for c in ("case 7:", "case 6:", "case 5:", "case 4:", "case 3:",
                                        "case 1:", "case 2:", "default:")]
    j = min(x for x in nxt if x != -1)
    chunk = text[i:j]
    calls = [n for n in c_calls(chunk) if n != "level2_init"]
    ms = re.findall(r"if \(\(([^()]*(?:\([^()]*\)[^()]*)*)\) == 0\)", chunk)
    flags = []
    if ms:
        for tok in re.split(r"\|", ms[-1]):
            tok = re.sub(r"\(uint8_t\)", "", tok).strip()
            if tok and tok not in flags:
                flags.append(tok)
    return calls, flags


def c_defs():
    defs = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "src", "*.c"))):
        with open(path, encoding="utf-8") as f:
            for n, ln in enumerate(f, 1):
                m = re.match(r"^(?:__attribute__\(\(weak\)\)\s+)?[A-Za-z_][\w \*]*?[ \*](\w+)\s*\([^;]*\)\s*\{", ln)
                if m:
                    weak = "weak" in ln
                    old = defs.get(m.group(1))
                    if old is None or (old[2] and not weak):      # la definicion fuerte gana a la debil
                        defs[m.group(1)] = (os.path.relpath(path, ROOT), n, weak)
    return defs


def expected(calls):
    out = []
    for name, ln in calls:
        c = MAP.get(name, name)
        out.append((name, c, ln))
    return out


def main():
    emit = "--emit" in sys.argv
    lines = read(ENTRY)
    text = c_text()
    defs = c_defs()
    problems = []

    def check(title, asm_list, c_list):
        exp = expected(asm_list)
        exp_c = [c for (_, c, _) in exp if c is not None]
        if exp_c != c_list:
            problems.append("%s: la secuencia no coincide\n   ASM->C: %s\n   C     : %s" % (title, exp_c, c_list))
        for (a, c, ln) in exp:
            if c is not None and c not in defs:
                problems.append("%s: `%s` (entry.asm L%d) no tiene definicion C (`%s`)" % (title, a, ln, c))
            if c is not None and c in defs and defs[c][2]:
                problems.append("%s: `%s` solo existe como definicion debil" % (title, c))
        return exp

    def table(exp):
        rows = ["| entry.asm | ASM | C | definicion |", "|---|---|---|---|"]
        for (a, c, ln) in exp:
            if c is None:
                rows.append("| L%d | `%s` | — (%s) | — |" % (ln, a, NOTE.get(a, "sin equivalente")))
            else:
                d = defs.get(c)
                where = "`%s:%d`" % (d[0], d[1]) if d else "**FALTA**"
                rows.append("| L%d | `%s` | `%s` | %s |" % (ln, a, c, where))
        return "\n".join(rows)

    out = []
    for lvl in (7, 6, 5, 4, 3, 1, 2):
        lab_i, lab_l = LEVELS[lvl]
        a = asm_label_line(lines, lab_i)
        b = asm_label_line(lines, lab_l)
        e = asm_loop_end(lines, b)
        exp_i = check("nivel %d init" % lvl, asm_calls(lines, a, b), c_init_calls(text, lab_i))
        c_l, c_f = c_loop(text, lvl)
        exp_l = check("nivel %d loop" % lvl, asm_calls(lines, b, e), c_l)
        fl = asm_flags(lines, b, e)
        if set(fl) != set(c_f):
            problems.append("nivel %d flags de salida: ASM %s / C %s" % (lvl, sorted(fl), sorted(c_f)))
        out.append((lvl, exp_i, exp_l, fl))

    # callejon (nivel 0): setup lab_00f3..lab_0155 y loop lab_0155..lab_01b7
    a = asm_label_line(lines, "lab_00f3")
    b = asm_label_line(lines, "lab_0155")
    e = asm_label_line(lines, "lab_01b7")
    c_setup = c_calls(text[text.index("\nlab_00f3:"):text.index("void game_start")])
    exp_s = check("callejon setup", asm_calls(lines, a, b), c_setup)
    body = text[text.index("gf_next_t game_alley_frame(void)"):text.index("static void level2_init")]
    exp_a = check("callejon loop", asm_calls(lines, b, e), [n for n in c_calls(body) if n != "game_death_handler"])

    if emit:
        print("# Checklist de cableado por nivel (T75)\n")
        print("Generado por `tools/check_niveles.py --emit` a partir de `entry.asm` (L%d-%d) y `src/game_flow.c`.\n" % (1, len(lines)))
        print("## Callejon (nivel 0)\n\n### Setup (lab_00f3..lab_0155)\n\n%s\n\n### Loop (lab_0155..lab_01b7)\n\n%s\n" % (table(exp_s), table(exp_a)))
        for lvl, ei, el, fl in out:
            nm = "1 (y 0 si se fuerza a mano)" if lvl == 1 else str(lvl)
            print("## Nivel %s\n\n### Init (%s)\n\n%s\n\n### Update (%s)\n\n%s\n\nFlags de salida: %s\n" % (
                nm, LEVELS[lvl][0], table(ei), LEVELS[lvl][1], table(el), ", ".join("`%s`" % f for f in fl)))

    if problems:
        print("\n".join(problems))
        print("\nRESULTADO: %d diferencia(s)/celda(s) vacia(s)" % len(problems))
        sys.exit(1)
    print("check_niveles: OK (callejon + niveles 1-7: ninguna celda vacia, secuencias iguales a entry.asm)")


if __name__ == "__main__":
    main()
