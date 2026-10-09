# Volcado de estado del cat.exe original

Parchea una copia de `cat.exe` para que, una vez por tick BIOS, añada a `STATE.BIN` el segmento de datos del
juego (`DS:0000-1FFF`). Permite comparar variables del original con las del port (plan de paridad, §3.3).

    python3 tools/orig_state/build_dump_exe.py cat.exe catdump.exe
    # ejecutar catdump.exe en DOSBox (ver DOSBOX_SANDBOX.md); STATE.BIN aparece en el directorio montado
    nasm -f bin -l cat.lst -o cat_rebuilt.exe alleycat-disassembly/cat.asm 2>/dev/null
    python3 tools/orig_state/read_state.py STATE.BIN cat.lst cat_x cat_y lives_count

Limites: engancha el bucle del callejón (llama a `update_animation`); en niveles interiores solo hay registros
si el bucle pasa por ese punto. El `.exe` y los volcados no se versionan (material del juego original).
