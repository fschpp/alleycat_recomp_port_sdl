/* animation_entry.c — literal port de game_loop.asm L158-285 (T70), ver PROGRESS.md §6be.
 *
 * Mapa lab_XXXX -> C (toda la entrada de update_animation):
 *   update_animation..lab_08fc  compuerta tick/pcjr_delay        -> aqui
 *   lab_08fd/lab_090c/lab_091d  retardo, vsync, reloj            -> aqui
 *   lab_0926..lab_0949          bloqueos nivel 4 / nivel 6       -> aqui
 *   lab_0953..lab_09d6          nivel 2: fase, muerte, maullido  -> aqui
 *   lab_09d6..lab_09f6          nivel 2: color del borde         -> aqui
 *   lab_09f6..lab_0a1a          nivel 2: prev_*, speed_ramp      -> update_cat_movement() (movement.c)
 */
#include "bios_clock.h"
#include "animation_entry.h"
#include "cat_state.h"
#include "cga.h"
#include "level2.h"
#include "level6.h"
#include "movement.h"
#include "palette.h"
#include "animation.h"
#include "sound.h"
#include "gen/ds_pool.h"

#include <stdint.h>
#include <time.h>

int32_t ua_tick_override = -1;

/* DS 0x057f anim_tick_delay: lab_0926 lo escribe; en el port nadie mas lo lee (alley.c usa su propio static
 * para la misma palabra como temporal de la animacion de muerte). Se exporta para los tests. */
uint16_t ua_anim_tick_delay = 0;

/* DS 0x0589 level2_phase1_ticks, 0x0599 level2_death_ticks, 0x05a9/0x05b9/0x05c9 level2_color{1,2,3}_ticks
 * (verificados con tools/asm_label.sh): tablas de 8 palabras, indice = difficulty_level*2. */
#define DS_L2_PHASE1  0x0589
#define DS_L2_DEATH   0x0599
#define DS_L2_COLOR1  0x05a9
#define DS_L2_COLOR2  0x05b9
#define DS_L2_COLOR3  0x05c9
/* `mov si,0x64e` es una constante inmediata: dentro de alley_save_buf (0x05fa..0x066c), 3 palabras x 5 filas
 * (cx=0x503). Va como inmediato, no como etiqueta (regla 3 de tareas.md). */
#define DS_L2_MEOW_SPRITE 0x064e

static uint16_t ds_word(unsigned off) { return (uint16_t)(ds_pool[off] | (ds_pool[off + 1] << 8)); }

static uint16_t ua_read_tick(void) {                 /* sub ah,ah / int 0x1a -> dx */
    if (ua_tick_override >= 0) return (uint16_t)ua_tick_override;
    return bios_clock_read();
}

ua_next_t update_animation_entry(void) {
    uint16_t dx = ua_read_tick();
    uint16_t ax;
    unsigned si;

    if (dx != anim_last_tick) goto lab_08fd;
    if (pcjr_delay == 0) goto lab_08fc;
    pcjr_delay--;
    if (pcjr_delay == 0) { ax = 0; goto lab_090c; }   /* AX = AH:AL del int 0x1a (AL = flag de medianoche, 0) */
lab_08fc:
    return UA_RET;

lab_08fd:
    ax = 0x20;
    if (rom_id == 0xfd) ax >>= 1;
    pcjr_delay = ax;
lab_090c:
    if (level_number == 0x2) goto lab_091d;
    if ((uint8_t)((uint8_t)in_level_mode | (uint8_t)scroll_direction) != 0) goto lab_0926;
lab_091d:
    /* push dx/ax; call check_vsync; pop ax/dx; jz lab_08fc. check_vsync devuelve ZF=1 = FUERA de retrace
     * (hardware.asm L38), y aqui jz = volver. Sin CGA real se trata como "en retrace" (ZF=0): seguir,
     * si no el nivel 2 y el estado quieto nunca avanzarian (mismo criterio que level6.h/level2.h). */
lab_0926:
    anim_last_tick = dx;
    ua_anim_tick_delay = ax;                             /* mov [anim_tick_delay],ax */
    if (level_number != 0x4) goto lab_093b;
    if (l3_door_anim_frame != 0) goto lab_08fc;
lab_093b:
    if (level_number != 0x6) goto lab_0949;
    if (l6_dat_44bd != 0) goto lab_08fc;
lab_0949:
    if (level_number == 0x2) goto lab_0953;
    return UA_L0BAC;                                    /* jmp near lab_0bac */

lab_0953:
    si = (unsigned)(difficulty_level << 1) & 0xffff;
    ax = (uint16_t)(anim_last_tick - level2_tick);
    if (ax < ds_word(DS_L2_PHASE1 + si)) goto lab_09d6;   /* jc */
    if (ax < ds_word(DS_L2_DEATH + si)) goto lab_0971;    /* jc */
    object_hit = 0x1;
lab_0971:
    meow_timer--;
    if (meow_timer != 0) goto lab_09b9;
    play_meow_sound();
    meow_timer = 0x6;
    {
        uint8_t al = level2_rise;
        if (cat_y < 0xb3) goto lab_0992;                /* cmp [cat_y],0xb3 / jc */
        if (al >= 0xc8) goto lab_0992;                  /* cmp al,0xc8 / jnc */
        al = (uint8_t)(al + 0x1e);
        level2_rise = al;
lab_0992: {
        uint8_t dl = cat_y;
        dl = (dl >= level2_rise) ? (uint8_t)(dl - level2_rise) : 0;   /* sub dl,al / jnc / sub dl,dl */
        dl &= 0xf8;
        size_t di = calc_cga_addr(dl, (uint16_t)cat_x, NULL);
        static uint16_t meow_save[16];                  /* bp=0xe: area de guardado propia (como show_pause_menu) */
        blit_masked(&ds_pool[DS_L2_MEOW_SPRITE], di, 3, 5, meow_save);
        }
    }
lab_09b9:
    scroll_direction = 0x0;
    in_level_mode = 0x1;
    immune_flag = 0x1;
    anim_counter = 0x20;
    l2_border_color = 0x0;                              /* sub bx,bx / mov ah,0xb / int 0x10 */
    bios_color_select(0x0, 0x0);                        /* el gato usa el indice 0: el color de fondo SI se ve */
    return UA_L0A86;

lab_09d6: {
    uint8_t bl = 0;
    si = (unsigned)(difficulty_level << 1) & 0xffff;
    if (ax < ds_word(DS_L2_COLOR1 + si)) goto lab_09f6;
    bl++;
    if (ax < ds_word(DS_L2_COLOR2 + si)) goto lab_09f6;
    bl = 0x5;
    if (ax < ds_word(DS_L2_COLOR3 + si)) goto lab_09f6;
    bl--;
lab_09f6:
    l2_border_color = bl;                               /* mov ah,0xb / int 0x10 */
    bios_color_select(0x0, bl);                         /* aplicar a la paleta (T78b): el gato cambia de color con el aire */
    }
    return UA_L09F6;
}

void update_animation_l2_path(void) {
    switch (update_animation_entry()) {
    case UA_RET:    return;
    case UA_L0BAC:  return;                     /* T72 */
    case UA_L09F6:  update_cat_movement(); break;   /* lab_09f6 .. lab_0a86 (incluye update_cat_dive) */
    case UA_L0A86:  update_cat_dive(); break;       /* lab_0a86 */
    }
    update_cat_frame();                         /* lab_0ace .. lab_0bab */
}
