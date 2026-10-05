# Subtareas para terminar el port Alley Cat (C/SDL2)

Cada tarea está pensada para resolverse en **un solo chat/turno de herramientas de Claude Free**: un rango corto de ASM, un destino claro en C, y una verificación concreta. Si una tarea no cabe, ver la regla de corte del protocolo.

- Port: https://github.com/fschpp/alleycat_recomp_port_sdl (`main`)
- ASM: https://github.com/gmegidish/alleycat-disassembly
- Complementa a `plan.md` (fases) y a `PROGRESS.md` (registro de sesión).

Tamaños: **S** ≤ 60 líneas de ASM, **M** 61–130, **L** 131–170 (una sola función).
Los números de línea son de `asm/src/<archivo>.asm` en el `main` actual del ASM.

---

## 0. Protocolo común (aplica a TODAS las tareas)

### 0.1 Prompt base para pegar al inicio de cada chat

```
Proyecto: port C/SDL2 de Alley Cat. Repos: github.com/fschpp/alleycat_recomp_port_sdl (port)
y github.com/gmegidish/alleycat-disassembly (ASM, fuente de verdad).
Tarea: <ID y título de tareas.md>.
Pasos: 1) clona ambos repos con git clone --depth 1 y corre tools/setup.sh si existe
(si no existe, ejecuta T00 primero). 2) Lee SOLO el rango de ASM indicado con sed -n 'A,Bp'.
3) Portá literal, etiqueta por etiqueta, con goto. 4) Compilá con -Wall -Wextra sin warnings.
5) Verificá como indica la tarea. 6) Actualizá PROGRESS.md (Current focus / Todo / Blockers)
y devolveme los archivos cambiados completos (o git diff). No toques nada fuera del alcance.
```

### 0.2 Reglas de traducción (salen de los errores ya encontrados en PROGRESS.md)

1. **Literal, no "limpio"**: una etiqueta `lab_XXXX` = una etiqueta C `lab_XXXX:` con `goto`. Refactorizar solo al final (Fase 5).
2. **Saltos sin signo**: `jc/jnc/ja/jbe` sobre bytes que pueden valer 0xFF se comparan como `uint8_t`/`uint16_t`, nunca con el tipo con signo.
3. **`mov reg,label` sin corchetes** carga la *dirección* del label como constante inmediata, no el dato (casos `dat_1b02`, `lives_cga_pos`, `meow_sound_data`, `l5_sprite_ptr`).
4. **Anchos de sprite en palabras**, y `CX` va empaquetado: alto en el byte alto, ancho (palabras) en el bajo. No convertir dos veces.
5. **Etiquetas con nombre no codifican su dirección** (solo `dat_XXXX` sí). Todo offset DS sale de `/tmp/data_segment_labels.txt` (`grep -w <label>`), nunca de adivinar.
6. **Cadenas de bytes `db` en el código** (`db 0x2b, 0xdb ; sub bx,bx`) son instrucciones: leer el comentario a la derecha.
7. **Efectos del flag stale**: si un `jnz` sigue a `mov`s (que no tocan flags), prueba el flag de un `cmp` anterior. Anotar en comentario.
8. Datos: usar `ds_pool` (`include/gen/ds_pool.h`) con el offset real. No crear script de extracción nuevo salvo que la tarea lo pida.
9. Estado global: reutilizar variables de `include/cat_state.h`. Si falta una, añadirla ahí con el tipo del original (byte = `uint8_t`, flag 0xFF/0x00 no es `bool`).
10. Tiempo: `int 0x1a` → `read_bios_tick()`; `check_vsync` → no-op; esperas bloqueantes solo donde el original bloquea y es cutscene de una vez.

### 0.3 Regla de corte (si no cabe en el turno)

Parar en el `lab_XXXX` más cercano, dejar `/* TODO(<ID>-cont): continuar en lab_XXXX */` con un `return` seguro, compilar, y anotar en PROGRESS.md "§ <ID>: portado hasta lab_XXXX". La continuación es una tarea nueva `<ID>b` con el rango restante.

### 0.4 Entrega de cada tarea

- Archivos modificados completos (o `git diff`), más `Makefile` si se añadió `.c` a `SRC`.
- Declaración en el `.h` correspondiente; reemplazo de los stubs (buscar con `grep -rn "<nombre>" src include`).
- Entrada nueva en PROGRESS.md con: qué se portó, offsets DS verificados, desviaciones y cómo se verificó.
- Mensaje de commit sugerido.

### 0.5 Equivalencias de nombres ya existentes (no son pendientes)

| ASM | C |
|---|---|
| `init_objects` | `init_cycle_objects` |
| `cycle_animations` | `update_cycle_objects` |
| `process_keyboard` | `input_process_keys` |
| `draw_thrown_sprite` / `erase_thrown_sprite` | `draw_l1_thrown` / `erase_l1_thrown` (`level_objects.c`) |
| `draw_level3_enemy` / `erase_level3_enemy` | `draw_l3_bird` / `erase_l3_bird` |
| `draw_score` / `draw_high_score` | `draw_high_score_display` / `draw_current_score` (nombres cruzados en el original, ver §5v) |
| `init_player` | falta (T18), solo existe `reset_jump` |

---

## FASE 0: herramientas (todas S)

### T00 — Script de setup y utilidades [S] ✅ HECHO (PROGRESS.md §6g)
- **Destino:** `tools/setup.sh`, `tools/asm_range.sh`, parche de `tools/resolve_data_segment.py`.
- **Qué hacer:**
  1. `resolve_data_segment.py` tiene `ROOT` fijo en `/home/claude/work/...`. Cambiarlo para leer `ALLEYCAT_ASM` del entorno (default `../alleycat-disassembly`).
  2. `setup.sh`: clona los dos repos (`--depth 1`), exporta `ALLEYCAT_ASM`, corre el resolver (genera `/tmp/data_segment.bin` y `/tmp/data_segment_labels.txt`), intenta `apt-get install -y libsdl2-dev` (si falla, avisa y sugiere `-fsyntax-only`).
  3. `asm_range.sh <archivo> <ini> <fin>`: imprime ese rango con números de línea.
  4. `asm_label.sh <label>`: imprime `grep -w` del label en `/tmp/data_segment_labels.txt`.
