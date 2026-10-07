# Checklist de cableado por nivel (T75)

Para cada nivel 0-7: cada funcion que `entry.asm` llama (init, update, salida) y la funcion C que la cubre.
`entry.asm` = `asm/src/entry.asm` de gmegidish/alleycat-disassembly (los numeros `L` son lineas de ese archivo).
`game_flow.c` = `src/game_flow.c` (`game_level_enter` = init, `game_level_frame` = update, `game_level_exit` = salida).

**Verificacion automatica:** `python3 tools/check_niveles.py` (exit 0 = ninguna celda vacia, y la secuencia de llamadas
de cada bloque de C es exactamente la de `entry.asm`, con la tabla de equivalencias de tareas.md §0.5; compara tambien
los flags de salida de cada loop). Las tablas de abajo las genera `python3 tools/check_niveles.py --emit`.

**Resultado T75: ninguna celda vacia.** Hay 1 desviacion de comportamiento (D1) y 3 notas (D2-D4), ver el final.

## Despacho (lab_0238, tabla de saltos `cs:0x250`)

| `level_number` | destino ASM | bloque C (`game_level_enter`) |
|---|---|---|
| 0 y 1 | `lab_03e2` (fija `level_number=1`) | `lab_03e2` |
| 2 | `lab_0459` | `lab_0459` -> `level2_init()` |
| 3 | `lab_0394` | `lab_0394` |
| 4 | `lab_0349` | `lab_0349` |
| 5 | `lab_02fe` | `lab_02fe` |
| 6 | `lab_02aa` | `lab_02aa` |
| 7 | `lab_0260` | `lab_0260` |
| > 7 | clamp a 0 (`sub bx,bx`) -> `lab_03e2` | `bx = 0` -> `default:` |

## Salida de nivel (lab_0427, comun a los niveles 1-7)

| entry.asm | ASM | C |
|---|---|---|
| L425-428 | `restart_game != 0` -> `lab_00ae` | `game_level_exit`: `GF_TO_00AE` |
| L429-432 | `show_attract != 0` -> `lab_00a3` | `GF_TO_00A3` |
| L433-436 | `object_hit != 0` -> `start_in_level = 0` | `start_in_level = 0x0` |
| L437-439 | `level_state = level_number; level_number = 0` | idem |
| L440 | `call level_transition` | `level_transition()` (`src/transition.c:14`) |
| L441 | `jmp lab_00f3` (setup del callejon) | `GF_TO_00F3` |

## Muerte en el callejon y selector de nivel (lab_01b7..lab_022a)

| entry.asm | ASM | C |
|---|---|---|
| L181-194 | `int 0x1a` -> `game_tick`; `saved_cat_x/y`; `start_in_level=1`; `force_level7` | `game_death_handler` (`src/game_flow.c`) |
| L201-229 | seleccion ponderada (`random` x2, `level_pool`, `level_pool_hard`, historial `last_level/prev_level`) | `select_next_level` (`src/game_flow.c`) |
| L230-237 | `lab_0238` | `GF_TO_0238` -> `game_level_enter()` |

Los niveles 2 y 7 no salen de ninguna tabla: el 2 solo por `level_complete` del bloque 0/1 (lab_0403) y el 7 solo por `force_level7`.

## Tablas por nivel (generadas)

## Callejon (nivel 0)

### Setup (lab_00f3..lab_0155)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L119 | `clear_screen` | `clear_screen` | `src/alley_drawing.c:170` |
| L120 | `render_sprites` | `render_sprites` | `src/alley_drawing.c:203` |
| L122 | `silence_speaker` | `silence_speaker` | `src/sound.c:101` |
| L126 | `setup_level` | `setup_level` | `src/game_setup.c:78` |
| L133 | `setup_alley` | `setup_alley` | `src/game_setup.c:23` |
| L135 | `init_sound` | `init_sound` | `src/enemy.c:34` |
| L136 | `init_player` | `init_player` | `src/fall_object.c:25` |
| L137 | `reset_jump` | `reset_jump` | `src/fall_object.c:20` |
| L138 | `init_objects` | `init_cycle_objects` | `src/cycle_objects.c:53` |
| L139 | `draw_score` | `draw_high_score_display` | `src/score.c:64` |
| L140 | `draw_high_score` | `draw_current_score` | `src/score.c:60` |
| L141 | `init_music` | `init_music` | `src/sound.c:544` |

