# Plan para finalizar el port Alley Cat (C/SDL2)

Fecha: 2026-10-04
Repo del port: https://github.com/fschpp/alleycat_recomp_port_sdl (`main`, último commit 2026-09-18 "avances alley.asm")
Referencia ASM: https://github.com/gmegidish/alleycat-disassembly

Regla de trabajo: no portar lógica sin verificarla contra el ASM real. Traducción literal con `goto` en rutinas ramificadas, offsets resueltos con `tools/resolve_data_segment.py`, nunca por el nombre de la etiqueta.

---

## Aviso previo: `main` de GitHub está desactualizado

En `main` no aparecen `check_window_landing`, `check_stairs_collision` ni `reset_window_state` real (sigue como stub en `game_setup.c`). Si ese trabajo está solo en la copia local, hacer push antes de continuar.

No se pudo compilar en el sandbox (falta `libsdl2-dev`). El estado del build sale del PROGRESS.md, que dice que compila sin warnings.

---

## Estado actual

Hecho y verificado: CGA, sprites, movimiento, enemigos (perro, pez, objetos de patrulla, objeto que cae), HUD de score/vidas, `sound.asm` con PC speaker emulado, pipeline de `alley.asm`, fondos de los niveles 1 a 6, epílogo del nivel 7, pájaro del nivel 3, lluvia de objetos y huellas.

## Trabajo pendiente (líneas de ASM aproximadas)

| Bloque | Qué falta | Líneas |
|---|---|---|
| `level_objects.asm` | Niveles 2, 4, 5 y 6 (objetos y enemigos), barra de bonus | ~1800 |
| `ui.asm` | Título, pausa, attract, joystick, cupid, `love_scene_outro` | ~640 |
| `game_loop.asm` | `update_animation`: confirmar que esté cableado completo | ~660 |
| `throw.asm` | Objetos lanzados desde ventanas; mantiene `current_floor` y `window_column` | ~170 |
| `alley_drawing.asm` | Escena del callejón, edificios, ventanas, ícono de dificultad | ~220 |
| `enemy.asm` | Wipe de pantalla, paleta, pantalla de resultado | ~200 |
| `hardware.asm`, `input.asm` | Handlers de teclado/timer, joystick | ~180 |
| Menores | `update_viewport`, `render_sprites`, `pixel_to_bitmask` | ~75 |

---

## Current focus

Cerrar el callejón (nivel 0) de punta a punta: `throw.asm`, `window_open_state` y `alley_drawing.asm`.

## Todo List

### Fase 0: base
- [ ] Hacer push del trabajo local y alinear `main` con la copia local
- [ ] Confirmar que `make` compila y corre en el entorno local
- [ ] Revisar que `main.c` use `update_animation` completo y no pedazos de demo
- [ ] Actualizar PROGRESS.md (sección 0) con este plan

### Fase 1: callejón jugable (nivel 0)
- [ ] `throw.asm` completo: `update_thrown_objects`, `generate_throw_object`, `generate_throw_pattern`, `rotate_throw_bits`, `check_throw_range` (cierra el ítem (h))
- [ ] `window_open_state`: toggling desde `ui.asm` (~715) y `level_objects.asm` (~246)
- [ ] Desbloquear niveles 0 y 7 en `check_level_collision` (`check_window_landing`, `check_stairs_collision`)
- [ ] `alley_drawing.asm`: `draw_alley_scene`, `draw_all_buildings`, `draw_building`, `draw_all_windows`, `draw_window_strip`, `draw_alley_details`, `draw_object_row`, `init_alley_objects`, `draw_difficulty_icon`
- [ ] `update_viewport` (modelar el área de scratch DS 0x000e)
- [ ] `render_sprites` (sprites decorativos del callejón; verificar sus 3 tablas de punteros)
- [ ] `check_dog_collision` real (landing del nivel 0)
- [ ] `pixel_to_bitmask`