- **Verificar:** correr `setup.sh` en limpio; el resolver debe imprimir 28976 bytes y 783 labels.

### T01 — Arnés de tests headless [M]
- **Destino:** `tests/` (nuevo), target `make test` en `Makefile`.
- **Qué hacer:**
  1. `tests/harness.h` con helpers: `cga_clear(byte)`, `cga_count_pixels(color)`, `cga_dump_ascii(x,y,w,h)`, `cga_dump_ppm(path)` (usando la paleta CGA ya usada en `video.c`).
  2. `tests/test_template.c` y un test de ejemplo que reproduzca el chequeo de `§6f` (24 frames de caminata dejan ~111 píxeles negros).
  3. `make test`: compila cada `tests/test_*.c` contra los `.c` que no dependen de SDL (cga, cat_state, movement, alley, etc.) y los ejecuta.
- **Verificar:** `make test` pasa; el test de ejemplo reproduce el número de `§6f`.

### T02 — Lista real de pendientes autogenerada [S]
- **Destino:** `tools/unported.py`, `docs/PENDIENTES.md`.
- **Qué hacer:** script que lee todas las etiquetas con nombre de `asm/src/*.asm` y busca su definición en `src/*.c` (regex de definición de función, **no** `grep -w`, porque los comentarios dan falsos positivos), usando la tabla 0.5 de equivalencias. Imprime tamaño en líneas de cada pendiente.
- **Verificar:** debe listar, entre otras, `check_window_landing`, `check_stairs_collision`, `update_cupid`, `handle_level_complete`, `spawn_thrown_object`, `init_player`, `level_transition`, `start_auto_walk`.

---

## FASE 1: callejón jugable (nivel 0)

### T10 — `pixel_to_bitmask` + `check_window_landing` [S · 72] ✅ HECHO (PROGRESS.md §6g)
- **ASM:** `level_physics.asm` L197–268.
- **Destino:** `src/level_collision.c` (+ `include/level_collision.h`); enganchar en `check_level_collision` para `level_number==0`, hoy comentado como "no portado" (L172–186).
- **Datos:** `window_column=0x0525`, `current_floor=0x052f`, `window_row_offset=0x1025`, bitmap de ventanas `throw_col_data=0x1016` (el `test byte [bx+si+0x1016],ch`).
- **Notas:** `pixel_to_bitmask` devuelve `ch` (máscara) y ajusta `bx`/`si`; leer sus 13 líneas antes de portar. Si la lógica sale `stc`: `cat_y=dl`, `cat_y_bottom=dl+0x32`, `cat_x &= 0xfff8`, `at_platform=1`.
- **Verificar:** test que fija `cat_y=0x08/0x28/0x48`, `current_floor`, `window_column` y un bit de `throw_col_data` y comprueba carry y efectos; caso negativo con `cat_y` distinto.

### T11 — `check_stairs_collision` (nivel 7) [S · 46] ✅ HECHO (PROGRESS.md §6h; no lee `current_floor`)
- **ASM:** `level_objects.asm` L302–347.
- **Destino:** `src/level_collision.c`; enganchar para `level_number==7`.
- **Datos:** leer `window_open_state=0x2be2`, `current_floor`.
- **Verificar:** test con tres combinaciones de `window_open_state`/`current_floor` y la posición del gato.

### T12 — Helpers de `throw.asm` [M · 107] ✅ HECHO (PROGRESS.md §6i; incluye fix de `cga_random`)
- **ASM:** `throw.asm` L170–276 (`rotate_throw_bits` 170–192, `check_throw_range` 193–210, `generate_throw_object` 211–256, `generate_throw_pattern` 257–276).
- **Destino:** nuevo `src/throw.c` + `include/throw.h`; añadir a `Makefile`.
- **Datos:** `throw_sprite_small=0x0460`, `throw_sprite_large=0x0490`, `throw_pattern_ptrs=0x04d0`, `throw_bits=0x0540`, `throw_rotate_dir=0x0541`, `throw_col_data=0x1016`, `throw_obj_buf=0x04d7`, `in_throw_range=0x04d6`. Extensión de cada tabla por cómo se indexa, no por el label.
- **Notas:** `generate_*` llama `random()` en orden fijo: respetar el número exacto de llamadas.
- **Verificar:** semilla fija del LFSR, ejecutar `generate_throw_pattern` N veces y comparar contra una traza escrita a mano de las 3 primeras salidas.

### T13 — `update_thrown_objects` + `reset_window_state` real [L · 165] ✅ HECHO (PROGRESS.md §6j)
- **ASM:** `throw.asm` L5–169 y L277–281 (`reset_window_state`).
- **Destino:** `src/throw.c`; reemplazar el stub `reset_window_state` de `src/game_setup.c` (L17).
- **Datos:** `window_column=0x0525`, `throw_col_init=0x0526`, `throw_col_step=0x0529`, `throw_y_param=0x052c`, `current_floor=0x052f`, `throw_timer=0x0531`, `throw_delay=0x0532`, `throw_last_tick=0x0544`, `throw_draw_col=0x051d`, `throw_scroll_src=0x0517`.
- **Notas:** el código pasa por `reloc_2` (L69), es solo un marcador, el código continúa. Usa `check_vsync` (no-op) y `random`.
- **Verificar:** harness que corre 200 ticks simulados y comprueba que `current_floor` ∈ {0,1,2} y `window_column` cambian, y que no se sale de rango. Mostrar el volcado ASCII de una frame con un objeto en vuelo.

