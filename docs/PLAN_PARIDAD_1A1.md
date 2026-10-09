# Plan de pruebas para lograr comportamiento 1 a 1 con el Alley Cat original

Objetivo: que el port (`alleycat_recomp_port_sdl`) produzca, ante las mismas entradas y
el mismo estado inicial, **las mismas imágenes y el mismo estado de juego** que el
`cat.exe` original, y que cada diferencia que quede esté documentada como desviación
consciente (como `JUMP_TICK_RELOAD`, `THROW_TIMER_DIV`).

Complementa a `DOSBOX_SANDBOX.md` (cómo ejecutar DOSBox y capturar). Leerlo primero.

---

## 1. Qué se puede y qué no se puede medir (estado verificado)

| Capacidad | Estado | Notas |
|---|---|---|
| Ejecutar `cat.exe` original sin pantalla (Xvfb + DOSBox 0.74) | **Verificado** | Teclas con `xdotool` tras un clic de foco. |
| Capturas de vídeo **exactas** | **Verificado** | El recorte `(80,100,720,500)` del escritorio reducido a 320x200 con `NEAREST` coincide **píxel a píxel** (0 diferencias de 64 000) con la captura nativa de DOSBox (Ctrl+F5 → PNG 320x200 en el directorio `captures=`). Se puede comparar contra `cga_mem` del port sin ambigüedad. |
| Leer el ASM y localizar etiquetas, `cx`, `bp`, flags | **Verificado** | `gmegidish/alleycat-disassembly`. Fuente de verdad. |
| Parchear una copia del `.exe` (saltar a un nivel, forzar flags) | **Verificado** | Ver `DOSBOX_SANDBOX.md` §5. |
| Ejecutar el port sin SDL2 (volcar `cga_mem` por presentación) | **Verificado** | Arnés con `l7_tick_fn` y hooks (`/tmp/vis.c`). |
| Depurador / `MEMDUMP` para leer variables del original | **No disponible** | El `dosbox` 0.74 de apt no trae depurador. `dosbox-x` (apt, 2024.03) se instala tras `apt-get update`, pero su binario no incluye los comandos de depurador (`MEMDUMP` no aparece). |
| Leer variables del original por otra vía | **Por construir** | Ver §3.3 (volcado desde el propio juego con una "cueva de código", o save-states de DOSBox-X). |
| Determinismo temporal exacto del original | **No garantizado** | El original sincroniza con ticks BIOS (18,2 Hz) del reloj real: DOSBox lo deriva del reloj del host. Se compara por **estado y eventos**, no por número de cuadro (ver §3.4). |
| Audio | **No verificado** | No hay tarjeta de sonido; se compara por tablas de notas y llamadas al altavoz, no por audio. |

Regla de honestidad: toda diferencia detectada se registra, aunque sea "aceptada".

---

## 2. Definición de "1 a 1" (niveles de fidelidad, de mayor a menor valor)

- **N1 – Datos:** todas las tablas y sprites del port son bytes idénticos a los del `.exe`.
- **N2 – Primitivas:** `calc_cga_addr`, `blit_*`, `save_from_cga`, `random`, `add_score`… devuelven lo mismo que el ASM para las mismas entradas.
- **N3 – Estado:** en los puntos de sincronización, las variables del juego (posiciones, vidas, puntuación, temporizadores, nivel) son iguales.
- **N4 – Vídeo:** el cuadro presentado es idéntico píxel a píxel en esos puntos.
- **N5 – Ritmo:** las duraciones relativas (cuántos ticks dura cada fase) son equivalentes. Las desviaciones de velocidad son conscientes y están documentadas.
- **N6 – Audio:** misma secuencia de frecuencias/duraciones.

Se persigue N1→N4 como "1 a 1"; N5 y N6 como "equivalente documentado".

---

## 3. Infraestructura a construir (Fase A, antes de las pruebas)

### 3.1 Captura exacta del original
- Script `tools/orig_capture.sh` (basado en `run2.sh`) que, además de recortar el escritorio, **guarda en 320x200** (`NEAREST`) y los indexa por evento (ver §3.4), no por tiempo.
- Verificar una vez más en cada sesión nueva que `desktop→320x200` == captura nativa (test de 64 000 píxeles).