### Fase 2: niveles interiores (`level_objects.asm`)
Orden de menor a mayor dependencia.
- [ ] Nivel 3: `init_level3_doors`, `close_level3_door`
- [ ] Nivel 4: `update_level4_state`, `update_level4_anim`, `init_level4_objects`, `randomize_l4_pos`, `calc_l4_obj_pos`, `check_l4_*`, `erase_level4_sprite`, y el tail diferido de `init_level4_bg`
- [ ] Nivel 5: `update_level5_anim`, `update_level5_objects`, `check_l5_*`, `calc_l5_direction`, `draw_l5_perch`, `erase_l5_perch`
- [ ] Nivel 6: `update_level6_timing`, `update_level6_movement`, `prepare_l6_erase`, `clear_l6_object`, `refresh_l6_display`, `check_l6_proximity`, `draw_l6_alert`, `draw_l6_tracker`, `erase_l6_tracker`, `draw_l6_tile`, `calc_l6_addr`
- [ ] Nivel 2: `check_level_objects`, `init_level2_objects`, `update_level2_objects`, `animate_level2_blocks`, `erase_level_object`, `reset_caught_objects`, `draw_bg_tile`, `update_entrance_anim`
- [ ] Re-verificar el nivel 1 contra el resto

### Fase 3: flujo del juego
- [ ] `entry.asm` completo: selector de nivel ponderado, tabla de saltos por nivel, game over
- [ ] `show_level_result`, `draw_result_frame`
- [ ] `animate_screen_wipe`, `set_palette`, `set_ega_palette`
- [ ] Barra de bonus: `animate_score_bar`, `binary_to_bcd`, `print_bonus_score`, `print_level7_bonus`, `flash_score_color`, `save_score_regions`, `mask_score_tiles`
- [ ] `handle_level_complete` (hoy stub en el epílogo del nivel 7)

### Fase 4: UI y entrada
- [ ] `show_title_screen`, `move_title_cat`, `animate_title_icon`
- [ ] `show_attract_mode`, `show_pause_menu`
- [ ] Cupid: `reset_cupid`, `cupid_toggle_window`, `check_cupid_collision` (para que `check_l7_cupid` deje de dar siempre falso)
- [ ] `love_scene_outro`
- [ ] `check_special_keys`
- [ ] Joystick (opcional si solo se usa teclado): `poll_joystick`, `detect_joystick`, `test_joystick_axis`, `decode_joystick_axis`
- [ ] `install_handlers`, `restore_handlers`, `detect_video`: se resuelven con SDL, no hace falta portarlos

### Fase 5: cierre y fidelidad
- [ ] Quitar stubs y simplificaciones: recorte parcial del sprite del perro, sprite propio del pez, flash de borde en la explosión, flag `[0x410]`, flags sin nombre `[0x552]`, `[0x553]`, `[0x558]`, `[0x418]`, `[0x556]`
- [ ] Decidir si las cutscenes bloqueantes del nivel 7 se convierten a máquina de estados
- [ ] Partida completa por los 7 niveles con checklist por nivel
- [ ] Reordenar PROGRESS.md (secciones 5x a 5z están después de la 6) y limpiar correcciones superpuestas

---

## Latest Blockers/Discoveries

- El ítem (h) (`check_window_landing`, `check_stairs_collision`) depende de `throw.asm` y de `window_open_state` en `ui.asm`, no de `spawn_window_event`.
- `main` de GitHub no incluye el trabajo reciente de la máquina de estados de ventanas.
- Varios niveles dependen de flags sin nombre; resolverlos con `resolve_data_segment.py` antes de portar cada nivel.

## Riesgos

- La Fase 2 es la más grande: unas 1000 líneas de lógica ramificada en cuatro niveles. Mantener la traducción literal con `goto`.
- Orden dentro de la Fase 1: `throw.asm` antes que `ui.asm` y que los niveles 0 y 7.
- Los pasos que dependen de timing real (BIOS tick, retrace) siguen siendo aproximaciones documentadas; no mezclarlos con trabajo de fidelidad hasta la Fase 5.

## Completed

- [x] CGA framebuffer, input, video SDL2, RNG
- [x] Extracción y verificación de sprites (cat, enemy, object, extralife, title, death, digits)
- [x] Movimiento, climb, colisiones de niveles 1 a 6
- [x] Perro, objetos de patrulla, pez/gravedad, objeto que cae
- [x] Score/lives HUD
- [x] `sound.asm` con PC speaker y backend SDL2
- [x] `alley.asm` (save/restore, ventanas, muerte)
- [x] Fondos de niveles 1 a 6, pájaro del nivel 3, lluvia de objetos y huellas
- [x] Epílogo del nivel 7 (5 chunks)
