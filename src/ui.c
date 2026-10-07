/* ui.c — helpers de texto y espera de ui.asm (T50). Ver include/ui.h y PROGRESS.md §6au. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "ui.h"
#include "cat_state.h"
#include "cga.h"
#include "bios_text.h"
#include "gen/ds_pool.h"
#include "game_flow.h"
#include "score.h"
#include "sound.h"
#include "input.h"
#include "alley_drawing.h"
#include "update_animation.h"
#include "cycle_objects.h"
#include "fall_object.h"
#include "game_setup.h"
#include "enemy.h"
#include "hardware.h"
#include "speaker.h"

uint16_t keyboard_counter;                 /* DS 0x0693 */
uint16_t title_joy_offset;                 /* DS 0x6d8f */
uint16_t title_input_tick;                 /* DS 0x6dfa */
uint16_t title_saved_cx;                   /* DS 0x6dfc: tick (dx) guardado al pausar */
uint16_t title_saved_dx;                   /* DS 0x6dfe: palabra alta del tick (cx) */
uint16_t pause_counter;                    /* DS 0x6e00 */
uint16_t title_scroll_pos;                 /* DS 0x6f24: desplazamiento CGA del gatito/pareja del epilogo (T58) */
uint16_t title_scroll_tick_1;              /* DS 0x6f26: ultimo tick visto en la fase 1 del epilogo */
uint16_t title_scroll_tick_2;              /* DS 0x6f28: ultimo tick visto en la fase 2 del epilogo */
void (*ui_wait_hook)(void);
uint8_t (*joy_port_fn)(void);

#define DS_TEXT_PTRS  0x6d37               /* attract_icon_sprite_a: punteros a cadenas (word) */
#define DS_TEXT_CURS  0x6d63               /* attract_icon_sprite_b: cursor (word, dh = fila) */

