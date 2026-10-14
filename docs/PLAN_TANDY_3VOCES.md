# Plan: música de 3 voces (Tandy/PCjr) idéntica al original

Objetivo (pedido de Pepepompin, 2026-10-13): que `build/alleycat --tandy` reproduzca **la música de 3 voces del original**, no una aproximación.
Fuente de verdad: la compilación de 3 voces del juego (CRC32 `6B65D8B3`, 56 624 B) que se obtiene de nuestro `cat.exe` (CRC32 `791CE265`)
con `791CE265.DIF` del parche de NewRisingSun (`tools/tandy/`), y el parche TANDYCAT (`TANDYCAT.DIF`) que la activa.

## Hallazgos que fijan el alcance (2026-10-13)
- La compilación 3v es otra build, no un parche pequeño (100 bloques de diferencias en el código tras normalizar direcciones).
  Layout distinto: 3v = código en CS=base (CS:0) y datos en DS = CS + 0x6740; la nuestra = datos en DS=base, código en CS=base+0x7230.
- **Todo el bloque `sound.asm` es idéntico** (sonidos de altavoz sin cambios; solo cambia la carga de DS y un par de direcciones).
  Lo nuevo de sonido es: (a) un motor de 3 voces en CS:0x16a0-0x1830 (`out 0xC0`), (b) los ganchos que lo llaman en el bucle del
  título/atracción (CS:0x6044-0x6149, 0x6174), (c) la detección/pregunta de sonido externo del PCjr (CS:0x61f8-0x622d),
  (d) llamadas `INT 1Ah AX=8000h/8003h` (multiplexor de sonido de la BIOS Tandy) en CS:0x170c/0x171f.
- El resto de diferencias (calibración de joystick, teclado Tandy, `mov es,[cs:0xb]`…) no son de sonido: **no se portan** salvo que afecten al sonido.

## Fases
- **F0 Herramientas** (hecho): `tools/tandy/bspatch.py`, `make_lst.sh` (listado NASM completo), `funcmap.py` (mapa etiqueta nuestra -> CS 3v).
- **F1 Desensamblado del motor 3v**: anotar CS:0x1697-0x1830 y 0x6044-0x6240 (estado de voz de 9 B en DS:0x1228, tablas DS:0x124c/0x1014,
  datos de la melodía), qué variables DS usa, y el significado de DS:[0] / DS:[1]. Salida: `docs/TANDY_MOTOR.md`.
- **F2 Oráculo**: ejecutar el código 3v real en un emulador x86-16 (unicorn) con `INT 1Ah`/`INT 10h`/puertos simulados y un reloj determinista;
  registrar cada `out 0xC0` (valor + tick) y cada cambio de PIT. Es la referencia contra la que se compara el port.
- **F3 Emulador del chip** `src/sn76496.c` (3 tonos + ruido, atenuación 2 dB/paso, divisor de 10 bits, reloj 3,579545 MHz/32) + mezcla en `audio.c`
  junto al altavoz. Prueba unitaria contra valores conocidos del SN76496.
- **F4 Motor en C** `src/tandy_music.c`: traducción literal de F1 (init, tick por voz, silencio, PIT de ritmo), con los datos de música
  extraídos del DS 3v (generador `gen_tandy_music.c`, igual que `gen_ds_pool.c`).
- **F5 Cableado**: en `--tandy`, el bucle de título/atracción llama al motor en vez de `play_music_note` (como el 3v); pregunta de sonido externo
  del PCjr; multiplexor `INT 1Ah` como no-op documentado.
- **F6 Verificación**: secuencia de escrituras `(tick, valor)` del port == la del oráculo para toda la melodía (título y atracción), 0 diferencias.
  Prueba en `tests/test_tandy_music.c`. Después, escucharlo en PC real (lo hace Pepepompin).

## Fuera de alcance / decisiones
- No se versionan ejecutables ni `.DIF`. Sin sonido real de la tarjeta PS/1 ni de los chips en `0x1E0` (la 3v base solo usa `0xC0`; el parche de NewRisingSun
  añade detección de puerto: se evalúa después de F6).
- El resto de la lógica del juego sigue siendo la de nuestra build (la paridad con `cat.exe` ya verificada).

## Estado (2026-10-14)
F1 (docs/TANDY_MOTOR.md), F2 (tools/tandy/oracle.py + make_trace.py), F3 (src/sn76496.c, mezcla en audio.c), F4 (src/tandy_music.c, datos generados), F5 (bucle del titulo bajo `--tandy`) y F6 (`make build/test_tandy_music`, 0 diferencias, 10632 escrituras) hechos. Pendiente: escucha en PC real y los puntos de PROGRESS.md.