### 3.2 Arranque en estado conocido
- Biblioteca de **parches de inicio** (`tools/patch_start.py`): cada uno produce `catv<N>.exe` que salta directo a: título, alley, nivel 2…7, game over, victoria.
- Para fijar el estado inicial (posición del gato, vidas, puntuación, nivel de dificultad, semilla del azar): parchear las constantes inmediatas de las instrucciones de inicialización (`mov [cat_x],imm`, etc.), buscándolas por patrón de bytes, igual que se hizo con la victoria.
- Para determinismo del azar: localizar la rutina `random` en el ASM y verificar de dónde toma la semilla (tick BIOS); parchear la semilla a una constante en ambos lados.

### 3.3 Leer variables del original (N3)
Dos vías, de menos a más esfuerzo:

1. **Cueva de código con volcado (recomendada).** Parchear una rutina que se ejecuta por cuadro (p. ej. el final del bucle principal) para llamar a un trozo de código nuevo, colocado en un hueco libre del segmento de código o al final del `.exe`, que:
   - abre/crea `STATE.BIN` con INT 21h (3Ch/40h),
   - escribe `N` bytes desde el inicio del segmento de datos (`DS:0000`–`DS:0600`, donde viven las variables del juego) y cierra.
   El archivo aparece en el directorio montado `/tmp/dos`; se compara con las variables del port. Debe leer `DS` (`mov ax,[cs:ds_seg]`) y preservar todos los registros/flags.
2. **Save-states de DOSBox-X** (si se consigue instalar y arrancar en Xvfb sin bloquear): la RAM de la VM está en el `.sav`; se parsea el volcado de memoria y se lee `DS:xxxx`. Pendiente de verificar el formato.

Salida común: `tools/orig_state.py` que devuelve un `dict` `{nombre_variable: valor}` usando el mapa de etiquetas del ASM (`dat_XXXX`/`[0x...]`) → nombre del port.

### 3.4 Sincronización sin depender del tiempo
- El original avanza por ticks reales; las capturas por intervalo fijo se desfasan. Se sincroniza por **eventos observables**: p. ej. "cuadro en el que cambia `cat_y`", "primer cuadro tras `level_number` cambia". Con el volcado de §3.3 se escribe un **contador de cuadros del juego** en `STATE.BIN` y se captura **cada vez que el contador cambia** (sondeo rápido del archivo) para obtener (estado, vídeo) emparejados.
- Del lado del port: el arnés de pruebas (reloj falso) ejecuta **N pasos lógicos** y vuelca estado y `cga_mem` en cada paso. Se alinean ambas trazas por el contador lógico.

### 3.5 Comparador
- `tools/compare.py`: toma una traza del original y una del port, emite un informe:
  - tabla de variables que difieren (primer cuadro en que divergen, valor original vs. port),
  - mapa de diferencias de píxeles (PNG con rojo donde difiere) y recuento,
  - veredicto N3/N4 por escena.
- Los "dorados" (capturas del original) **no se suben al repositorio**: son material del juego original. Se generan localmente a partir del `cat.exe` del usuario; el repositorio solo contiene scripts y el informe.

---

## 4. Catálogo de pruebas (Fase B), ordenado por prioridad

Cada prueba indica: **qué se compara**, **cómo se prepara**, **criterio de aceptación**.

### B1 – Datos (N1) — barato y de alto valor
1. **Segmento de datos completo:** volcar el `DS` inicial del `.exe` (según el cargador MZ: datos tras el código + relocaciones) y comparar byte a byte con `ds_pool` del port. Criterio: 0 diferencias en los rangos usados por el port. *Detecta tablas mal resueltas (como la tabla de punteros del nivel 7 desplazada 1 byte en el listado).*
2. **Tablas por nivel** (movimiento, objetos, paletas, notas): comparar cada una con el ASM, no solo con el pool.

### B2 – Primitivas gráficas (N2)
3. `calc_cga_addr(y,x)` para todo `y∈[0,199]`, `x∈[0,319]` → misma dirección y desplazamiento de bits que el ASM. Se obtiene el valor del original con una **prueba parcheada** que llama a la rutina y vuelca el resultado; o por inspección formal del ASM si es corto.
4. `blit_to_cga`, `blit_transparent`, `save_from_cga`, `restore`: para sprites representativos (anchura 1…13 words, alturas 1…32) y posiciones en los bordes (wrap de fila, y>192, x>300). Criterio: mismo contenido de `cga_mem` tras la llamada, incluido el comportamiento de **wrap** en los bordes.
5. **Todos los sprites** (cada fotograma de cada animación) dibujados en una posición fija sobre fondo conocido: capturar el original (con parche que dibuja el sprite *k*) y el port. Barrido sistemático sobre las tablas de punteros (`dat_4a60` y análogas) detecta dimensiones `cx` invertidas (el bug del gato 4x13) y punteros desplazados.