### Loop (lab_0155..lab_01b7)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L149 | `process_keyboard` | `input_process_keys` | `src/input.c:81` |
| L158 | `poll_joystick` | `poll_joystick` | `src/joystick.c:31` |
| L159 | `update_animation` | `update_animation` | `src/update_animation.c:11` |
| L160 | `update_enemies` | `update_enemies` | `src/enemy.c:207` |
| L167 | `play_sound` | `play_sound` | `src/sound.c:170` |
| L168 | `update_thrown_objects` | `update_thrown_objects` | `src/throw.c:155` |
| L169 | `update_cat_jump` | `update_cat_jump` | `src/jump_gravity.c:152` |
| L170 | `apply_cat_gravity` | `apply_cat_gravity` | `src/jump_gravity.c:84` |
| L171 | `animate_falling` | `animate_falling` | `src/fall_object.c:85` |
| L172 | `cycle_animations` | `update_cycle_objects` | `src/cycle_objects.c:145` |
| L173 | `draw_lives` | `draw_lives` | `src/score.c:70` |

## Nivel 7

### Init (lab_0260)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L259 | `level_transition` | `level_transition` | `src/transition.c:14` |
| L260 | `draw_level_background` | `draw_level_background` | `src/level_background.c:377` |
| L261 | `setup_level` | `setup_level` | `src/game_setup.c:78` |
| L262 | `init_sound` | `init_sound` | `src/enemy.c:34` |
| L263 | `init_thrown_objects` | `init_thrown_objects` | `src/level_objects.c:135` |
| L264 | `reset_cupid` | `reset_cupid` | `src/cupid.c:24` |
| L265 | `init_level7_objects` | `init_level7_objects` | `src/level7_epilogue.c:165` |
| L266 | `init_music` | `init_music` | `src/sound.c:544` |

### Update (lab_027e)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L268 | `process_keyboard` | `input_process_keys` | `src/input.c:81` |
| L269 | `poll_joystick` | `poll_joystick` | `src/joystick.c:31` |
| L270 | `play_sound` | `play_sound` | `src/sound.c:170` |
| L271 | `update_animation` | `update_animation` | `src/update_animation.c:11` |
| L272 | `update_cupid` | `update_cupid` | `src/cupid.c:33` |
| L273 | `tick_level_thrown_objects` | `tick_level_thrown_objects` | `src/level7_epilogue.c:381` |
| L274 | `spawn_thrown_object` | `spawn_thrown_object` | `src/level7_epilogue.c:296` |
| L275 | `update_level7_objects` | `update_level7_objects` | `src/level7_epilogue.c:476` |

Flags de salida: `cat_died`, `cat_caught`, `show_attract`, `restart_game`

## Nivel 6

### Init (lab_02aa)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L286 | `level_transition` | `level_transition` | `src/transition.c:14` |
| L287 | `draw_level_background` | `draw_level_background` | `src/level_background.c:377` |
| L288 | `level6_stubs` | — (`ret` desnudo (T29): no hay funcion C) | — |
| L289 | `setup_level` | `setup_level` | `src/game_setup.c:78` |
| L290 | `init_thrown_objects` | `init_thrown_objects` | `src/level_objects.c:135` |
| L291 | `init_sound` | `init_sound` | `src/enemy.c:34` |
| L292 | `init_music` | `init_music` | `src/sound.c:544` |

### Update (lab_02c5)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L294 | `process_keyboard` | `input_process_keys` | `src/input.c:81` |
| L295 | `poll_joystick` | `poll_joystick` | `src/joystick.c:31` |
| L296 | `play_sound` | `play_sound` | `src/sound.c:170` |
| L297 | `update_level6_movement` | `update_level6_movement` | `src/level6.c:252` |
| L298 | `update_level6_timing` | `update_level6_timing` | `src/level6.c:187` |
| L299 | `update_animation` | `update_animation` | `src/update_animation.c:11` |
| L302 | `update_enemies` | `update_enemies` | `src/enemy.c:207` |
| L305 | `tick_thrown_objects` | `tick_thrown_objects` | `src/level_objects.c:149` |

Flags de salida: `cat_died`, `object_hit`, `cat_caught`, `restart_game`, `show_attract`

## Nivel 5

### Init (lab_02fe)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L318 | `level_transition` | `level_transition` | `src/transition.c:14` |
| L319 | `draw_level_background` | `draw_level_background` | `src/level_background.c:377` |
| L320 | `init_level5_objects` | `init_level5_objects` | `src/level5.c:113` |
| L321 | `setup_level` | `setup_level` | `src/game_setup.c:78` |
| L322 | `init_thrown_objects` | `init_thrown_objects` | `src/level_objects.c:135` |
| L323 | `init_sound` | `init_sound` | `src/enemy.c:34` |
| L324 | `init_music` | `init_music` | `src/sound.c:544` |

