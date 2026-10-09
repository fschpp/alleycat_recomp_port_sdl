# Paridad 1 a 1: traza del original vs. traza del port (plan §3.4/§3.5)

Flujo (el `cat.exe` y los volcados NO se versionan):

    # 1. original con volcado por tick BIOS (ver tools/orig_state/README.md y docs/DOSBOX_SANDBOX.md)
    python3 tools/orig_state/build_dump_exe.py cat.exe /tmp/dos/catdump.exe
    nasm -f bin -l /tmp/dos/cat.lst -o /tmp/dos/junk.exe ../alleycat-disassembly/cat.asm 2>/dev/null   # solo interesa el listado
    tools/parity/orig_run.sh catdump.exe w 110 0.25 "space@6 n@14 k@22 space@30 down:Right@50 up:Right@62"
    # 2. el port, con la MISMA entrada que registro el original (guion sacado de STATE.BIN)
    tools/parity/build_port_trace.sh
    SC=$(python3 tools/parity/compare.py --script /tmp/dos/STATE.BIN /tmp/dos/cat.lst)
    PIT_SEED=1234 build/port_trace 140 100 "$SC" > /tmp/p.txt
    # 3. comparar
    python3 tools/parity/compare.py /tmp/dos/STATE.BIN /tmp/dos/cat.lst /tmp/p.txt --cols cat_x,cat_y,scroll_speed

* `port_trace` (sin SDL): `game_alley_frame` con reloj BIOS falso, ITERS iteraciones por tick (DOSBox hace ~100), azar
  fijado con `PIT_SEED`. Ojo: `cga_init()` siembra el azar con el reloj real; el arnes lo pisa para ser reproducible.
* `compare.py` corta en el primer tick con perro activo (`enemy_active != 0`) en cualquiera de los dos lados: el azar del
  original (`random` se llama en cada iteracion) no se puede alinear con el del arnes por tick.
* Para comparar el comportamiento del perro hace falta el diferencial por iteracion (ver PLAN §9, "Siguiente").