### T14 — `alley_drawing` A: ícono de dificultad y filas de objetos [S · 75]
- **ASM:** `alley_drawing.asm` L53–127 (`draw_difficulty_icon` 53–63, `init_alley_objects` 64–87, `draw_object_row` 88–127).
- **Destino:** nuevo `src/alley_drawing.c` + `include/alley_drawing.h`.
- **Datos:** `throw_chance=0x2aba`, `throw_timer`, `current_floor`, `window_column`; sprites del ícono en `result_sprites`/`ds_pool` (verificar extensión).
- **Notas:** `init_alley_objects` inicializa `window_open_state` (L82): este es el origen de esa tabla.
- **Verificar:** test que llama `init_alley_objects` y vuelca `window_open_state[]` y `throw_timer`; `draw_difficulty_icon` con `difficulty_level` 0 y 7, comparando pixel counts.

### T15 — `alley_drawing` B: ventanas y edificios [M · 80]
- **ASM:** `alley_drawing.asm` L161–184 (`draw_window_strip`, `draw_all_windows`) y L242–286 (`draw_building`, `draw_all_buildings`).
- **Destino:** `src/alley_drawing.c`.
- **Datos:** `window_sprite_data=0x2680`, `window_row_y_table=0x2bd4`, `window_row_col_offset=0x2bdb`, `window_open_state=0x2be2`.
- **Verificar:** dibujar sobre fondo negro y comprobar volcado ASCII de una franja; el número de píxeles no negros debe ser > 0 y estable entre dos ejecuciones.

### T16 — `alley_drawing` C: detalles y escena [M · 104]
- **ASM:** `alley_drawing.asm` L6–52 (`clear_screen`, `draw_alley_scene`) y L185–241 (`draw_alley_details`).
- **Destino:** `src/alley_drawing.c`.
- **Notas:** `draw_alley_details` usa `random()` (mantener el orden de llamadas) y `blit_to_cga`; usa `draw_block_list` ya portado en `level_background.c` (exportarlo en el `.h` si es `static`).
- **Verificar:** renderizar la escena completa a PPM (T01) y revisarla a ojo; contar píxeles por color como regresión.

### T17 — `update_viewport` y `render_sprites` [M · 62]
- **ASM:** `alley.asm` L2–31 y `sound.asm` L40–71.
- **Destino:** `src/alley.c` y `src/alley_drawing.c`.
- **Notas:** `update_viewport` copia bytes al área scratch DS `0x000e` y apunta `cat_sprite_data` a ella: modelarlo como `uint8_t scratch_000e[N]` y que `cat_sprite_ptr` apunte ahí (convención de puntero real de §6f). `render_sprites` usa `sprite_variant_table`, `sprite_dims_table`, `sprite_data_ptrs`: aplicar el método de verificación de §3/§4 (diferencias de punteros = ancho×2×alto) antes de extraer.
- **Verificar:** `update_viewport` llamada una vez deja `cat_sprite_ptr == scratch_000e`; `render_sprites` dibuja y el número de píxeles coincide en dos ejecuciones con la misma semilla.

### T18 — `init_player`, `start_auto_walk`, `check_dog_collision` real [M · 100]
- **ASM:** `level_physics.asm` L269–283; `game_loop.asm` L108–157; `enemy.asm` L69–113.
- **Destino:** `src/fall_object.c` (`init_player`, junto a `reset_jump`), `src/game_setup.c` (`start_auto_walk`), `src/enemy.c` (`check_dog_collision` real, hoy stub en `game_setup.c`).
- **Notas:** `check_dog_collision` solo actúa si `level_number==0` y `gravity_y!=0` (es la landing del proyectil sobre el gato); llama `restore_alley_buffer`, `restore_gravity_bg`, `enter_building`, `handle_cat_death`. `init_player`: `jump_anim_counter=0`, `gravity_y=0`, `idle_aggro_flag=0`, `deduct_life=0`, `jump_toss_delay=9`.
- **Verificar:** tests de `init_player` (valores exactos) y de `check_dog_collision` con proyectil solapando al gato (efectos: `dog_catch_flag=1`, `gravity_h_speed=0x60`, `at_platform=0`).

### T19 — Integración del callejón en el loop [M]
- **ASM de referencia:** `entry.asm` L131–180 (lab_0137 a lab_0176, el loop del callejón).
- **Destino:** `src/main.c`, `src/game_setup.c`.
- **Qué hacer:** en `setup_alley`/`setup_level` llamar `init_alley_objects`, `draw_alley_scene`, `reset_window_state`. En el loop, replicar el orden y cadencia de `entry.asm` (cada 4.º frame, o cada frame con enemigo activo, la cadena `update_thrown_objects → update_cat_jump → apply_cat_gravity → animate_falling → update_cycle_objects → draw_lives`). Quitar `memset(cga_mem, DEMO_BG_BYTE…)` y el `lives_display = 0xff` forzado (ya no hacen falta).
- **Verificar:** ejecutar 600 frames headless en nivel 0 con entrada simulada; volcar PPM y revisar que hay edificios, ventanas, objetos lanzados y que el gato no deja rastro.

---

## FASE 2: niveles interiores (`level_objects.asm`)

Estado compartido de los niveles 4 y 5: **los dos usan las variables `l5_obj_*`** (el nivel 4 las reutiliza). Declararlas una sola vez en un `.h` común (`include/level45_state.h`) en T21.

### T20 — Puertas del nivel 3 [S · 65]
- **ASM:** `level_objects.asm` L1376–1440 (`init_level3_doors`, `update_level3_doors`, `close_level3_door`).
- **Destino:** `src/level3_enemy.c` (mismo nivel).
- **Datos:** `l3_door_toggle=0x396b`, `l3_door_anim_frame=0x39e1`, `l3_door_cga_1/2/3=0x39e2/4/6`, `l3_door_sprite_base=0x39e8`.
- **Verificar:** test que avanza `update_level3_doors` unos ticks y comprueba que el frame de animación cicla y `close_level3_door` restaura el fondo (pixel count).