### Update (lab_0319)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L326 | `process_keyboard` | `input_process_keys` | `src/input.c:81` |
| L327 | `poll_joystick` | `poll_joystick` | `src/joystick.c:31` |
| L328 | `play_sound` | `play_sound` | `src/sound.c:170` |
| L329 | `update_level5_objects` | `update_level5_objects` | `src/level5.c:292` |
| L330 | `update_level5_anim` | `update_level5_anim` | `src/level5.c:165` |
| L331 | `update_animation` | `update_animation` | `src/update_animation.c:11` |
| L332 | `tick_thrown_objects` | `tick_thrown_objects` | `src/level_objects.c:149` |
| L333 | `update_enemies` | `update_enemies` | `src/enemy.c:207` |

Flags de salida: `object_hit`, `cat_caught`, `cat_died`, `show_attract`, `restart_game`

## Nivel 4

### Init (lab_0349)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L345 | `level_transition` | `level_transition` | `src/transition.c:14` |
| L346 | `draw_level_background` | `draw_level_background` | `src/level_background.c:377` |
| L347 | `setup_level` | `setup_level` | `src/game_setup.c:78` |
| L348 | `init_thrown_objects` | `init_thrown_objects` | `src/level_objects.c:135` |
| L349 | `init_sound` | `init_sound` | `src/enemy.c:34` |
| L350 | `init_level4_objects` | `init_level4_objects` | `src/level4.c:66` |
| L351 | `init_music` | `init_music` | `src/sound.c:544` |

### Update (lab_0364)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L353 | `process_keyboard` | `input_process_keys` | `src/input.c:81` |
| L354 | `poll_joystick` | `poll_joystick` | `src/joystick.c:31` |
| L355 | `play_sound` | `play_sound` | `src/sound.c:170` |
| L356 | `update_animation` | `update_animation` | `src/update_animation.c:11` |
| L357 | `update_level4_state` | `update_level4_state` | `src/level4.c:278` |
| L358 | `update_level4_anim` | `update_level4_anim` | `src/level4.c:155` |
| L359 | `tick_thrown_objects` | `tick_thrown_objects` | `src/level_objects.c:149` |
| L360 | `update_enemies` | `update_enemies` | `src/enemy.c:207` |

Flags de salida: `object_hit`, `cat_caught`, `cat_died`, `show_attract`, `restart_game`

## Nivel 3

### Init (lab_0394)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L372 | `level_transition` | `level_transition` | `src/transition.c:14` |
| L373 | `draw_level_background` | `draw_level_background` | `src/level_background.c:377` |
| L374 | `setup_level` | `setup_level` | `src/game_setup.c:78` |
| L375 | `init_thrown_objects` | `init_thrown_objects` | `src/level_objects.c:135` |
| L376 | `init_sound` | `init_sound` | `src/enemy.c:34` |
| L377 | `init_level3_doors` | `init_level3_doors` | `src/level3_enemy.c:244` |
| L378 | `init_level3_enemy` | `init_level3_enemy` | `src/level3_enemy.c:109` |
| L379 | `init_music` | `init_music` | `src/sound.c:544` |

### Update (lab_03b2)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L381 | `process_keyboard` | `input_process_keys` | `src/input.c:81` |
| L382 | `poll_joystick` | `poll_joystick` | `src/joystick.c:31` |
| L383 | `play_sound` | `play_sound` | `src/sound.c:170` |
| L384 | `update_animation` | `update_animation` | `src/update_animation.c:11` |
| L385 | `update_level3_enemy` | `update_level3_enemy` | `src/level3_enemy.c:118` |
| L386 | `update_level3_doors` | `update_level3_doors` | `src/level3_enemy.c:264` |
| L387 | `tick_thrown_objects` | `tick_thrown_objects` | `src/level_objects.c:149` |
| L388 | `update_enemies` | `update_enemies` | `src/enemy.c:207` |

Flags de salida: `object_hit`, `cat_caught`, `cat_died`, `show_attract`, `restart_game`

## Nivel 1 (y 0 si se fuerza a mano)