### B3 – Escenas deterministas sin entrada (N3+N4)
6. **Título / "IBM presents"** (animación del gato por la valla): estado + vídeo.
7. **Menús** (joystick, dificultad, "Press any key"): vídeo.
8. **Cinematica de victoria del nivel 7** (ya parcialmente comprobada: corazón con gato, estela, oleadas en 8 direcciones). Falta: comparación píxel a píxel con el mismo fondo, y el flash/barrido final.
9. **Pantalla de game over / fin de partida**, tabla de puntuación alta y entrada de iniciales.
10. **Barrido de transición (`wipe`)** entre niveles, cuadro a cuadro.

### B4 – Física y entrada (N3), con teclas guionizadas
11. **Movimiento en el callejón:** andar a izquierda/derecha N pasos desde una posición fija; comparar `cat_x/cat_y`, animación y vídeo cada paso.
12. **Salto y gravedad:** salto vertical y en arco desde varios puntos (`JUMP_TICK_RELOAD` es una desviación conocida: cuantificar y registrar la relación de ticks).
13. **Ventanas y tendederos:** subir a cada ventana y a cada cuerda; comparar `at_platform`, `current_floor`.
14. **Colisiones:** gato–ratón (el bug 1 de T83), gato–perro, gato–objetos arrojados, gato–cubos.
15. **Cuerdas de ropa moviéndose (`throw_timer`)** — desviación `THROW_TIMER_DIV`: medir la relación real (llamadas por tick) y justificar el divisor con datos del original.

### B5 – Niveles interiores 1–7 (N3+N4)
Una prueba por nivel, con parche de inicio y entradas guionizadas:
16. **Nivel 1 (cocina/ratones), 2 (pecera: aire y color de fondo — T78b/T83), 3 (enemigo), 4, 5, 6 (perros y platos — T82), 7 (corazones, cupidos, gatas, el bug del frame de pausa).**
17. Para cada nivel: estado de entrada, primer cuadro, N cuadros de juego con entrada fija, condición de victoria/derrota, salida.

### B6 – Reglas del juego (N3)
18. **Puntuación** (cada fuente de puntos), **vidas**, bonus de vida extra (`extralife_sprites`), **dificultad** (Kitten/House Cat/Tomcat/Alley Cat), pausa y reinicio (Ctrl-S, Ctrl-R, Ctrl-M, Esc).
19. **Azar:** con la misma semilla, la misma secuencia de `random` y sus consumidores (movimiento aleatorio de gatas, ratones…).

### B7 – Variantes de hardware y audio (N5/N6)
20. **`rom_id` (PCjr/Tandy vs PC):** las ramas que dependen de la máquina; el original solo puede ejecutarse como `machine=cga` en DOSBox 0.74, así que las ramas PCjr se verifican solo por lectura de ASM.
21. **Sonido:** extraer del ASM las secuencias (nota, duración) por evento y compararlas con las llamadas a `speaker_*` del port (trazando llamadas con `--wrap` como ya hace el Makefile para `test-t82`).

---

## 5. Orden de ejecución recomendado

1. **Fase A mínima** (≈1 sesión): captura exacta (§3.1, ya verificada), parches de inicio (§3.2) y volcado de estado (§3.3 vía 1).
2. **B1 completo** (datos): rápido, sin sincronización temporal, y descarta una clase entera de errores.
3. **B2.5** (barrido de todos los sprites): descubre errores de dimensiones/punteros en masa.
4. **B3** (escenas sin entrada): ya hay base (victoria nivel 7).
5. **B4 → B5 → B6** por nivel, empezando por los que ya tuvieron bugs (nivel 2, 6, 7, callejón).
6. **B7** al final.

Cada hallazgo se registra con: escena, cuadro de divergencia, valor original vs. port, etiqueta del ASM implicada y fix propuesto, y se añade un test al `make test` que **no dependa del original** (usa el valor esperado derivado del ASM, p. ej. el `4x13` del gato).

---

## 6. Criterios de aceptación por escena