### T21 — Nivel 4: helpers A [M · 110]
- **ASM:** `level_objects.asm` L1897–1932 (`check_l4_thrown_collision`, `init_level4_objects`) y L2094–2149 (`erase_level4_sprite`, `randomize_l4_pos`, `calc_l4_obj_pos`).
- **Destino:** nuevo `src/level4.c` + `include/level4.h` + `include/level45_state.h`.
- **Datos:** `l5_obj_*` (offsets en `data_segment_labels.txt`: `l5_obj_cga_addr=0x3ea6`, `l5_obj_active=0x3eae`, `l5_obj_hit=0x3eb2`, `l5_obj_anim=0x3eb6`, `l5_obj_frame=0x3eba`, `l5_obj_save_buf=0x3ec2`, `l5_obj_dims=0x3ecc`, `l5_obj_y_pos=0x3ed4`, `l5_obj_count=0x3ed8`, `l5_anim_delay=0x3ed9`, `l5_obj_index=0x3eda`), `l4_obj_x_table=0x1137`, `l4_platform_offset=0x1050`.
- **Notas:** `randomize_l4_pos` ↔ `calc_l4_obj_pos` ↔ `check_l4_proximity` se llaman entre sí; declarar prototipos primero.
- **Verificar:** semilla fija; `init_level4_objects` deja los arrays en valores conocidos que se anotan en el test.

### T22 — Nivel 4: helpers B y cola de `init_level4_bg` [S · ~85]
- **ASM:** `level_objects.asm` L2150–2197 (`check_l4_obj_cat`, `check_l4_obj_thrown`, `check_l4_proximity`) y la **cola** de `init_level4_bg` (ver L1824–1896; el tramo que siembra `dat_3ce3/3ce4/3cf3/3cf4` desde `difficulty_level`, hoy diferido como código muerto).
- **Destino:** `src/level4.c` y `src/level_background.c`.
- **Verificar:** colisiones con casos solapado/no solapado; tras `init_level4_bg` la tabla sembrada coincide con la lectura manual del ASM para `difficulty_level` 0 y 5.

### T23 — `update_level4_anim` [L · 161]
- **ASM:** `level_objects.asm` L1933–2093.
- **Destino:** `src/level4.c`.
- **Notas:** usa `blit_masked`, `calc_cga_addr`, `save/restore_alley_buffer`, `start_tone`. Seguir la regla de corte si hace falta (cortar en el `lab_` más cercano a la mitad).
- **Verificar:** correr 100 ticks con el gato fijo; comprobar que el objeto cambia de frame, se dibuja y se borra sin dejar residuo.

### T24 — `update_level4_state` [M · 104]
- **ASM:** `level_objects.asm` L1720–1823.
- **Destino:** `src/level4.c`.
- **Datos:** `l4_obj_cur_x=0x3d03`, `l4_obj_cur_y=0x3d05`, `l4_anim_offset_table=0x3d06`, `l4_last_tick=0x3d16`, `l3_platform_id`.
- **Verificar:** test con tick simulado y plataforma fija; revisar que `l4_obj_cur_*` se mueve dentro de los límites de las tablas.

### T25 — Nivel 5: helpers A [M · 76]
- **ASM:** `level_objects.asm` L2362–2437 (`check_l5_landing`, `calc_l5_direction`, `check_l5_cat_catch`, `check_l5_thrown`, `init_level5_objects`).
- **Destino:** nuevo `src/level5.c` + `include/level5.h`.
- **Datos:** `thrown_obj_x=0x327d`, `thrown_obj_y=0x327f`, `l5_*` (T21).
- **Verificar:** `calc_l5_direction` con cuatro posiciones relativas del gato; colisiones con casos límite.

### T26 — Nivel 5: helpers B [M · 63]
- **ASM:** `level_objects.asm` L2603–2665 (`check_l5_perch_hit`, `draw_l5_perch`, `erase_l5_perch`, `check_l5_thrown_near`, `check_thrown_near_cat`).
- **Destino:** `src/level5.c`. `check_thrown_near_cat` también lo usa el nivel 6 (T32): exportarlo.
- **Verificar:** dibujar/borrar el perch y comprobar que el fondo vuelve idéntico (round-trip como en §5p).

### T27 — `update_level5_anim` [L · 164]
- **ASM:** `level_objects.asm` L2198–2361.
- **Destino:** `src/level5.c`.
- **Notas:** usa `blit_transparent`, `play_random_chirp`, `random`. Mantener el orden de `random()`.
- **Verificar:** 150 ticks con semilla fija; imprimir la secuencia de `l5_obj_frame` y anotarla en el test como regresión.

### T28 — `update_level5_objects` [L · 165]
- **ASM:** `level_objects.asm` L2438–2602.
- **Destino:** `src/level5.c`.
- **Verificar:** como T27, además de un caso de aterrizaje (`check_l5_landing`) y de captura (`check_l5_cat_catch`).

### T29 — Nivel 6: helpers A (tracker, tiles, init) [M · 112]
- **ASM:** `level_objects.asm` L2984–3095 (`erase_l6_tracker`, `draw_l6_tracker`, `init_level6_objects`, `draw_l6_tile`, `calc_l6_addr`, `level6_stubs`).
- **Destino:** nuevo `src/level6.c` + `include/level6.h`.
- **Datos:** `l6_obj_x=0x4411`, `l6_obj_sprite_ptr=0x4429`, `l6_obj_state=0x4459`, `l6_obj_type=0x4471`, `l6_obj_init_y_tbl=0x4479`, `l6_obj_dims=0x4481`, `l6_obj_y=0x4499`, `l5_sprite_table_base=0x41fc`.
- **Notas:** `level6_stubs` (L3082–3095) son restos; leerlo y decidir si hay que portarlo o es relleno (anotarlo).
- **Verificar:** `calc_l6_addr` contra valores calculados a mano para tres objetos; dibujar/borrar tracker con round-trip.

