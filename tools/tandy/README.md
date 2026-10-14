# Versión de 3 voces (Tandy/PCjr) de Alley Cat

Nuestro `cat.exe` (55 067 B, CRC32 `791CE265`) NO tiene código de sonido de 3 voces (chip TI SN76489/76496, puerto 0xC0).
Existe otra compilación (CRC32 `6B65D8B3`, 56 624 B, "Pirated by Software Pirates, Inc") que sí lo tiene. El parche de NewRisingSun
(hilo de VCF "Tandy sound and video patch for Alley Cat", `alleycat_patch_nrs.zip`) trae `791CE265.DIF`, un bsdiff que convierte
nuestro `cat.exe` en esa compilación, y `TANDYCAT.DIF`, que la parchea para activar el sonido/vídeo Tandy.

`bspatch.py` es un bspatch (BSDIFF40) en Python puro, para hacerlo sin BSPATCH.EXE:

    python3 -I tools/tandy/bspatch.py cat.exe alleycat3v.exe 791CE265.DIF     # -> CRC32 6B65D8B3, 56 624 B
    python3 -I tools/tandy/bspatch.py alleycat3v.exe tandycat.exe TANDYCAT.DIF

No se versionan ejecutables ni los `.DIF` (no son nuestros). Ver PROGRESS.md §6cb.

Notas de ingeniería inversa de la compilación de 3 voces (imagen = archivo desde 0x200; CS = imagen, DS = imagen + 0x6740):
- motor de música: escrituras `out 0xC0` en CS:0x1732..0x1829 (silencio `0x9F`..`0xFF`, 3 voces con estado de 9 bytes en DS:0x1228
  copiado por voz desde la tabla DS:0x124c, notas en DS:0x1014); llamadores en CS:0x60d1, 0x6124, 0x613d, 0x6140.
- el código está desplazado respecto a nuestro desensamblado (≈2 600 bytes insertados y muchos cambios, 63 bloques en el bsdiff).