### Init (lab_03e2)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L401 | `level_transition` | `level_transition` | `src/transition.c:14` |
| L402 | `draw_level_background` | `draw_level_background` | `src/level_background.c:377` |
| L403 | `setup_level` | `setup_level` | `src/game_setup.c:78` |
| L404 | `init_thrown_objects` | `init_thrown_objects` | `src/level_objects.c:135` |
| L405 | `init_sound` | `init_sound` | `src/enemy.c:34` |
| L406 | `init_music` | `init_music` | `src/sound.c:544` |

### Update (lab_03fa)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L408 | `process_keyboard` | `input_process_keys` | `src/input.c:81` |
| L409 | `poll_joystick` | `poll_joystick` | `src/joystick.c:31` |
| L410 | `play_sound` | `play_sound` | `src/sound.c:170` |
| L411 | `update_animation` | `update_animation` | `src/update_animation.c:11` |
| L412 | `tick_thrown_objects` | `tick_thrown_objects` | `src/level_objects.c:149` |
| L413 | `update_enemies` | `update_enemies` | `src/enemy.c:207` |
| L414 | `update_entrance_anim` | `update_entrance_anim` | `src/level2.c:371` |

Flags de salida: `object_hit`, `cat_died`, `restart_game`, `show_attract`

## Nivel 2

### Init (lab_0459)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L446 | `level_transition` | `level_transition` | `src/transition.c:14` |
| L447 | `draw_level_background` | `draw_level_background` | `src/level_background.c:377` |
| L448 | `init_level2_objects` | `init_level2_objects` | `src/level2.c:30` |
| L449 | `setup_level` | `setup_level` | `src/game_setup.c:78` |
| L452 | `init_music` | `init_music` | `src/sound.c:544` |

### Update (lab_0478)

| entry.asm | ASM | C | definicion |
|---|---|---|---|
| L454 | `process_keyboard` | `input_process_keys` | `src/input.c:81` |
| L455 | `poll_joystick` | `poll_joystick` | `src/joystick.c:31` |
| L456 | `play_sound` | `play_sound` | `src/sound.c:170` |
| L457 | `update_animation` | `update_animation` | `src/update_animation.c:11` |
| L458 | `update_level2_objects` | `update_level2_objects` | `src/level2.c:217` |
| L459 | `animate_level2_blocks` | `animate_level2_blocks` | `src/level2.c:334` |

Flags de salida: `object_hit`, `cat_caught`, `show_attract`, `restart_game`

check_niveles: OK (callejon + niveles 1-7: ninguna celda vacia, secuencias iguales a entry.asm)

## Desviaciones y notas

**D1 — `game_alley_frame` pone `immune_flag = 0` en cada frame del callejon (NO esta en `entry.asm`).**
En el ASM `immune_flag` se pone a 1 en `lab_09b9` (`game_loop.asm` L246, pose de entrada) y solo se limpia en
`setup_level` para el nivel 2 (`game_loop.asm` L103). La linea `src/game_flow.c:237` es un resto del loop de `main.c` (T19).
Quitarla hace fallar `tests/test_game_flow_loop.c` (E4 comprueba `immune_flag == 0` tras el frame, con `immune_flag = 5` de
entrada): la desviacion esta fijada por un test, asi que no se toca dentro de T75. Abierta como **T75b**.

**D2 — `level6_stubs` (`entry.asm` L288):** es un `ret` desnudo (T29); `game_level_enter` lo omite a proposito.
No es una celda vacia.

**D3 — arranque (`detect_video`, `read_rom_id`, `install_handlers`, `init_bios_data`, L31-40):** SDL los reemplaza (T40,
`game_hw_init`). Fuera del alcance de la checklist por nivel.

**D4 — nivel 0 vs 1:** el bloque `lab_03e2` fija `level_number = 1` para los niveles 0 y 1 (misma entrada de la tabla de saltos).
El nivel 0 "jugable" es el callejon (`game_alley_frame`), no ese bloque. `game_level_frame` mantiene `case 0:` junto a `case 1:`
solo por si alguien deja `level_number = 0` a mano.

## Estado de la entrada/salida por nivel (`make test-soak-levels`)

`tests/test_soak.c` corre cada nivel con entrada simulada y comprueba que entra y sale por el flag esperado
(la prueba de partida completa y el registro de resultados por nivel son T79):

| nivel | frames hasta salir | `level_number` al salir |
|---|---|---|
| 0 | 1442 | 2 |
| 1 | 790 | 1 |
| 2 | 811 | 2 |
| 3 | 943 | 3 |
| 4 | 1579 | 4 |
| 5 | 1000 | 5 |
| 6 | 52654 | 6 |
| 7 | 247 | 7 |