### T30 — Nivel 6: helpers B [M · 76]
- **ASM:** `level_objects.asm` L2741–2816 (`prepare_l6_erase`, `clear_l6_object`, `refresh_l6_display`, `check_l6_proximity`, `draw_l6_alert`).
- **Destino:** `src/level6.c`.
- **Verificar:** `check_l6_proximity` con tres distancias; `clear_l6_object` restaura el fondo.

### T31 — `update_level6_timing` [M · 75]
- **ASM:** `level_objects.asm` L2666–2740.
- **Destino:** `src/level6.c`.
- **Notas:** llama `activate_enemy_chase` (ya portado en `enemy.c`) y `play_explosion_effect` (sonido bloqueante, ya portado).
- **Verificar:** test con tick simulado que dispara la transición de estado de un objeto y comprueba `l6_obj_state`.

### T32 — `update_level6_movement` [L · 167]
- **ASM:** `level_objects.asm` L2817–2983.
- **Destino:** `src/level6.c`.
- **Notas:** usa `draw_l1_thrown`/`erase_l1_thrown` (equivalentes de `draw_thrown_sprite`), `check_thrown_near_cat` (T26). Si no cabe: cortar a mitad por `lab_`.
- **Verificar:** 100 ticks con el gato fijo y un objeto lanzado; confirmar movimiento y borrado limpio.

### T33 — Nivel 2: helpers y datos [M · 82]
- **ASM:** `level_objects.asm` L806–871 (`init_level2_objects`, `reset_caught_objects`) y L1001–1016 (`erase_level_object`).
- **Destino:** nuevo `src/level2.c` + `include/level2.h`.
- **Datos:** `l2_anim_toggle=0x3411`, `l2_obj_x=0x3447`, `l2_obj_y=0x3477`, `l2_obj_hit=0x348f`, `l2_obj_active=0x34a7`, `l2_obj_cga_addr=0x34bf`, `l2_obj_cur_addr=0x34ef`, `l2_obj_init_y=0x34f1`. Cada array tiene 24 bytes (separación entre offsets): confirmarlo contra cómo se indexa.
- **Notas:** `reset_caught_objects` tiene un salto a sí mismo (`tail=reset_caught_objects`): es un bucle, leerlo bien.
- **Verificar:** tras `init_level2_objects` con semilla fija, volcar los arrays y compararlos con una traza manual.

### T34 — `check_level_objects` [M · 116]
- **ASM:** `level_objects.asm` L690–805.
- **Destino:** `src/level2.c`.
- **Notas:** usa `reset_noise`/`update_noise`/`start_tone` (ya portados, PWM directo) y un bucle de ruido sincronizado con el tick. Mantener el bloqueo si el original bloquea.
- **Verificar:** colocar al gato sobre un objeto activo → `l2_obj_hit` se activa, suena, se borra y `add_score` se llama si corresponde.

### T35 — `update_level2_objects` [M · 129]
- **ASM:** `level_objects.asm` L872–1000.
- **Destino:** `src/level2.c`.
- **Verificar:** 100 ticks con semilla fija; los objetos caen y reaparecen en posiciones deterministas (anotar las 3 primeras).

### T36 — Animaciones del nivel 2 [M · 83]
- **ASM:** `level_objects.asm` L1017–1099 (`animate_level2_blocks`, `update_entrance_anim`).
- **Destino:** `src/level2.c`.
- **Datos:** `level2_block_types=0x2656`, `level2_bar_sprites=0x2020`.
- **Verificar:** pixel counts de la franja de bloques en dos ticks consecutivos (deben diferir).

### T37 — Nivel 7: `spawn_thrown_object` [M · 98]
- **ASM:** `level_objects.asm` L41–138.
- **Destino:** `src/level7_epilogue.c`.
- **Datos:** `l7_obj_x=0x2b5a`, `l7_obj_y=0x2b6a`, `l7_obj_active=0x2b72`, `l7_obj_erase_sprite=0x2b7a` (ya verificados en §6b).
- **Verificar:** al correr el spawn con semilla fija se activa un slot de `l7_obj_*`; `check_l7_object_overlap` (ya portado) deja de ser inerte.

### T38 — Nivel 7: `tick_level_thrown_objects` [M · 70]
- **ASM:** `level_objects.asm` L139–208.
- **Destino:** `src/level7_epilogue.c`.
- **Verificar:** 100 ticks; los objetos de `l7_obj_*` caen y se desactivan al salir de pantalla.

### T39 — `draw_love_scene_bg` y `draw_bg_tile` [M · 93]
- **ASM:** `level_objects.asm` L209–301.
- **Destino:** `src/level_background.c` (reemplaza el no-op del nivel 7 en `draw_level_background`).
- **Datos:** `l7_bg_tile_ptrs=0x2e20`, `window_open_state=0x2be2` (L246 escribe esa tabla).
- **Verificar:** renderizar el fondo del nivel 7 a PPM y revisarlo; pixel count > 0.

---

## FASE 3: flujo del juego (`entry.asm` y pantallas)

### T40 — `entry.asm` parte 1: arranque, título y nueva partida [M · 117]
- **ASM:** `entry.asm` L27–143 (`entry` hasta `lab_0140`).
- **Destino:** nuevo `src/game_flow.c` + `include/game_flow.h`.
- **Qué hacer:** leer todo el rango, listar qué llamadas ya existen en C y cuáles faltan (T50–T55), y escribir `game_start()` con ellas como llamadas a funciones declaradas (las que faltan, stubs con `/* TODO(Txx) */`). Respetar `sound_enabled=0xFF` y `init_music`.
- **Verificar:** compila; `game_start()` deja `lives_count`, `difficulty_level`, `level_number` en los valores del ASM (anotarlos).