static uint16_t ui_ds_word(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

/* print_string (L203-214): lodsb / cmp al,0 / jz fin / teletype (bl=2, ah=0xe) / loop. */
const uint8_t *print_string(const uint8_t *si) {
lab_5e2d:
    {
        uint8_t al = *si++;                                /* lodsb */
        if (al == 0x0) goto lab_5e3a;                      /* cmp al,0 / jz */
        bios_teletype(al, 0x2);                            /* mov bl,2 / mov ah,0xe / int 0x10 */
    }
    goto lab_5e2d;
lab_5e3a:
    return si;
}

/* set_cursor (L237-242): dl=0, bh=dl=0, ah=2, int 0x10: cursor a (dh, 0). */
void set_cursor(uint8_t dh) {
    bios_set_cursor(dh, 0x0);
}

/* wait_for_input (L383-397). Con joystick: repite `in al,0x201 / and al,0x10 / jnz` hasta que el bit 4 sea 0.
 * Sin joystick: toma keyboard_counter y espera a que cambie (la ISR de INT 9 lo incrementa). El port llama a
 * ui_wait_hook en cada vuelta para que main.c bombee SDL (si no, la ventana se congelaria). */
void wait_for_input(void) {
    uint16_t ax;
    if (use_joystick == 0x0) goto lab_5fa7;                /* cmp byte [use_joystick],0 / jz */
lab_5f9e:
    if (ui_wait_hook) ui_wait_hook();
    {
        uint8_t al = joy_port_fn ? joy_port_fn() : 0xff;   /* mov dx,0x201 / in al,dx */
        if ((al & 0x10) != 0) goto lab_5f9e;               /* and al,0x10 / jnz */
    }
    return;
lab_5fa7:
    ax = keyboard_counter;                                 /* mov ax,[keyboard_counter] */
lab_5faa:
    if (ui_wait_hook) ui_wait_hook();
    if (ax == keyboard_counter) goto lab_5faa;             /* cmp ax,[keyboard_counter] / jz */
}

/* display_text_line (L405-413): dx = word [title_joy_offset + attract_icon_sprite_b] (set_cursor usa dh = fila),
 * si = word [bx + attract_icon_sprite_a]; title_joy_offset += 2 entre medias; print_string. */
void display_text_line(void) {
    uint16_t bx = title_joy_offset;
    uint16_t dx = ui_ds_word((uint16_t)(bx + DS_TEXT_CURS));
    set_cursor((uint8_t)(dx >> 8));                        /* dl se pone a 0 dentro de set_cursor */
    bx = title_joy_offset;
    title_joy_offset = (uint16_t)(title_joy_offset + 0x2);
    print_string(&ds_pool[ui_ds_word((uint16_t)(bx + DS_TEXT_PTRS))]);
}

/* clear_cga (L418-429): rep stosw 0xfa0 words (8000 bytes) en 0xb800:0 y en 0xb800:0x2000. */
void clear_cga(void) {
    memset(&cga_mem[0], 0, 0xfa0 * 2);
    memset(&cga_mem[0x2000], 0, 0xfa0 * 2);
}

/* ================= T52/T53: pantalla de titulo ================= */
static uint16_t attract_anim_idx;      /* DS 0x6a8d */
static uint8_t  attract_key_pressed;   /* DS 0x6a8a */
static uint16_t attract_save_int;      /* DS 0x6150 */
static uint16_t attract_last_tick;     /* DS 0x6a8b */
static uint16_t attract_frame_tick;    /* DS 0x6a93 */
static uint16_t attract_start_tick;    /* DS 0x6a88 */

#define DS_ATTRACT_ICON_PTRS 0x6a8f    /* 2 words: frames del icono */
#define DS_ATTRACT_TIMING    0x56da    /* word: ticks hasta el modo demo (0x34e) */
#define CGA_TITLE_ICON       0x1d38     /* dat_1d38 */

/* animate_title_icon (L221-231): attract_anim_idx += 2; frame = word [(idx & 2) + attract_icon_ptrs]; blit 10 words x
 * 12 filas a dat_1d38. */
void animate_title_icon(void) {
    attract_anim_idx = (uint16_t)(attract_anim_idx + 0x2);
    uint16_t bx = (uint16_t)(attract_anim_idx & 0x2);
    uint16_t si = ui_ds_word((uint16_t)(bx + DS_ATTRACT_ICON_PTRS));
    blit_to_cga(&ds_pool[si], CGA_TITLE_ICON, 0x0a, 0x0c);   /* mov cx,0xc0a */
}

/* move_title_cat (L164-197). check_vsync se toma como "en retrace" (ZF=0): el `jz lab_5e2a` no salta y la animacion
 * avanza en cada llamada; el ritmo lo marca el bucle del titulo (un paso por vuelta de ui_wait_hook). */
void move_title_cat(void) {
    uint16_t dx, ax;
    if ((uint16_t)cat_x > 0x20) goto lab_5de2;             /* cmp word [cat_x],0x20 / ja */
    input_horizontal = 0x1;
    goto lab_5e1c;
lab_5de2:
    if ((uint16_t)cat_x < 0x120) goto lab_5df1;            /* cmp word [cat_x],0x120 / jc */
    input_horizontal = (int8_t)0xff;
    goto lab_5e1c;
lab_5df1:
    dx = score_tick();                                     /* sub ah,ah / int 0x1a */
    ax = (uint16_t)(dx - attract_start_tick);
    if (ax < 0x12) goto lab_5e1c;                          /* cmp ax,0x12 / jc */
    attract_start_tick = dx;
    dx = cga_random();                                     /* call random (dx) */
    input_horizontal = 0x0;
    if ((uint8_t)dx > 0xa0) goto lab_5e1c;                 /* cmp dl,0xa0 / ja */
    {
        uint8_t dl = (uint8_t)(dx & 0x1);
        if (dl == 0) dl = 0xff;                            /* jnz / mov dl,0xff */
        input_horizontal = (int8_t)dl;
    }
lab_5e1c:
    scroll_speed = 0x4;                                    /* check_vsync != 0 */
    update_animation();                               /* update_animation (T74) */
}

/* show_title_screen (L55-156). Bloqueante como el original; ui_wait_hook presenta/bombea en cada vuelta. */
void show_title_screen(void) {
    uint16_t dx, ax;
    level_number = 0x0;
    init_player();
    init_cycle_objects();                                  /* init_objects */
    draw_alley_scene();
    blit_to_cga(&ds_pool[0x6152], 0x00bd, 0x0b, 0x1d);     /* mov cx,0x1d0b: alto 0x1d, ancho 0x0b words */
    blit_to_cga(&ds_pool[0x63d0], 0x069e, 0x0e, 0x16);
    blit_to_cga(&ds_pool[0x6638], 0x0a78, 0x03, 0x0c);
    blit_to_cga(&ds_pool[0x6680], 0x0ca8, 0x0e, 0x08);
    blit_to_cga(&ds_pool[0x6760], 0x1d6e, 0x0c, 0x0b);     /* dat_1d6e */
    blit_to_cga(&ds_pool[0x6868], 0x1dec, 0x04, 0x08);     /* dat_1dec */
    attract_anim_idx = 0x0;
    animate_title_icon();
    cat_x = 0x0;
    setup_alley();
    cat_y = 0x60;
    cat_y_bottom = 0x92;
    draw_high_score_display();                             /* draw_score (nombres cruzados, §5v) */
    draw_current_score();                                  /* draw_high_score */
    lives_count = 0x9;
    lives_display = 0xff;
    draw_lives();
    init_sound();
    input_horizontal = 0x0;
    input_vertical = 0x0;
    attract_key_pressed = 0x0;
    attract_save_int = keyboard_counter;
lab_5d54:
    dx = score_tick();                                     /* sub ah,ah / int 0x1a */
    attract_last_tick = dx;
    title_music_restart(dx);                               /* title_music_tick = dx / title_music_pos = 0 */
    attract_frame_tick = dx;
    attract_start_tick = (uint16_t)(dx - 0x30);
lab_5d71:
    if (ui_wait_hook) ui_wait_hook();                      /* port: presentar y bombear SDL (el original gira sin pausa) */
    dx = score_tick();
    ax = (uint16_t)(dx - attract_frame_tick);
    if (ax < 0x24) goto lab_5d89;                          /* cmp ax,0x24 / jc */
    attract_frame_tick = dx;
    animate_title_icon();
lab_5d89:
    dx = (uint16_t)(dx - attract_last_tick);
    ax = ui_ds_word(DS_ATTRACT_TIMING);                    /* mov ax,[attract_timing] */
    if (attract_shown == 0x0) goto lab_5da0;               /* cmp byte [0x41a],0 / jz */
    ax = (uint16_t)(ax + 0x48);
    if (dx >= ax) goto lab_5d54;                           /* cmp dx,ax / jnc: reinicia el ciclo del titulo */
    goto lab_5da7;
lab_5da0:
    ax = (uint16_t)(ax + 0x6);
    if (dx > ax) goto lab_5dd3;                            /* ja: timeout -> modo demo */
lab_5da7:
    play_music_note();
    move_title_cat();
    if (use_joystick == 0x0) goto lab_5dca;
    {
        uint8_t al = joy_port_fn ? joy_port_fn() : 0xff;   /* mov dx,0x201 / in al,dx */
        if ((al & 0x10) == 0) goto lab_5dc3;               /* and al,0x10 / jz */
    }
    attract_key_pressed = 0x1;                             /* boton suelto: ya se puede aceptar una pulsacion */
    goto lab_5dca;
lab_5dc3:
    if (attract_key_pressed != 0x0) goto lab_5dd3;
lab_5dca:
    if (attract_save_int == keyboard_counter) goto lab_5d71;   /* sin tecla nueva: otra vuelta */
lab_5dd3:
    return;
}

/* --- T54: show_attract_mode (ui.asm L300-382) y la deteccion de joystick que usa (L431-L469). PROGRESS.md §6ax. --- */

/* test_joystick_axis (L452-469): `out 0x201` dispara el monoestable (no-op en el port); lee 0x201 hasta que los bits 0-1
 * (ejes X/Y del joystick A) bajen (CF=0 -> responde) o pasen 0x12 ticks (CF=1). Devuelve CF. */
int test_joystick_axis(void) {
    uint8_t al;
    uint16_t dx;
    title_input_tick = score_tick();                       /* sub ah,ah / int 0x1a / mov [title_input_tick],dx */
lab_601b:
    if (ui_wait_hook) ui_wait_hook();                      /* port: presentar y bombear SDL */
    al = joy_port_fn ? joy_port_fn() : 0xff;               /* mov dx,0x201 / in al,dx (sin joystick: bits a 1) */
    if ((al & 0x3) != 0) goto lab_6025;                    /* test al,3 / jnz */
    return 0;                                              /* clc / ret */
lab_6025:
    dx = (uint16_t)(score_tick() - title_input_tick);      /* int 0x1a / sub dx,[title_input_tick] */
    if (dx < 0x12) goto lab_601b;                          /* cmp dx,0x12 / jc */
    return 1;                                              /* stc / ret */
}

/* detect_joystick (L436-450). int 0x11 -> bios_equipment (bit 12 = adaptador de juegos). Sin adaptador, o si ninguno de
 * los 2 intentos de test_joystick_axis responde, muestra el aviso (4 lineas desde title_joy_offset=0x24), espera una
 * tecla y devuelve CF=1. Con el equipo por defecto del port (0x0020, sin bit 12) siempre falla: igual que un PC sin
 * game port. Devuelve CF (0 = joystick detectado). */
int detect_joystick(void) {
    uint16_t ax;
    int cx;
    if ((bios_equipment & 0x1000) == 0) goto lab_5ff6;     /* int 0x11 / test ax,0x1000 / jz */
    if (test_joystick_axis() == 0) goto lab_600e;          /* call / jnc */
    if (test_joystick_axis() == 0) goto lab_600e;
lab_5ff6:
    title_joy_offset = 0x24;
    for (cx = 0x4; cx != 0; cx--) display_text_line();     /* mov cx,4 / call / loop */
    ax = keyboard_counter;
lab_6007:
    if (ui_wait_hook) ui_wait_hook();
    if (ax == keyboard_counter) goto lab_6007;             /* cmp ax,[keyboard_counter] / jz */
    return 1;                                              /* stc */
lab_600e:
    return 0;                                              /* CF=0 */
}

/* show_attract_mode (L300-382). Pantalla de seleccion previa a la partida: (1) pregunta Y/N de joystick (matriz 0x6c1 =
 * Y, 0x6c2 = N; Y sin joystick vuelve a empezar), (2) dificultad con K/H/T/A (0x6c3..0x6c6 -> difficulty_counter 0..3),
 * (3) instrucciones del teclado o del joystick y espera final. Bit 7 de la matriz a 0 = tecla pulsada (ver hardware.h).
 * Cada espera de tecla toma keyboard_counter y gira hasta que cambia; si la tecla nueva no es de las esperadas, vuelve
 * a esperar. Las vueltas llaman a ui_wait_hook (el original gira sin pausa). */
void show_attract_mode(void) {
    uint16_t ax;
    int cx;
    silence_speaker();
lab_5ee8:
    clear_cga();
    title_joy_offset = 0x0;
    display_text_line();
lab_5ef4:
    ax = keyboard_counter;                                 /* mov ax,[keyboard_counter] */
lab_5ef7:
    if (ui_wait_hook) ui_wait_hook();
    if (ax == keyboard_counter) goto lab_5ef7;             /* cmp ax,[keyboard_counter] / jz */
    if ((key_matrix[KEY_IDX_JOY_YES] & 0x80) == 0) goto lab_5f12;   /* test [0x6c1],0x80 / jz: Y pulsada */
    if ((key_matrix[KEY_IDX_JOY_NO] & 0x80) != 0) goto lab_5ef4;    /* test [0x6c2],0x80 / jnz: N no pulsada */
    use_joystick = 0x0;
    goto lab_5f1c;
lab_5f12:
    if (detect_joystick() != 0) goto lab_5ee8;             /* call / jc: sin joystick, se reinicia la pantalla */
    use_joystick = 0x1;
lab_5f1c:
    for (cx = 0x5; cx != 0; cx--) display_text_line();     /* mov cx,5 / push / call / pop / loop */
lab_5f26:
    ax = keyboard_counter;
lab_5f29:
    if (ui_wait_hook) ui_wait_hook();
    if (ax == keyboard_counter) goto lab_5f29;
    ax = 0x0;                                              /* db 0x2b,0xc0 = sub ax,ax */
    if ((key_matrix[KEY_IDX_DIFF0] & 0x80) == 0) goto lab_5f50;     /* K */
    ax++;
    if ((key_matrix[KEY_IDX_DIFF1] & 0x80) == 0) goto lab_5f50;     /* H */
    ax++;
    if ((key_matrix[KEY_IDX_DIFF2] & 0x80) == 0) goto lab_5f50;     /* T */
    ax++;
    if ((key_matrix[KEY_IDX_DIFF3] & 0x80) != 0) goto lab_5f26;     /* A: si no esta pulsada, otra espera */
lab_5f50:
    diff_icon_idx = ax;                                    /* mov [difficulty_counter],ax */
    for (cx = 0x5; cx != 0; cx--) display_text_line();
    if (use_joystick == 0x0) goto lab_5f7e;
    title_joy_offset = 0x20;
    display_text_line();
    display_text_line();
    title_joy_offset = 0x18;
    display_text_line();
    display_text_line();
    goto lab_5f93;
lab_5f7e:
    title_joy_offset = 0x1c;
    display_text_line();
    display_text_line();
    title_joy_offset = 0x16;
    display_text_line();
lab_5f93:
    wait_for_input();
}

/* --- T55: show_pause_menu (ui.asm L245-299). PROGRESS.md §6ay. --- */
#define DS_STR_PAWS_GAME       0x6d91
#define DS_STR_KEY_CONTINUE    0x6db2
#define DS_STR_BUTTON_CONTINUE 0x6dd3

/* show_pause_menu: silencia el altavoz, guarda el tick, guarda 0x20 words x 0x10 filas de CGA desde 0xdca (el original
 * las deja en DS:0xe; el port usa un buffer propio, ver PROGRESS.md), escribe "Paws Game" (fila 0xb, col 5) y la
 * indicacion (fila 0xc, col 5; "button" con joystick), espera una tecla/boton, restaura la region y el tick, y fija
 * pause_counter = keyboard_counter (process_keyboard no vuelve a pausar con esa misma pulsacion). */
void show_pause_menu(void) {
    static uint8_t pause_save[0x20 * 2 * 0x10];            /* DS:0xe..0x40e en el original */
    const uint8_t *si;
    silence_speaker();
    title_saved_cx = score_tick();                         /* sub ah,ah / int 0x1a / mov [title_saved_cx],dx */
    title_saved_dx = 0x0;                                  /* mov [title_saved_dx],cx: palabra alta, no modelada */
    save_from_cga(pause_save, 0xdca, 0x20, 0x10);          /* si=0xdca, di=0xe, cx=0x1020 */
    bios_set_cursor(0xb, 0x5);                             /* mov dx,0xb05 / int 0x10 ah=2 */
    si = &ds_pool[DS_STR_PAWS_GAME];
    print_string(si);
    bios_set_cursor(0xc, 0x5);                             /* mov dx,0xc05 */
    si = &ds_pool[DS_STR_KEY_CONTINUE];
    if (use_joystick == 0x0) goto lab_5eba;
    si = &ds_pool[DS_STR_BUTTON_CONTINUE];
lab_5eba:
    print_string(si);
    wait_for_input();
    blit_to_cga(pause_save, 0xdca, 0x20, 0x10);            /* si=0xe, di=0xdca */
    set_bios_tick(title_saved_cx);                         /* mov ah,1 / int 0x1a con cx=saved_dx, dx=saved_cx */
    pause_counter = keyboard_counter;
}

/* --- T58: love_scene_outro (ui.asm L489-570). PROGRESS.md §6bb. --- */
#define DS_DAT_6E10  0x6e10        /* 3 words x 12 filas (0x48 bytes): sprite que baja */
#define DS_DAT_6E58  0x6e58        /* 6 words x 17 filas (0xcc bytes): imagen final (termina justo en title_scroll_pos) */

/* love_scene_outro: epilogo del nivel 7. Fase 1: un paso por tick BIOS; borra el sprite anterior (3x12 words a cero;
 * el original pone a cero DS:0xe..0x56 con rep stosw y lo usa de fuente, el port usa un buffer propio), baja
 * title_scroll_pos 0x1e0 (6 filas de banco = 12 lineas) y dibuja dat_6e10; con sonido, un tono PIT canal 2 de
 * divisor pos>>1 (`db 0xd1,0xe8` = shr ax,1, no 0). Se repite mientras pos < 0x1a40. Fase 2: dibuja dat_6e58 (6x17) en
 * pos y alterna dos tonos (0xc00 con tick par, 0xb54 con impar) hasta que pasen 0x12 ticks desde el ultimo tick de la
 * fase 1; luego silence_speaker. check_vsync = en retrace (el `jz lab_6058` no salta). Bloqueante como el original
 * (cutscene de una sola vez); ui_wait_hook presenta/bombea SDL en cada vuelta de espera de tick. */
void love_scene_outro(void) {
    static const uint8_t blank[0x24 * 2];                  /* DS:0xe, 0x24 words a cero (sub ax,ax / rep stosw) */
    uint16_t dx, ax;
    title_scroll_pos = 0x25;                               /* mov word [title_scroll_pos],0x25 */
lab_6058:
    /* call check_vsync / jz lab_6058: en retrace, no salta */
    blit_to_cga(blank, title_scroll_pos, 0x3, 0xc);        /* si=0xe, di=pos, cx=0xc03: borra el sprite anterior */
    title_scroll_pos = (uint16_t)(title_scroll_pos + 0x1e0);
    blit_to_cga(&ds_pool[DS_DAT_6E10], title_scroll_pos, 0x3, 0xc);
lab_607d:
    if (ui_wait_hook) ui_wait_hook();                      /* port: presentar y bombear SDL */
    dx = score_tick();                                     /* sub ah,ah / int 0x1a */
    if (dx == title_scroll_tick_1) goto lab_607d;          /* cmp dx,[title_scroll_tick_1] / jz */
    title_scroll_tick_1 = dx;
    if (sound_enabled == 0x0) goto lab_60a7;               /* cmp byte [0],0 / jz */
    pit_out_43(0xb6);                                      /* mov al,0xb6 / out 0x43,al */
    ax = (uint16_t)(title_scroll_pos >> 1);                /* shr ax,1 */
    pit_ch2_out((uint8_t)(ax & 0xff));                     /* out 0x42,al */
    pit_ch2_out((uint8_t)(ax >> 8));                       /* mov al,ah / out 0x42,al */
    port61_out((uint8_t)(port61_in() | 0x3));              /* in al,0x61 / or al,3 / out 0x61,al */
lab_60a7:
    if (title_scroll_pos < 0x1a40) goto lab_6058;          /* cmp / jb (sin signo) */
    blit_to_cga(&ds_pool[DS_DAT_6E58], title_scroll_pos, 0x6, 0x11);   /* cx=0x1106 */
lab_60bc:
    if (ui_wait_hook) ui_wait_hook();
    dx = score_tick();
    if (dx == title_scroll_tick_2) goto lab_60bc;          /* cmp dx,[title_scroll_tick_2] / jz */
    title_scroll_tick_2 = dx;
    if (sound_enabled == 0x0) goto lab_60e6;
    pit_out_43(0xb6);
    ax = 0xc00;
    if ((dx & 0x1) == 0) goto lab_60e0;                    /* test dl,1 / jz (el mov previo no toca flags) */
    ax = 0xb54;
lab_60e0:
    pit_ch2_out((uint8_t)(ax & 0xff));
    pit_ch2_out((uint8_t)(ax >> 8));
lab_60e6:
    dx = (uint16_t)(dx - title_scroll_tick_1);             /* sub dx,[title_scroll_tick_1] */
    if (dx < 0x12) goto lab_60bc;                          /* cmp dx,0x12 / jb (sin signo) */
    silence_speaker();
}