- **"Idéntica":** N3 y N4 sin diferencias en todos los puntos de sincronización.
- **"Equivalente documentada":** N3/N4 idénticos salvo una constante de ritmo registrada en `PROGRESS.md` con la medición que la justifica.
- **"Pendiente":** diferencia sin explicar (se abre una tarea en `tareas.md`).

Informe final: matriz escena × nivel de fidelidad (N1…N6), con enlace al hallazgo.

---

## 7. Riesgos y límites

- **Temporización:** DOSBox no reproduce el ritmo exacto de un PC real; por eso se compara por eventos. `cycles=max` vs. `fixed` cambia la velocidad relativa de bucles que no esperan a un tick (los que dependen de "llamadas", como `throw_timer`); fijar `cycles=fixed N` y registrar `N`.
- **Azar:** si la semilla viene del tick BIOS, hay que fijarla en ambos lados o no se puede comparar nada que dependa del azar.
- **Colores:** el original usa los registros CGA/paleta que establece el programa; el port aplica su propia paleta. Se comparan **índices de color (0–3)**, no RGB.
- **Cobertura:** el nivel 7 completo y los modos PCjr solo se cubren parcialmente sin un PC real.
- **Derechos:** no versionar capturas ni volcados del original en el repositorio.

---

## 8. Hallazgos de partida (ya conocidos)

| # | Área | Hallazgo | Estado |
|---|---|---|---|
| 1 | Nivel 7, victoria | Dimensiones del sprite del gato invertidas (13x4 en vez de 4x13) | Corregido en T84c |
| 2 | Nivel 7, victoria | `l7_cupid_active` se anulaba al final de la oleada y no de cada pasada | Corregido en T84d |
| 3 | Nivel 7, victoria | El original no borra el corazón de pasos anteriores (estela) | Fiel por defecto (`L7_POS_ERASE_TRAIL=0`) |
| 4 | Nivel 7, victoria | El video no se presentaba durante la cinematica bloqueante | Corregido en T84 (`l7_step_hook`) |
| 5 | Alley, tendederos | `throw_timer` calibrado por llamadas | Desviación consciente `THROW_TIMER_DIV=32` |
| 6 | Alley, ventanas | `jump_tick_delay` calibrado por llamadas | Desviación consciente `JUMP_TICK_RELOAD=1` |
| 7 | Nivel 2 | Color de fondo/borde no aplicado a la paleta | Corregido en T83 |
| 8 | Alley, ratones | Rama de `at_platform` invertida, `add_score` vacío | Corregido en T83 |


---

## 9. Registro de ejecución

### 2026-10-08 — B1 (datos) ejecutado

| Prueba | Resultado |
|---|---|
| B1.1 `ds_pool` vs segmento de datos del `.exe` | **0 diferencias** en 28 976 bytes (offset de archivo `0x300`; el MZ no tiene relocaciones dentro de ese segmento). |
| B1.2 61 tablas/sprites `src/gen_*.c` | **61/61** aparecen como bloque contiguo en el segmento de datos. |
| B2.5 (parcial) dimensiones `mov cx` ASM vs llamadas a blit del port | **0 discrepancias**, pero solo 11 de 46 llamadas son comprobables estáticamente. No sustituye al barrido por pantalla. |

Conclusión: los **datos** del port son fieles al original. Los errores vistos hasta ahora (gato 13x4, `l7_cupid_active`, ramas invertidas) son de **lógica y parámetros de llamada**, no de datos. Eso orienta el resto del plan hacia B2.5 (barrido por pantalla), B3 y B4, que necesitan el volcado de estado (§3.3).

Herramienta: `tools/parity_b1.py` (parche `parity_b1.patch`). Lo siguiente en el plan: §3.3 (volcado de estado desde el propio juego) y B3.

### 2026-10-08 — §3.3 (volcado de estado del original) construido y verificado