### T41 — `entry.asm` parte 2: loop del callejón y selector de nivel [M · 93]
- **ASM:** `entry.asm` L144–236 (lab_0155 a lab_022a).
- **Destino:** `src/game_flow.c`.
- **Notas:** incluye el selector de nivel ponderado aleatorio tras la muerte; extraer los pesos como tabla verificada (`grep` del label correspondiente en `data_segment_labels.txt`).
- **Verificar:** test del selector con semilla fija: histograma de 1000 tiradas vs. los pesos.

### T42 — `entry.asm` parte 3: tabla de saltos y primeros niveles [M · 94]
- **ASM:** `entry.asm` L237–330 (lab_0238 y los primeros bloques de nivel).
- **Destino:** `src/game_flow.c`.
- **Notas:** los 7 bloques son casi iguales (init, `process_keyboard`, `poll_joystick`, `play_sound`, `update_animation`, update específico, flag de salida). Escribir un `run_level(n)` genérico con la tabla de funciones por nivel (init/update/exit) en lugar de 7 copias, **solo si** se documenta cada diferencia entre bloques.
- **Verificar:** la tabla por nivel coincide con la lectura del ASM (anotar las llamadas por nivel en un comentario).

### T43 — `entry.asm` parte 4: niveles restantes y cierre [M · 137]
- **ASM:** `entry.asm` L331–467.
- **Destino:** `src/game_flow.c`.
- **Notas:** incluye `init_level3_enemy` (lab_0394) y `init_level7_objects`; completar la tabla de T42.
- **Verificar:** `run_level` para cada n=1..7 arranca y termina por flag de salida en un test con entrada simulada.

### T44 — `level_transition` y paleta [M · 78]
- **ASM:** `enemy.asm` L114–158 (`level_transition`) y L242–274 (`set_palette`, `set_ega_palette`).
- **Destino:** `src/game_flow.c`, `src/video.c`.
- **Notas:** la paleta CGA/EGA se traduce a una tabla de colores SDL (selección de paleta/intensidad); `int 0x10` no existe.
- **Verificar:** test que llama `set_palette` con los valores del ASM y confirma el color resultante de los 4 índices CGA.

### T45 — `animate_screen_wipe` [M · 83]
- **ASM:** `enemy.asm` L159–241.
- **Destino:** `src/game_flow.c`.
- **Notas:** llama `calc_cga_addr`, `play_wipe_note` y `wipe_sound_start` (esta última es un `ret` vacío, ver §6e). El wipe es una animación de duración fija: puede ser bloqueante (cutscene) o convertirse en estado por frame; documentar la elección.
- **Verificar:** correr el wipe sobre una pantalla llena y comprobar que termina con todos los bytes en el valor final del ASM.

### T46 — `show_level_result` y `draw_result_frame` [M · 84]
- **ASM:** `enemy.asm` L275–358.
- **Destino:** `src/game_flow.c`.
- **Datos:** `result_melody_bonus=0x5aa3`, `cga_palette_table` (ver §5k).
- **Notas:** llama `init_result_melody`, `play_result_note`, `show_extra_life`, `love_scene_outro` (T58) y `silence_speaker`.
- **Verificar:** renderizar el marco a PPM; revisar que se llama la melodía correcta según el resultado.

### T47 — `handle_level_complete` [M · 128]
- **ASM:** `level_objects.asm` L1100–1227.
- **Destino:** `src/game_flow.c` (o `src/score.c`).
- **Notas:** `run_victory_sequence` ya escribe `l7_completion_counter`/`l7_completion_tick` esperando esta función; conectarlos.
- **Verificar:** simular fin de nivel con `difficulty_level` y puntaje conocidos; comprobar los incrementos esperados.

### T48 — Barra de bonus A [M · 77]
- **ASM:** `level_objects.asm` L1228–1304 (`save_score_regions`, `flash_score_color`, `print_bonus_score`, `print_level7_bonus`, `mask_score_tiles`).
- **Destino:** `src/score.c`.
- **Verificar:** `print_bonus_score` con un valor BCD conocido, comparando el volcado de píxeles con el patrón de `digit_sprites`.

### T49 — Barra de bonus B [M · 71]
- **ASM:** `level_objects.asm` L1305–1375 (`reloc_8` y datos, `animate_score_bar`, `binary_to_bcd`).
- **Destino:** `src/score.c`.
- **Verificar:** `binary_to_bcd` para 0, 9, 10, 99, 100, 255 y 65535 contra el cálculo directo; `animate_score_bar` termina y deja la barra completa.

---

## FASE 4: UI y entrada

### T50 — Texto: helpers y fuente [M · ~75]
- **ASM:** `ui.asm` L203–220 (`print_string`), L237–250 (`set_cursor`), L383–435 (`wait_for_input`, `display_text_line`, `clear_cga`).
- **Destino:** nuevo `src/ui.c` + `include/ui.h`.
- **Notas:** el original imprime con BIOS (`int 0x10`) usando la fuente ROM de 8×8 del PC, que **no** está en el ASM. Incluir una fuente 8×8 de dominio público embebida en `src/font8x8.c` y dibujar con `blit_to_cga`.
- **Verificar:** imprimir "ALLEY CAT" y volcar a PPM; comprobar legibilidad.

### T51 — Hardware y arranque [S · ~75]
- **ASM:** `ui.asm` L7–54 (`detect_video`, `print_startup_msg`) y `hardware.asm` L227–264 (`check_special_keys`).
- **Destino:** `src/ui.c`, `src/input.c`.
- **Notas:** `detect_video`, `install_handlers`/`restore_handlers` y la lectura de ROM id no necesitan portarse (SDL los reemplaza); documentar qué hace cada rama y qué valor fijo se usa (`rom_id=0xFF`). `check_special_keys` sí se porta (pausa, sonido, etc.).
- **Verificar:** test de `check_special_keys` con cada tecla especial simulada.

