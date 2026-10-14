# Motor de musica de 3 voces (Tandy/PCjr, SN76496)

Fuente: compilacion `alleycat61568.exe` (CRC32 6B65D8B3 = original + `791CE265.DIF` de NewRisingSun). Direcciones CS del archivo
(codigo en CS=base; DS = CS + 0x674 parrafos). Implementado literalmente en `src/tandy_music.c`; datos en
`src/gen_tandy_music.c` (generado con `tools/tandy/gen_music_data.py`).

## Variables DS
- `DS:0` = sound_enabled (tambien "hay 3 voces"), `DS:1` = musica activa (alterna con `not`).
- `DS:0x1010` ultimo tick; `0x1012` tick de inicio (tambien base del timeout del titulo, 0x50a ticks).
- `DS:0x1014..` tabla de frecuencias (palabras indexadas por BYTE; valor = divisor SN76496: lo nibble, hi 6 bits <<4).
- `DS:0x109a..0x1227` pistas: por voz una pista de notas (indices, 0 = silencio, 0x86 = fin/bucle) y una de duraciones (en ticks).
- `DS:0x1228` estado de trabajo (9 bytes): dur, vol, pos(w), canal(0x80/0xa0/0xc0), ptr pista(w), ptr duraciones(w).
- `DS:0x1231/0x123a/0x1243` estado persistente de cada voz; `DS:0x124c` tabla de punteros; `0x1252` indice de voz (0,2,4);
  `0x1254` contador de bucles (+2 cada vez que la voz 0 reinicia); `0x1256` limite por pasada (`02 00 04 00 04 00 00 00`).

## Rutinas
| CS | funcion |
|---|---|
| 0x16a0 | init: silencio x2 (si DS:0), PIT canal 0 = 0xCFFF, estados a 0/vol 0xF, guarda tick |
| 0x16e9/0x16f0 | PIT ch0 divisor 0xCFFF / 0 (restaura) |
| 0x170c/0x171f | begin/stop: silencio + `INT 1Ah AX=8003/8000` (multiplexor de sonido BIOS Tandy; sin efecto en el chip) |
| 0x1732 | silencio: `out 0xC0` 0x9F,0xBF,0xDF,0xFF |
| 0x173e | tick: si cambio el tick BIOS, copia estado de cada voz a 0x1228, llama 0x178d, lo devuelve |
| 0x178d | voz: cuenta duracion; envolvente de volumen (+2 por tick hasta 0xF); al acabar lee la siguiente nota; la voz `idx` suena solo si `limite[loops&6] >= idx` |

## Hooks
Bucle del titulo 0x6044..0x6149 (ver `tandy_title_loop` en `src/ui.c`): init, tick cada vuelta (en vez de 0x56a0), animacion cada 0x2c ticks,
gato cada 0x16, timeout 0x50a, Del conmuta musica. Salida 0x613d: silencio, 0x171f, port61 off, PIT=0.

## Verificacion
`tools/tandy/oracle.py` ejecuta el codigo original en unicorn (x86-16) y registra los `out`; `tests/test_tandy_music.c` exige que el
port produzca la misma secuencia (puerto, valor, tick): 0 diferencias en 3000 ticks (traza en `tests/data/`), y 6000 ticks en sesion.
Pendiente: [0x6ce] del titulo se toma como "suelta"; el chip suena a oido (sn76496.c) sin verificar contra hardware real.