- `tools/orig_state/` (parche `orig_state.patch`): `build_dump_exe.py` + `cave.asm` generan `catdump.exe`; `read_state.py` lee `STATE.BIN` por nombre de variable.
- Verificado con 241 ticks del juego real: un registro por tick BIOS (consecutivos), 8 KB de `DS` por registro, variables coherentes (`lives_count` 3→2, `cat_x` 0→296, `current_floor`, `frame_counter`).
- Hallazgo útil: el desensamblado de `gmegidish` da el **mapa exacto de direcciones** (listado de NASM → 717 etiquetas de datos) y reensambla el `.exe`. Con nasm reciente fallan 3 líneas por tamaño de operando (`hardware.asm:226`, `enemy.asm:346`, `score.asm:118`), pero el listado se genera igual; para reconstruir el `.exe` hay que corregirlas o usar nasm antiguo.
- Dato de ritmo: en DOSBox el bucle del callejón itera ~100 veces por tick (`frame_counter` salta ~107 por tick); relevante para `throw_timer` y `jump_tick_delay`, que cuentan llamadas.
- Pendiente: el enganche solo cubre el bucle del callejón; para niveles interiores hay que añadir un enganche en su bucle (misma cueva, otro punto de llamada).
- Lo siguiente en el plan: alinear una traza del original con una del port (§3.4/§3.5) y empezar B4 (movimiento y salto).

### 2026-10-09 — §3.4/§3.5 (traza por tick + comparador) y primer tramo de B4 (andar y subir)

Herramientas nuevas (`tools/parity/`): `orig_run.sh` (DOSBox sin pantalla con teclas `down:`/`up:`), `port_trace.c` (+ `build_port_trace.sh`,
traza del port por tick BIOS simulado, sin SDL), `compare.py` (compara columnas por tick y genera el guion de entrada del port a partir de la
propia traza del original). `tools/orig_state/`: la ventana del volcado pasa de 0x2000 a 0x2c00 bytes (`rng_seed` vive en `DS:0x2ae5`).

| Escena (n = tick de registro; entrada = la que registro el original) | Comparado | Resultado |
|---|---|---|
| Arranque del juego (entrada del gato desde la izquierda) | `cat_x`, `scroll_direction` n=1..9 | **Igual** (`cat_x`=8 y `scroll_direction` 1→0 en n=9) |
| Andar a la derecha desde reposo (aceleracion de `scroll_speed` 2→8) | `cat_x`, `cat_y`, `scroll_speed`, `scroll_direction`, `in_level_mode`, hasta el primer perro | **Igual** (127/127 ticks; 7 de ellos andando: 12,16,22,28,36,44 y `scroll_speed` 2,3,3,4,4,5) |
| Subir (Arriba mantenida: `in_level_mode` 255→1, `cat_y` 173,162,155,150,…,161) | `cat_x`, `cat_y`, `scroll_speed`, `in_level_mode`, `at_platform` | **Igual** (128/128 ticks, semillas 1234 y 999) |
| Perro | `enemy_*` | **No comparable por tick** (ver abajo) |

Hallazgos de metodo (no son fallos del port):
* **El azar no se alinea por tick.** En el original `rng_seed` cambia en cada iteracion del bucle (cientos de cambios por tick), y el perro
  aparece en reposo (p. ej. n=76 en una corrida de 476 ticks quieta) o justo al andar (n=122/134 en otras dos). El port con `PIT_SEED` fijo
  tambien genera perro en reposo (22 de 40 semillas en 300 ticks), asi que *no hay evidencia de diferencia* en la tasa de aparicion, pero
  tampoco de igualdad: se necesita el diferencial por iteracion.
* **Al andar el original da 2 o 3 pasos de `scroll_speed` por tick** segun el reparto de iteraciones (se vio 12→19 y 196→220 en una corrida);
  el port da exactamente 2. Es cuantizacion de ritmo (N5), no de estado; en las comparaciones de arriba la entrada coincide y no se nota
  porque el tramo andando es corto.
* `cga_init()` siembra `rng_seed` con el reloj real (`cga.c:25`): cualquier arnes que no lo pise no es reproducible (lo hacen `port_trace` y,
  con otra via, `test_soak`).
* Registro de variables del original: `current_floor` (`DS:0x52f`) cambia 0/1/2 sin que el gato cambie de piso en la traza del original, asi
  que el nombre no describe su uso real; no se usa para comparar hasta aclararlo.

Siguiente (por orden): (1) **diferencial por iteracion**: cueva que registre cada llamada a `update_animation` (ventana corta alrededor del
perro) y un arnes del port que cargue el registro *k*, ejecute **una** pasada y compare con el registro *k+1*; es la unica forma de validar
`update_enemies`/`check_enemy_activate` sin depender del azar ni del ritmo; (2) saltos en arco y ventanas (B4.12/13) con el mismo metodo
por tick; (3) aclarar `current_floor`.