### T52 — `show_title_screen` [M · 109]
- **ASM:** `ui.asm` L55–163.
- **Destino:** `src/ui.c`.
- **Datos:** `title_music_pos=0x5320`, `title_music_tick=0x5322`, `title_music_freqs=0x5324`, `title_music_seq=0x538c`, `attract_*` (ver `data_segment_labels.txt`), sprites de `title_sprites` (verificados en §5k).
- **Verificar:** renderizar la pantalla de título a PPM; la música avanza con `play_music_note`.

### T53 — Gato y ícono del título [S · 55]
- **ASM:** `ui.asm` L164–202 (`move_title_cat`) y L221–236 (`animate_title_icon`).
- **Destino:** `src/ui.c`.
- **Datos:** `attract_anim_idx=0x6a8d`, `attract_icon_ptrs=0x6a8f`, `attract_icon_sprite_a=0x6d37`, `attract_icon_sprite_b=0x6d63`.
- **Verificar:** 50 ticks del gato del título; posición y frame dentro de rango.

### T54 — `show_attract_mode` [M · 76]
- **ASM:** `ui.asm` L307–382.
- **Destino:** `src/ui.c`.
- **Datos:** `attract_timing=0x56da`, `title_joy_offset=0x6d8f`.
- **Verificar:** la rutina termina al simular una tecla; el texto mostrado coincide con el del ASM.

### T55 — `show_pause_menu` [S · 56]
- **ASM:** `ui.asm` L251–306.
- **Destino:** `src/ui.c`.
- **Datos:** `pause_counter=0x6e00`, `title_saved_cx=0x6dfc`, `title_saved_dx=0x6dfe`.
- **Notas:** guarda y restaura una región con `save_from_cga`/`blit_to_cga`.
- **Verificar:** pausa → reanudar deja la pantalla idéntica a la anterior (diff de `cga_mem`).

### T56 — Cupid A: estado y movimiento [M · 105]
- **ASM:** `ui.asm` L571–675 (`reset_cupid`, `update_cupid`).
- **Destino:** nuevo `src/cupid.c` + `include/cupid.h`.
- **Datos:** `cupid_prev_x=0x70ec`, `cupid_anim_tick=0x70ee`, `cupid_arrow_x=0x70f0`, `cupid_active=0x70f2`, `cupid_x=0x70f3`, `cupid_y=0x70f5`, `cupid_dir=0x70f6`, `cupid_drawn=0x70f7`, `cupid_erase_addr=0x70f8`, `cupid_draw_addr=0x70fa`.
- **Notas:** `level7_epilogue.c` tiene sus propios `cupid_active/x/y` locales (L196–199): sustituirlos por los de `cupid.h`.
- **Verificar:** activar el cupid y avanzar 100 ticks; `cupid_x` recorre el rango esperado.

### T57 — Cupid B: dibujo, ventanas y colisión [M · 116]
- **ASM:** `ui.asm` L676–791 (`draw_cupid`, `erase_cupid`, `cupid_toggle_window`, `check_cupid_collision`).
- **Destino:** `src/cupid.c`; quitar los stubs de `level7_epilogue.c` (`erase_cupid_stub`).
- **Datos:** `window_open_state=0x2be2`, `window_row_y_table=0x2bd4`, `window_row_col_offset=0x2bdb`, `cupid_sprite_offset=0x70fc`, `cupid_sprite_end=0x7120`.
- **Notas:** `cupid_toggle_window` es el segundo escritor de `window_open_state` (ver §6f). Llama `draw_bg_tile` (T39).
- **Verificar:** alternar una ventana dos veces deja `window_open_state` como al inicio; `check_cupid_collision` con y sin solape.

### T58 — `love_scene_outro` [M · 82]
- **ASM:** `ui.asm` L489–570.
- **Destino:** `src/ui.c`.
- **Datos:** `title_scroll_pos=0x6f24`, `title_scroll_tick_1=0x6f26`, `title_scroll_tick_2=0x6f28`.
- **Notas:** cutscene final; bloqueante está justificado (una sola vez). Usa `check_vsync` (no-op).
- **Verificar:** corre hasta terminar bajo `timeout 30s`; PPM del último frame.

### T59 — Joystick en UI [S · 53]
- **ASM:** `ui.asm` L436–488 (`detect_joystick`, `test_joystick_axis`).
- **Destino:** `src/ui.c`.
- **Notas:** **Opcional.** Mapear a mando SDL o dejar siempre "no detectado" y documentarlo. Datos: `title_input_tick=0x6dfa`.
- **Verificar:** ejecución con y sin dispositivo simulado.

### T60 — Joystick en input [M · 86]
- **ASM:** `input.asm` L4–89 (`poll_joystick`, `decode_joystick_axis`).
- **Destino:** `src/input.c`.
- **Datos:** `joy_button=0x069a`, `joy_timer=0x069c`, `joy_pending=0x069e`, `joy_last_tick=0x069f`.
- **Notas:** **Opcional**, igual que T59. Si se hace, mapear ejes SDL a `scroll_direction`/`in_level_mode` con la misma codificación −1/0/1.
- **Verificar:** eje simulado a izquierda/derecha produce las direcciones esperadas.

### T61 — Verificación de `read_keyboard_dirs` y `process_keyboard` [M · 104]
- **ASM:** `input.asm` L90–193.
- **Destino:** `src/input.c` (solo corregir si hay diferencias).
- **Qué hacer:** comparar línea por línea con `input_poll`/`input_process_keys`; anotar en PROGRESS.md qué teclas coinciden (la tabla de scancodes ya está recuperada en §1).
- **Verificar:** test con cada tecla del juego simulada; efectos en `scroll_direction`, `in_level_mode`, `sound_enabled` (toggle `~`), pausa, salida.

---

## FASE 5: auditoría, integración y cierre

### T70 — Auditoría de `update_animation` A [L · 128]
- **ASM:** `game_loop.asm` L158–285 (`lab_08fc` a `lab_0a1a`: despacho por `level_number` y el bloque del nivel 2, ya portado en `select_cat_sprite`/`update_cat_movement`).
- **Qué hacer:** (1) **solo lectura**: tabla `lab_XXXX → función C` y lista de ramas sin portar; (2) portar las que falten (muerte por ahogamiento, `play_death_melody`, `play_meow_sound`, bordes de color).
- **Destino:** `src/movement.c`/`src/animation.c`. **Verificar:** test por rama; comparar con los tests de §5b/§5e.

### T71 — Auditoría de `update_animation` B [L · 155]
- **ASM:** `game_loop.asm` L286–440 (`lab_0a2e` a `lab_0bab`).
- **Mismo método que T70.** Atención a `walk_frame`/`scroll_speed`/`anim_accumulator`.

### T72 — Auditoría de `update_animation` C [L · 244 → partir en dos]
- **ASM:** `game_loop.asm` L441–684 (`lab_0bac` a `lab_0e1f`): la máquina `entry_steps`/`at_platform` y las llamadas a `check_level_objects`, `check_jump_collision`, `check_dog_collision`.
- **Corte:** C1 = L441–560, C2 = L561–684. Mismo método que T70.

### T73 — Auditoría de `update_animation` D [M · 133]
- **ASM:** `game_loop.asm` L685–817 (`lab_0e23` a `lab_0f86`): climb/transition (ya en §5m/§5n) y `lab_0f63`.
- **Mismo método que T70.** Conectar `check_level_objects` (T34) y `check_window_landing` (T10).

### T74 — Reemplazar el loop demo por el flujo real [M]
- **Destino:** `src/main.c`, `src/game_flow.c`.
- **Qué hacer:** `main()` solo inicializa SDL/audio y llama `game_run()`; eliminar el `level_number = 3` fijo, los `printf` de ayuda y `difficulty_level = 5`. Mantener `SDL_Delay`/tick de 18.2 Hz para los timers BIOS.
- **Verificar:** arrancar con título → partida → muerte → selector de nivel → nivel → resultado, en una ejecución headless de 2 minutos sin crash.

### T75 — Checklist de cableado por nivel [M]
- **Qué hacer:** una tabla en `docs/NIVELES.md` con, por nivel 0–7, cada función del ASM que `entry.asm` llama (init, update, exit) y la función C que la cubre. Cualquier celda vacía abre una tarea nueva.
- **Verificar:** ninguna celda vacía.

### T76 — Fidelidad: recorte parcial del perro [M]
- **ASM:** `enemy.asm`, `update_enemy_viewport` (ver su `copy_with_stride` y el suma de `enemy_approach_timer` al puntero).
- **Destino:** `src/enemy.c`. Portar el "reveal" gradual en lugar de mostrar el frame completo (§5p).
- **Verificar:** PPM de la secuencia de aproximación en 5 ticks.

### T77 — Fidelidad: sprite del salto del pez y flags sin nombre [M]
- **ASM:** `level_physics.asm` (`update_cat_jump`, rama que dibuja al pez) y las lecturas de `[0x418]`, `[0x556]`, `[0x558]`, `[0x552]`, `[0x553]`, `[0x410]`.
- **Qué hacer:** extraer el sprite del pez (método §3/§4) y buscar, con `grep` en todo el ASM, **quién escribe** cada flag; reemplazar los "siempre 0" por el valor real.
- **Verificar:** el pez se ve durante el salto; cada flag documentado con su escritor.

### T78 — Fidelidad: explosión (borde) y cutscenes [S]
- **Qué hacer:** modelar el registro de borde de color (`int 0x10` AH=0x0B) en `video.c` y usarlo en `play_explosion_effect`; decidir si las cutscenes bloqueantes del nivel 7 se dejan como están (recomendado) y dejarlo escrito.
- **Verificar:** el borde cambia a rojo y vuelve a negro durante la explosión.

### T79 — Prueba completa y ordenamiento de PROGRESS.md [M]
- **Qué hacer:** (1) ejecutar los 7 niveles con entrada simulada y registrar resultados en `docs/NIVELES.md`; (2) reordenar PROGRESS.md (secciones 5x–5z después de la 6, correcciones superpuestas), dejando §0 con Current focus / Todo / Blockers al día; (3) mover lo completado a "Completed".
- **Verificar:** `make` y `make test` limpios; PROGRESS.md sin contradicciones.

---

## Orden recomendado y dependencias

```
T00 → T01 → T02
T10, T11, T12 → T13 → T14 → T15 → T16 → T17 → T18 → T19      (callejón)
T20 → T21 → T22 → T23 → T24                                    (L3 y L4)
T25 → T26 → T27 → T28                                          (L5)
T29 → T30 → T31 → T32   (T32 necesita T26)                     (L6)
T33 → T34 → T35 → T36                                          (L2)
T37 → T38 → T39                                                (L7)
T50 → T51 → T52 → T53 → T54 → T55 → T56 → T57 → T58            (UI; T57 necesita T39)
T40 → T41 → T42 → T43 (después de T50–T55 para enlazar)
T44 → T45 → T46 (T46 necesita T58) → T47 → T48 → T49
T59, T60 opcionales; T61
T70 → T71 → T72 → T73 → T74 → T75 → T76 → T77 → T78 → T79
```

## Resumen de tareas

| Fase | Tareas | Total aprox. |
|---|---|---|
| 0 Herramientas | T00–T02 | 3 |
| 1 Callejón | T10–T19 | 10 |
| 2 Niveles | T20–T39 | 20 |
| 3 Flujo | T40–T49 | 10 |
| 4 UI/entrada | T50–T61 | 12 |
| 5 Cierre | T70–T79 (T72 en dos) | 11 |
| **Total** | | **66** |
