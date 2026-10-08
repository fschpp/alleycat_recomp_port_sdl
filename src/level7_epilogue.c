#include "bios_clock.h"
#include "cat_state.h"
#include "alley.h"
#include "sound.h"
#include "palette.h"
#include "speaker.h"
#include "cga.h"
#include "level7_epilogue.h"
#include "game_flow.h"
#include "level_collision.h"
#include "level3_enemy.h"
#include "input.h"
#include "cupid.h"
#include "gen/ds_pool.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* BIOS `int 0x1a` tick substitute — same convention used throughout this
 * port. */
uint16_t (*l7_tick_fn)(void) = NULL;   /* gancho para tests (como l2_tick_fn); NULL = reloj real */
static uint16_t read_bios_tick(void) {
    if (l7_tick_fn) return l7_tick_fn();
    return bios_clock_read();
}

/* Sleeps a short real-world slice. Used only by the victory-cutscene
 * functions below (position_victory_cat/init_victory_wave) to pace their
 * *deliberately real, blocking* busy-wait loops — see the long comment
 * above run_victory_sequence for why blocking is the right call here,
 * unlike everywhere else in this port. */
/* T84: la cinematica bloquea (como el original) y dibuja directo en cga_mem; sin presentar el video durante las
 * esperas, la animacion de los corazones nunca llegaba a verse (solo el barrido final). main.c apunta
 * l7_step_hook a "bombear eventos SDL + video_present" (como wipe_step_hook, T45); NULL = nada (tests). Se llama
 * como mucho cada ~15 ms de espera acumulada. */
void (*l7_step_hook)(void) = NULL;
static int l7_slept_ms = 0;

static void l7_present(void) {
    l7_slept_ms = 0;
    if (l7_step_hook) l7_step_hook();
}

static void l7_sleep_ms(int ms) {
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
    l7_slept_ms += ms;
    if (l7_slept_ms >= 15) l7_present();
}

static uint16_t ds_word(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

/* --- ds_pool offsets. Confirmed with tools/resolve_data_segment.py
 * (real linear walk of cat.asm's .data section), NOT assumed from label
 * names — see include/level7_epilogue.h and PROGRESS.md. ---
 *   dat_4500 (0x4500, 72 bytes = 3 words x 12 rows): the "idle" heart-cat
 *   sprite frame, used when l7_cat_dir==0 (not currently facing/walking).
 *   dat_4a60 (0x4a60, 32 bytes = 16 words): a pointer table of the
 *   walking-animation frames — indices 0,2,4,6,8,0xa (six walk-cycle
 *   frames) plus 0xc,0xe (two "paused, still facing" frames) for the
 *   right-facing cat, then the same eight again at +0x10 for the
 *   left-facing (mirrored) cat.
 *   window_row_y_table (0x2bd4, already known from the unported window
 *   subsystem): read here purely as static per-window row data to seed
 *   each heart-cat's starting y, no live window state needed. */
#define L7_IDLE_SPRITE   0x4500
#define L7_SPRITE_PTRS   0x4a60
#define WINDOW_ROW_Y_TABLE 0x2bd4
/* l7_heart_y_table (0x45bf, 8 bytes): per-difficulty byte threshold used
 * to gate how often a heart-cat re-picks its walking direction.
 * l7_heart_sprite_table (0x45c7): only its first 16 bytes (8 words, one
 * per difficulty) are used *in this chunk*, as a per-difficulty pacing
 * interval — the disassembler's name suggests real sprite bitmaps live
 * further into this same block too, likely consumed by check_l7_cupid
 * (not yet ported, see below). */
#define L7_HEART_Y_TABLE      0x45bf
#define L7_HEART_SPRITE_TABLE 0x45c7

/* --- "current" heart-cat scratch state (dat_4548.. in the original —
 * see the header comment; swapped in/out of the 7 slots below by
 * save/load_l7_slot). --- */
static int16_t  l7_cat_x;
static uint8_t  l7_cat_dir;      /* 0=idle, 1=right, 0xff=left */
static uint8_t  l7_cat_y;
static uint16_t l7_cat_save_addr;
static uint8_t  l7_cat_moving;
static uint16_t l7_cat_last_tick;
static uint16_t l7_cat_anim_idx;
static uint8_t  l7_cat_delay;

typedef struct {
    int16_t  x;
    uint8_t  dir;
    uint8_t  y;
    uint16_t save_addr;
    uint8_t  moving;
    uint16_t last_tick;
    uint16_t anim_idx;
    uint8_t  delay;
} l7_cat_slot_t;

static l7_cat_slot_t l7_slots[7];
static uint16_t l7_heart_index;
static uint16_t l7_heart_cga_addr;
static uint16_t l7_heart_tick;    /* 0x45b8 — pacing reference tick */
static uint16_t l7_heart_speed;   /* 0x45bc — current per-tick move step */
static uint8_t  l7_heart_caught;  /* 0x45be */

/* A 3-word x 12-row buffer of the CGA "0x5555" dither/fill pattern —
 * clear_l7_sprite fills with this constant rather than restoring a saved
 * background, matching the original exactly (the epilogue's backdrop is
 * a flat area, so a fixed fill is a correct erase there). */
static const uint16_t l7_clear_pattern[3 * 12] = {
    0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555,
    0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555,
    0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555,
    0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555,
};

static void save_l7_slot(uint16_t idx) {
    l7_cat_slot_t *s = &l7_slots[idx];
    s->x = l7_cat_x;
    s->dir = l7_cat_dir;
    s->y = l7_cat_y;
    s->save_addr = l7_cat_save_addr;
    s->moving = l7_cat_moving;
    s->last_tick = l7_cat_last_tick;
    s->anim_idx = l7_cat_anim_idx;
    s->delay = l7_cat_delay;
}

static void load_l7_slot(uint16_t idx) {
    l7_cat_slot_t *s = &l7_slots[idx];
    l7_cat_x = s->x;
    l7_cat_dir = s->dir;
    l7_cat_y = s->y;
    l7_cat_save_addr = s->save_addr;
    l7_cat_moving = s->moving;
    l7_cat_last_tick = s->last_tick;
    l7_cat_anim_idx = s->anim_idx;
    l7_cat_delay = s->delay;
}

static void draw_l7_sprite(const uint8_t *src, uint16_t dst) {
    blit_or(src, dst, 3, 12, NULL);
}

static void clear_l7_sprite(uint16_t dst) {
    blit_to_cga((const uint8_t *)l7_clear_pattern, dst, 3, 12);
}

static void setup_l7_sprite(void) {
    l7_cat_moving = 0;
    const uint8_t *src = &ds_pool[L7_IDLE_SPRITE];

    if (l7_cat_dir != 0) {
        uint16_t bx = l7_cat_anim_idx;
        if (l7_cat_delay != 0) {
            /* pausa (tras un golpe): frames 0xc/0xe. Caminando (delay == 0) usa los frames 0..5. */
            bx = (uint16_t)((bx & 2) + 0xc);
        }
        if (l7_cat_dir == 0xff) bx = (uint16_t)(bx + 0x10);
        uint16_t ofs = (uint16_t)(ds_pool[L7_SPRITE_PTRS + bx] |
                                   (ds_pool[L7_SPRITE_PTRS + bx + 1] << 8));
        src = &ds_pool[ofs];
    }

    uint16_t di = l7_heart_cga_addr;
    l7_cat_save_addr = di;
    draw_l7_sprite(src, di);
}

static void erase_l7_sprite(void) {
    if (l7_cat_moving == 0) {
        clear_l7_sprite(l7_cat_save_addr);
    }
}

void init_level7_objects(void) {
    l7_heart_index = 0;
    do {
        uint16_t dx = (uint16_t)(cga_random() & 0x7f);
        dx = (uint16_t)(dx + 0x60);
        l7_cat_x = (int16_t)dx;
        l7_cat_dir = 0;
        l7_cat_moving = 1;
        l7_cat_anim_idx = 0;
        l7_cat_delay = 0;

        uint16_t last_tick = 0;
        if (l7_heart_index == 0) {
            uint16_t tick = read_bios_tick();
            last_tick = (tick != 0) ? tick : (uint16_t)-1;
        }
        l7_cat_last_tick = last_tick;

        uint8_t y = (uint8_t)(ds_pool[WINDOW_ROW_Y_TABLE + l7_heart_index] + 3);
        l7_cat_y = y;

        save_l7_slot(l7_heart_index);
        l7_heart_index++;
    } while (l7_heart_index < 7);
    l7_heart_index = 0;
}

/* --- Cupid subsystem (ui.asm's update_cupid/draw_cupid/erase_cupid) —
 * a separate, still entirely-unported flying-arrow enemy for level 7.
 * cupid_active defaults to 0 (false), so check_l7_cupid below correctly
 * and faithfully always reports "no hit" until that subsystem exists —
 * no special-casing needed, the literal port just degrades gracefully.
 * draw_cupid/erase_cupid stubbed the same way, for the same reason. */
/* T56: cupid_active/cupid_x/cupid_y son ahora los de cupid.h (src/cupid.c). */
/* T57: erase_cupid/draw_cupid son ahora los reales de cupid.c. */

/* l7_obj_x/y/active (0x2b5a/0x2b6a/0x2b72, 8 entries each) — the level-7
 * "heart falls from a window" objects. Populated by spawn_thrown_object/
 * tick_level_thrown_objects (top of level_objects.asm, not yet ported —
 * that's the level-7-specific counterpart to the general levels-0-6
 * thrown-object rain in level_objects.c/§5z). Until that's ported these
 * arrays stay all-zero/inactive, so check_l7_object_overlap below is a
 * real, complete port that simply never finds anything active yet. */
int16_t l7_obj_x[8];                  /* 0x2b5a (publicos: los lee tests/test_level7.c) */
uint8_t l7_obj_y[8];                  /* 0x2b6a */
uint8_t l7_obj_active[8];             /* 0x2b72 */
/* Estado de spawn_thrown_object (T37). Offsets DS resueltos con tools/resolve_data_segment.py (grep -w). El DS inicial los trae a 0. */
uint16_t l7_obj_spawn_slot = 0;       /* 0x2e8d (word) slot a spawnear; >= 8 = ninguno (lo deja en 0xffff al terminar) */
uint8_t  l7_obj_closest_dist = 0;     /* 0x2e91 */
uint16_t l7_obj_closest_row = 0;      /* 0x2e92 (word) */
uint16_t l7_obj_last_picked = 0;      /* 0x2e94 (word) */
uint16_t l7_obj_last_tick = 0;        /* 0x2e8f (word) ultimo tick BIOS procesado por tick_level_thrown_objects */
uint16_t l7_obj_cur_x = 0;            /* 0x2e96 (word) */
uint8_t  l7_obj_cur_y = 0;            /* 0x2e98 */
/* l7_obj_sprite (0x2af0..0x2b4a = 0x5a bytes = 3 palabras x 15 filas, verificado contra l7_obj_init_x_table=0x2b4a). */
#define L7_OBJ_SPRITE 0x2af0
/* l7_obj_erase_sprite (0x2b7a, 90 bytes = 3 words x 15 rows): the plain
 * background patch blitted over a caught heart-drop object to erase it. */
#define L7_OBJ_ERASE_SPRITE 0x2b7a

/* restore_alley_buffer / draw_alley_foreground: real implementations exist
 * (restore_alley_buffer) or are stubbed (draw_alley_foreground) elsewhere,
 * but aren't exposed via a shared header — alley_movement.c and
 * cycle_objects.c each already carry their own local
 * stub/near-duplicate for this exact reason (see cycle_objects.c's
 * cycle_restore_alley_buffer/cycle_start_tone). Following that same
 * established precedent here rather than introducing a new cross-file
 * dependency. */
static void l7_restore_alley_buffer(void) { restore_alley_buffer(); }
static void l7_draw_alley_foreground(void) { draw_alley_foreground(); }
/* start_tone is real now (src/sound.c). The original's second argument here
 * is `mov bx,l5_sprite_ptr` — the label's own DS OFFSET used as a frequency,
 * not a pointer dereference, same trick as play_meow_sound's
 * meow_sound_data. Resolved to 0x123b by tools/resolve_data_segment.py. */
#define L5_SPRITE_PTR_AS_FREQ 0x123b

/* check_l7_cupid — literal port. */
static bool check_l7_cupid(void) {
    if (!cupid_active) return false;
    return check_rect_collision((int16_t)cupid_x, cupid_y, 0x10, 0x0c,
                                 l7_cat_x, l7_cat_y, 0x18, 0x08);
}

/* check_l7_object_overlap — literal port. At most one falling heart can
 * be "caught" per call (the original falls straight through to its
 * shared tail on the first hit, never resuming the loop — mirrored here
 * with an early return rather than `break` + extra logic, to keep the
 * post-hit tail identical to the original's single linear path). */
static void check_l7_object_overlap(void) {
    for (int bx = 7; bx >= 0; bx--) {
        if (!l7_obj_active[bx]) continue;

        bool hit = check_rect_collision(l7_obj_x[bx], l7_obj_y[bx], 0x18, 0x0f,
                                         l7_cat_x, l7_cat_y, 0x18, 0x0c);
        if (!hit) continue;

        if (!l7_heart_caught) {
            l7_restore_alley_buffer();
            if (cupid_active) erase_cupid();
        }
        erase_l7_sprite();

        l7_obj_active[bx] = 0;
        uint16_t addr = (uint16_t)calc_cga_addr(l7_obj_y[bx], (uint16_t)l7_obj_x[bx], NULL);
        blit_to_cga(&ds_pool[L7_OBJ_ERASE_SPRITE], addr, 3, 15);

        if (!l7_heart_caught) {
            if (cupid_active) draw_cupid();
            l7_draw_alley_foreground();
        }

        uint16_t dx = 0;
        if (l7_heart_index != 6) {
            uint16_t tick = read_bios_tick();
            dx = (tick != 0) ? tick : (uint16_t)-1;
        }
        l7_cat_last_tick = dx;
        return;
    }
}

/* spawn_thrown_object (level_objects.asm L41-138, T37) — literal, etiqueta por etiqueta.
 * Notas de traduccion:
 *  - `db 0xd0,0xe3` (comentado como "shl bl,0x0" en el desensamblado) es D0 /4 = SHL BL,1: duplica el indice para direccionar
 *    las tablas de WORDS (l7_obj_x). Los accesos de BYTE (l7_obj_y/active) usan el indice ANTES de duplicar.
 *  - `not al` (no `neg`): para una fila por encima del gato la "distancia" guardada es ~(a-b) = |a-b|-1. Con las filas a 24 px la
 *    FILA elegida es la misma con not o neg; lo que cambia es l7_obj_closest_dist (estado DS) y eso lo verifica el test.
 *    Empates (`ja` solo salta si es estrictamente mayor): gana el ultimo evaluado = indice mas bajo (el bucle baja de 6 a 0).
 *  - `cmp word [0x2e92],0xffff` es codigo muerto (el bucle siempre deja una fila), pero se porta igual.
 *  - `mov ah,0xb / int 10h` (BX=0: color de fondo/borde = 0) no tiene efecto visible en el port (como l2_border_color).
 *  - check_rect_collision: A = (ax=x, dl=y, si=0x18, cl=0xf), B = (bx=x, dh=y, di=0x18, ch=0xf) con cx=0xf0f. */
void spawn_thrown_object(void) {
    uint16_t cx, bx, si, ax;
    uint8_t al, dl;

    if (l7_obj_spawn_slot < 0x8) goto lab_2e68;           /* cmp word [spawn_slot],8 / jb */
lab_2e67:
    return;
lab_2e68:
    if (joy_button != 0x0) goto lab_2e67;                 /* cmp byte [0x69a],0 */
    l7_obj_closest_row = 0xffff;
    l7_obj_closest_dist = 0xff;
    cx = 0x7;
lab_2e7d:
    bx = (uint16_t)(cx - 1);                              /* mov bx,cx / dec bx */
    al = cat_y;
    {
        uint8_t row_y = ds_pool[WINDOW_ROW_Y_TABLE + bx];
        bool borrow = al < row_y;                         /* sub al,[bx+window_row_y_table] -> CF */
        al = (uint8_t)(al - row_y);
        if (!borrow) goto lab_2e8b;                       /* jnb */
    }
    al = (uint8_t)~al;                                    /* not al */
lab_2e8b:
    if (al > l7_obj_closest_dist) goto lab_2e98;          /* ja (sin signo) */
    l7_obj_closest_dist = al;
    l7_obj_closest_row = bx;
lab_2e98:
    cx = (uint16_t)(cx - 1);                              /* loop */
    if (cx != 0) goto lab_2e7d;
    if (l7_obj_closest_row != 0xffff) goto lab_2ea8;      /* cmp word [0x2e92],0xffff / jnz */
    l7_obj_closest_row = 0x0;
lab_2ea8:
    bx = l7_obj_spawn_slot;
    si = l7_obj_closest_row;
    al = ds_pool[WINDOW_ROW_Y_TABLE + si];
    l7_obj_y[bx] = al;
    l7_obj_cur_y = al;
    ax = (uint16_t)cat_x;
    /* shl bl,1: bx pasa a indice de word; abajo se usa l7_obj_x[bx] (= [bx*2+l7_obj_x]) */
    if (ax < 0x108) goto lab_2ec8;                        /* jb */
    ax = 0x107;
lab_2ec8:
    ax &= 0xffc;
    l7_obj_x[bx] = (int16_t)ax;
    l7_obj_cur_x = ax;
    cx = 0x8;
lab_2ed5:
    bx = (uint16_t)(cx - 1);                              /* mov bx,cx / dec bx */
    if (bx == l7_obj_spawn_slot) goto lab_2f07;
    if (l7_obj_active[bx] == 0x0) goto lab_2f07;
    dl = l7_obj_y[bx];
    if (check_rect_collision(l7_obj_x[bx], dl, 0x18, 0xf, l7_obj_cur_x, l7_obj_cur_y, 0x18, 0xf)) {
        return;                                           /* pop cx / jnb no tomado -> ret (solapa otro objeto activo) */
    }
lab_2f07:
    cx = (uint16_t)(cx - 1);                              /* loop */
    if (cx != 0) goto lab_2ed5;
    l7_restore_alley_buffer();
    if (cupid_active == 0x0) goto lab_2f16;
    erase_cupid();
lab_2f16:
    bx = l7_obj_spawn_slot;
    l7_obj_last_picked = bx;
    l7_obj_active[bx] = 0x1;
    dl = l7_obj_y[bx];
    cx = (uint16_t)l7_obj_x[bx];                          /* shl bl,1 / mov cx,[bx+l7_obj_x] */
    ax = (uint16_t)calc_cga_addr(dl, cx, NULL);
    blit_to_cga(&ds_pool[L7_OBJ_SPRITE], ax, 3, 15);      /* di=ax, si=l7_obj_sprite, es=0xb800, cx=0xf03 */
    l7_obj_spawn_slot = 0xffff;
    /* sub bx,bx / mov ah,0xb / int 0x10: sin efecto visible */
    check_l7_all_objects();
    if (cupid_active == 0x0) goto lab_2f59;
    draw_cupid();
lab_2f59:
    l7_draw_alley_foreground();
    start_tone(0x3e8, 0x4a5);
}

/* tick_level_thrown_objects (level_objects.asm L139-208, T38) — literal. Pese al nombre NO mueve nada: una vez por tick BIOS mira si el
 * gato toca algun corazon activo (rect A = objeto 0x18x0xf, rect B = gato 0x18x0xe, cx=0xe0f); si si, lo "recoge": lo desactiva, lo
 * borra con l7_obj_erase_sprite y deja su slot en l7_obj_spawn_slot para que spawn_thrown_object lo reutilice.
 *  - `shl bl,0x0` del desensamblado = SHL BL,1 (ver spawn_thrown_object).
 *  - Con un spawn pendiente (spawn_slot < 8) no se escanea: solo se limpia l7_obj_last_picked.
 *  - Si el slot tocado es el ultimo recogido (l7_obj_last_picked) se sale sin tocar nada (ni siquiera last_picked).
 *  - `mov bx,1 / mov ah,0xb / int 0x10` (color de fondo/borde 1): sin efecto visible en el port. */
void tick_level_thrown_objects(void) {
    uint16_t cx, bx, tick;
    uint8_t dl;

    tick = read_bios_tick();                              /* sub ah,ah / int 0x1a -> dx */
    if (tick != l7_obj_last_tick) goto lab_2f71;
    return;
lab_2f71:
    l7_obj_last_tick = tick;
    if (l7_obj_spawn_slot < 0x8) goto lab_2fac;
    cx = 0x8;
lab_2f7f:
    bx = (uint16_t)(cx - 1);
    if (l7_obj_active[bx] == 0x0) goto lab_2faa;
    dl = l7_obj_y[bx];
    if (check_rect_collision(l7_obj_x[bx], dl, 0x18, 0xf, (uint16_t)cat_x, cat_y, 0x18, 0xe)) goto lab_2fb3;   /* jb */
lab_2faa:
    cx = (uint16_t)(cx - 1);                              /* loop */
    if (cx != 0) goto lab_2f7f;
lab_2fac:
    l7_obj_last_picked = 0xffff;
lab_2fb2:
    return;
lab_2fb3:
    bx = (uint16_t)(cx - 1);
    if (bx == l7_obj_last_picked) goto lab_2fb2;
    l7_restore_alley_buffer();
    if (cupid_active == 0x0) goto lab_2fca;
    erase_cupid();
lab_2fca:
    l7_obj_active[bx] = 0x0;
    dl = l7_obj_y[bx];
    l7_obj_spawn_slot = bx;
    blit_to_cga(&ds_pool[L7_OBJ_ERASE_SPRITE], calc_cga_addr(dl, (uint16_t)l7_obj_x[bx], NULL), 3, 15);   /* cx=0xf03 */
    if (cupid_active == 0x0) goto lab_2ffb;
    draw_cupid();
lab_2ffb:
    l7_draw_alley_foreground();
    start_tone(0x3e8, 0x349);
}

/* check_l7_all_objects — literal port. Sweeps every heart-cat slot that
 * hasn't already been redirected this frame (`l7_cat_last_tick==0`) and
 * checks it against the falling-heart objects above. */
void check_l7_all_objects(void) {
    l7_heart_caught = 1;
    uint16_t saved_index = l7_heart_index;
    l7_heart_index = 0;
    do {
        load_l7_slot(l7_heart_index);
        if (l7_cat_last_tick == 0) {
            check_l7_object_overlap();
            save_l7_slot(l7_heart_index);
        }
        l7_heart_index++;
    } while (l7_heart_index < 7);
    l7_heart_index = saved_index;
}

/* check_l7_cat_hit — literal port. */
static bool check_l7_cat_hit(void) {
    bool hit = check_rect_collision(cat_x, cat_y, 0x18, 0x0e,
                                     l7_cat_x, l7_cat_y, 0x18, 0x0c);
    if (!hit) return false;

    if (l7_heart_index == 6) {
        /* [0x553] — the same shared "enemy escaping" guard byte as
         * cat_caught (= [0x553], antes `enemy_escape_active`); the cupid slot (index 6)
         * getting caught marks the epilogue's finale as in progress. */
        cat_caught = 1;
        l7_restore_alley_buffer();
        erase_l7_sprite();
        return true;
    }

    l7_restore_alley_buffer();
    erase_l7_sprite();
    l7_draw_alley_foreground();
    transition_timer = 4;
    in_level_mode = 1;
    anim_counter = 4;
    anim_step = 8;
    l7_cat_delay = 4;
    l7_cat_dir = (l7_cat_x > cat_x) ? 1 : 0xff;
    start_tone(0xce4, L5_SPRITE_PTR_AS_FREQ);
    return true;
}

/* update_level7_objects — literal port of the round-robin per-tick
 * heart-cat update (lab_4c1b onward). Kept close to the original's
 * goto/label structure (rather than restructured into nested if/else)
 * deliberately: this function's branches interleave in a way that bit
 * a false structural rewrite once already this session (see
 * tick_thrown_objects in level_objects.c / PROGRESS.md §5z) — mirroring
 * the asm's control flow directly is the safer translation here. */
void update_level7_objects(void) {
    uint16_t tick = read_bios_tick();
    if (tick == l7_heart_tick) return;

    l7_heart_index++;
    if (l7_heart_index == 1 || l7_heart_index == 4) {
        l7_heart_tick = tick;
    } else if (l7_heart_index >= 7) {
        l7_heart_index = 0;
        l7_heart_tick = tick;
    }

    load_l7_slot(l7_heart_index);
    if (check_l7_cupid()) return;

    if (l7_cat_last_tick != 0) {
        uint16_t tick2 = read_bios_tick();
        uint16_t elapsed = (uint16_t)(tick2 - l7_cat_last_tick);
        uint16_t idx = (uint16_t)((difficulty_level & 7) * 2);
        uint16_t threshold = (uint16_t)(ds_pool[L7_HEART_SPRITE_TABLE + idx] |
                                         (ds_pool[L7_HEART_SPRITE_TABLE + idx + 1] << 8));
        if (l7_heart_index == 0) threshold = (uint16_t)(threshold << 1);
        if (elapsed < threshold) return;

        l7_cat_last_tick = 0;
        l7_cat_moving = 1;
        l7_cat_x = (cat_x > 0xa0) ? 0x24 : 0x108;
        l7_cat_dir = 0;
    }

    if (check_l7_cat_hit()) {
        save_l7_slot(l7_heart_index);
        return;
    }

    if (l7_cat_delay != 0) {
        l7_cat_delay--;
        if (l7_cat_delay != 0) goto movement;
        l7_cat_dir = (l7_cat_dir == 0xff) ? 1 : 0xff;
        goto movement;
    } else {
        if (l7_cat_y <= cat_y) {
            bool skip_random_gate = (l7_heart_index == 6 && cat_y < 0x28);
            if (!skip_random_gate) {
                uint8_t r = (uint8_t)(cga_random() & 0xFF);
                uint8_t threshold = ds_pool[L7_HEART_Y_TABLE + (difficulty_level & 7)];
                if (r > threshold) goto movement;
            }
            {
                uint16_t bird_grid = (uint16_t)l7_cat_x & 0xff8u;
                uint16_t cat_grid = (uint16_t)cat_x & 0xff8u;
                uint8_t dl;
                if (bird_grid == cat_grid) dl = 0;
                else dl = (bird_grid < cat_grid) ? 1 : 0xff;
                l7_cat_dir = dl;

                if (cat_y >= 0x28 && l7_heart_index == 6) {
                    l7_cat_dir = (dl == 0xff) ? 1 : 0xff;
                }
            }
        }
    }

movement:
    l7_heart_speed = (l7_cat_delay != 0) ? 4 : 8;
    {
        int32_t ax = l7_cat_x;
        if (l7_cat_dir == 0) {
            uint8_t r = (uint8_t)(cga_random() & 0xFF);
            if (r <= 0x10) {
                l7_cat_dir = (r & 1) ? 1 : 0xff;
            }
            goto cga_addr;   /* dir == 0: el ASM salta directo al calculo de direccion CGA, sin avanzar la animacion */
        } else if (l7_cat_dir == 1) {
            ax += l7_heart_speed;
            if (ax >= 0x10b) {
                ax = 0x10a;
                l7_cat_dir = 0xff;
                l7_cat_delay = 0;
            }
            l7_cat_x = (int16_t)ax;
            goto anim;
        } else { /* 0xff, moving left */
            ax -= l7_heart_speed;
            if (ax < 0 || ax <= 0x24) {
                ax = 0x25;
                l7_cat_dir = 1;
                l7_cat_delay = 0;
            }
            l7_cat_x = (int16_t)ax;
            goto anim;
        }
    }
anim:
    l7_cat_anim_idx = (uint16_t)(l7_cat_anim_idx + 2);
    if (l7_cat_anim_idx >= 0xc) l7_cat_anim_idx = 0;

    if (l7_cat_delay == 0) {
        uint8_t r = (uint8_t)(cga_random() & 0xFF);
        if (r <= 8) l7_cat_dir = 0;
    }

cga_addr:
    l7_heart_cga_addr = (uint16_t)calc_cga_addr(l7_cat_y, (uint16_t)l7_cat_x, NULL);

    if (check_l7_cupid()) return;

    if (!check_l7_cat_hit()) {
        erase_l7_sprite();
        setup_l7_sprite();
        l7_heart_caught = 0;
        check_l7_object_overlap();
    }

    save_l7_slot(l7_heart_index);
}

/* ===================================================================
 * Chunk 4: the victory-wave / cupid-pair cutscene
 * (position_victory_cat, animate_victory_pairs, init_victory_wave,
 * move_victory_object, run_victory_sequence).
 *
 * DELIBERATE EXCEPTION to this port's usual "never block the render
 * loop" rule (see level3_enemy.c's escape sequence and PROGRESS.md for
 * that rule and why it normally applies): this cutscene is a genuine
 * one-shot, non-interactive victory screen — the original blocks the
 * whole game on it by design, not as an artifact of DOS's cooperative
 * scheduling, and it only ever runs once per level-7 completion. A
 * frame-driven non-blocking state machine here would need to thread
 * three nested loop levels (the outer "keep waving until a bounce"
 * loop, the inner 8-object pass, and position_victory_cat's own
 * step/settle loop) through shared mutable counters — real risk for
 * limited benefit versus just... blocking, the way the original does,
 * for the few seconds this actually takes. Uses real host sleeps
 * (l7_sleep_ms) paced against the same ~55ms BIOS tick used everywhere
 * else in this port for consistency.
 * =================================================================== */

/* Real per-slot direction data (NOT runtime scratch — confirmed by
 * inspecting cat.asm's raw db content, unlike l7_cupid_x/y below which
 * genuinely are all-zero). Values only ever compared against 0/1, so
 * the original's slightly odd 0x00ff-as-"negative" encoding (instead of
 * the more usual 0xffff) doesn't matter — read as-is. */
#define L7_CUPID_X_DIR 0x4d6f
#define L7_CUPID_Y_DIR 0x4d7f
/* Per-wave parameter tables (3 entries each, one per wave — see
 * animate_victory_pairs), all real data. */
#define L7_CUPID_SPRITE_TBL 0x4d92
#define L7_CUPID_X_INIT_TBL 0x4d98
#define L7_CUPID_Y_INIT_TBL 0x4d9e
#define L7_CUPID_DX_TBL      0x4da4
#define L7_CUPID_DY_TBL      0x4daa
#define L7_CUPID_X_MAX_TBL   0x4db0
#define L7_CUPID_Y_MAX_TBL   0x4db6
#define L7_CUPID_DIMS_TBL    0x4dbc
#define L7_CUPID_SAVE_TBL    0x4dc2
/* position_victory_cat's own two sprites. */
#define L7_POSITION_SPRITE 0x4b8a /* dat_4b8a, 7 words x 32 rows */
#define L7_POSITION_ICON   0x4a82 /* dat_4a82, gato 4 words x 13 filas (cx=0xd04) */

static int16_t  l7_cupid_x[8];      /* real scratch, all-zero in .data */
static int16_t  l7_cupid_y[8];
static int16_t  l7_cupid_dx;
static int16_t  l7_cupid_dy;
static uint8_t  l7_cupid_bounce_cnt;
static uint8_t  l7_cupid_active;
static uint16_t l7_cupid_x_init, l7_cupid_y_init, l7_cupid_x_max, l7_cupid_y_max;
static uint16_t l7_cupid_dims, l7_cupid_sprite_ptr, l7_cupid_save_ptr;
static uint16_t l7_cupid_tick;

/* [0x414]/[0x412] — two globals not yet consumed anywhere else in this
 * port (read/written by the still-unported score-bar/HUD subsystem
 * too). Exposed via level7_epilogue.h rather than kept static, so a
 * future port of that subsystem has a real value to pick up. */
uint16_t l7_completion_counter; /* [0x414] */
/* l7_completion_tick: macro de level7_epilogue.h sobre start_tick ([0x412]) */

/* sound.asm is ported now (src/sound.c), so these forward to the real
 * functions instead of being no-ops. Kept as _stub-suffixed wrappers so the
 * call sites below stay a literal match for the assembly's own call order.
 * handle_level_complete still belongs to a separate, large, still-unported
 * scoring/HUD subsystem (level_objects.asm's score-bar animation). */
static void play_victory_note_stub(void) { play_victory_note(); }
static void play_swoop_sound_stub(void) { play_swoop_sound(); }
static void init_victory_melody_stub(void) { init_victory_melody(); }
static void play_full_victory_stub(void) { play_full_victory(); }
static void init_victory_melody_call(void) { init_victory_melody_stub(); }
static void silence_speaker_stub(void) { silence_speaker(); }

/* move_victory_object — literal port. */
static void move_victory_object(int slot) {
    int32_t ax = l7_cupid_x[slot];
    uint16_t xdir = ds_word((uint16_t)(L7_CUPID_X_DIR + slot * 2));
    if (xdir >= 1) {
        if (xdir == 1) {
            ax += l7_cupid_dx;
            if (ax > (int32_t)l7_cupid_x_max) { ax = l7_cupid_x_max; l7_cupid_bounce_cnt++; }
        } else {
            ax -= l7_cupid_dx;
            if (ax < 0) { ax = 0; l7_cupid_bounce_cnt++; }
        }
        l7_cupid_x[slot] = (int16_t)ax;
    }

    ax = l7_cupid_y[slot];
    uint16_t ydir = ds_word((uint16_t)(L7_CUPID_Y_DIR + slot * 2));
    if (ydir >= 1) {
        if (ydir == 1) {
            ax += l7_cupid_dy;
            if (ax > (int32_t)l7_cupid_y_max) { ax = l7_cupid_y_max; l7_cupid_bounce_cnt++; }
        } else {
            ax -= l7_cupid_dy;
            if (ax < 0) { ax = 0; l7_cupid_bounce_cnt++; }
        }
        l7_cupid_y[slot] = (int16_t)ax;
    }
}

/* init_victory_wave — literal port. Runs one wave (a set of up to 8
 * cupid-pair positions bouncing inside a box) until at least one bounce
 * happens in a pass. Only the "lead" slot (7) actually gets drawn each
 * pass (matching the original's `cx==8` check exactly) — the trailing
 * slots move but aren't drawn, and nothing is erased between draws, so
 * the lead's positions accumulate into a visible trail. */
static void init_victory_wave(void) {
    l7_cupid_active = 1;
    for (int cx = 8; cx >= 1; cx--) {
        play_victory_note_stub();
        int slot = cx - 1;
        l7_cupid_x[slot] = (int16_t)l7_cupid_x_init;
        l7_cupid_y[slot] = (int16_t)l7_cupid_y_init;
    }

    l7_cupid_bounce_cnt = 0;
    do {
        uint16_t wave_start = read_bios_tick();
        for (int cx = 8; cx >= 1; cx--) {
            play_victory_note_stub();
            int slot = cx - 1;
            if (l7_cupid_active == 0 || cx == 8) {
                uint16_t addr = (uint16_t)calc_cga_addr((uint8_t)l7_cupid_y[slot],
                                                          (uint16_t)l7_cupid_x[slot], NULL);
                blit_transparent(&ds_pool[l7_cupid_sprite_ptr], addr,
                                  (uint8_t)(l7_cupid_dims & 0xff), (uint8_t)(l7_cupid_dims >> 8), NULL);
            }
            move_victory_object(slot);
        }
        do {
            play_victory_note_stub();
            l7_sleep_ms(5);
        } while ((uint16_t)(read_bios_tick() - wave_start) < l7_cupid_save_ptr);
        /* T84d: el ASM (lab_520c) pone l7_cupid_active = 0 al final de CADA pasada de espera, no al terminar la
         * oleada. Asi solo la primera pasada dibuja unicamente el slot lider (cx == 8) y las siguientes dibujan los
         * 8 corazones, que salen en 8 direcciones y van llenando la pantalla. Antes el port solo dibujaba el lider
         * hasta el final de la oleada. */
        l7_cupid_active = 0;
    } while (l7_cupid_bounce_cnt == 0);
}

/* animate_victory_pairs — literal port: runs 3 waves back to back, each
 * with its own physics/sprite parameters loaded from the *_tbl tables. */
static void animate_victory_pairs(void) {
    for (int cx = 3; cx >= 1; cx--) {
        int idx = 3 - cx;
        uint16_t bx = (uint16_t)(idx * 2);
        l7_cupid_dx = (int16_t)ds_word((uint16_t)(L7_CUPID_DX_TBL + bx));
        l7_cupid_dy = (int16_t)ds_word((uint16_t)(L7_CUPID_DY_TBL + bx));
        l7_cupid_x_init = ds_word((uint16_t)(L7_CUPID_X_INIT_TBL + bx));
        l7_cupid_y_init = ds_word((uint16_t)(L7_CUPID_Y_INIT_TBL + bx));
        l7_cupid_x_max = ds_word((uint16_t)(L7_CUPID_X_MAX_TBL + bx));
        l7_cupid_y_max = ds_word((uint16_t)(L7_CUPID_Y_MAX_TBL + bx));
        l7_cupid_dims = ds_word((uint16_t)(L7_CUPID_DIMS_TBL + bx));
        l7_cupid_sprite_ptr = ds_word((uint16_t)(L7_CUPID_SPRITE_TBL + bx));
        l7_cupid_save_ptr = ds_word((uint16_t)(L7_CUPID_SAVE_TBL + bx));
        init_victory_wave();
    }
}

/* position_victory_cat — literal port: walks the cat toward horizontal
 * center while lowering it down the screen in 8px steps, drawing the
 * cupid-carries-cat sprite pair at each step and pacing each step by
 * `l7_cupid_tick` BIOS ticks (10 for the first step, 2 thereafter). */
static uint16_t pos_sprite_save[7 * 32];   /* T84b: fondo bajo el sprite del paso anterior */

/* T84c: el ASM (level_objects.asm L3534-3620) NO borra el corazon de pasos anteriores: pasa bp=0xe a
 * blit_transparent solo como buffer basura y nunca restaura. 0 = fiel al original (estela de corazones con gato
 * mientras baja). 1 = restaurar el fondo de cada paso (desviacion de T84b, por si en pantalla real se prefiere). */
#ifndef L7_POS_ERASE_TRAIL
#define L7_POS_ERASE_TRAIL 0
#endif

#if L7_POS_ERASE_TRAIL
static uint16_t pos_icon_save[4 * 13];     /* fondo bajo el gato del paso anterior */
static uint16_t pos_addr;
static bool     pos_drawn;
#endif

static void position_victory_cat(void) {
#if L7_POS_ERASE_TRAIL
    pos_drawn = false;
#endif
    int32_t ax = cat_x;
    if (ax >= 0x117) ax = 0x116;
    ax -= 0x10;
    if (ax < 0) ax = 0;
    ax &= 0xff0;
    cat_x = (int16_t)ax;
    cat_y = 0x14;

    int32_t d = ax - 0x80;
    if (d < 0) d = ~d;
    d >>= 3;
    if (d > 0xd) d = 0xd;
    d += 2;
    l7_cupid_dx = (int16_t)d;
    l7_cupid_tick = 0xa;

    for (;;) {
        if (l7_cupid_tick != 0xa) play_victory_note_stub();
        uint16_t step_start = read_bios_tick();

        int32_t cx_val = cat_x;
        int32_t cx_masked = cx_val & 0xff0;
        if (cx_masked == 0x80) {
            cx_val = cx_masked;
        } else if (cx_masked < 0x80) {
            cx_val += l7_cupid_dx;
        } else {
            cx_val -= l7_cupid_dx;
        }
        cat_x = (int16_t)cx_val;

        if (cat_y >= 0x54) return;

        cat_y = (uint8_t)(cat_y + 8);
        uint16_t addr = (uint16_t)calc_cga_addr(cat_y, (uint16_t)(cat_x + 4), NULL);
        /* El segundo sprite es un gato de 4 words x 13 filas (mov cx,0xd04: cl=ancho, ch=alto), a (x+12, y+6).
         * T84c: se pasaba (13, 4) invertido y se dibujaba ruido (las "flechas" de las capturas). */
#if L7_POS_ERASE_TRAIL
        if (pos_drawn) {
            blit_to_cga((const uint8_t *)pos_icon_save, (size_t)(pos_addr + 0xf3), 4, 13);
            blit_to_cga((const uint8_t *)pos_sprite_save, pos_addr, 7, 32);
        }
#endif
        blit_transparent(&ds_pool[L7_POSITION_SPRITE], addr, 7, 32, pos_sprite_save);
#if L7_POS_ERASE_TRAIL
        save_from_cga((uint8_t *)pos_icon_save, (size_t)(addr + 0xf3), 4, 13);
#endif
        blit_to_cga(&ds_pool[L7_POSITION_ICON], (size_t)(addr + 0xf3), 4, 13);
#if L7_POS_ERASE_TRAIL
        pos_addr = addr;
        pos_drawn = true;
#endif
        l7_present();   /* un cuadro por paso, sin depender de cuanto dure la espera */

        if (l7_cupid_tick == 0xa) {
            play_swoop_sound_stub();
            init_victory_melody_call();
        }

        for (;;) {
            play_victory_note_stub();
            if ((uint16_t)(read_bios_tick() - step_start) >= l7_cupid_tick) break;
            l7_sleep_ms(5);
        }

        if (l7_cupid_tick == 0xa) {
            handle_level_complete();                /* level_objects.asm L3618 (T47) */
            /* int 10h, ah=0xb, bx=0: borde/fondo a negro (T78: palette.c lo modela) */
            bios_color_select(0x0, 0x0);
        }
        l7_cupid_tick = 2;
    }
}

/* run_victory_sequence — literal port, the top-level entry point. */
void run_victory_sequence(void) {
    init_victory_melody_call();
    /* original also selects CGA palette 1 here on non-PCjr hardware
     * (int 10h, ah=0xb, bx=0x101) — this port always renders the one
     * fixed palette, so there's nothing to select. */

    position_victory_cat();
    animate_victory_pairs();
    play_full_victory_stub();
    l7_present();

    if (lives_count < 9) lives_count++;
    if (difficulty_level < 7) difficulty_level++;

    /* [0x414]/[0x412] — two globals not yet mapped anywhere else in this
     * port (read/written by the still-unported score-bar/HUD subsystem
     * too). Tracked here so a future port of that subsystem has a real
     * value to pick up rather than starting from nothing. */
    l7_completion_counter = 0;
    l7_completion_tick = read_bios_tick();

    silence_speaker_stub();
}

/* ===================================================================
 * Chunk 5: the PC-speaker march-music sequencer
 * (play_march_note, draw_march_frame, play_victory_march) — this closes
 * out the level-7 epilogue subsystem (§17 item (e)).
 *
 * play_victory_march runs once, from level_transition, right after
 * run_victory_sequence when leaving level 7 — same "genuine one-shot
 * cutscene" situation as chunk 4, so it's blocking for the same reason
 * (see the long comment above run_victory_sequence). draw_march_frame
 * and play_march_note are its two helpers, not independently blocking.
 * =================================================================== */

#define L7_BG_SPRITE_SEQ  0x52ca /* real data: note/sprite-select word sequence */
#define L7_BG_SPRITE_PTRS 0x5012 /* 2-entry word pointer table (two marching poses) */
#define L7_BG_PATTERN_TBL 0x52ae /* per-difficulty word table of {dst,flag}-pair-list pointers */

static uint16_t l7_bg_sprite_idx;  /* 0x5010 */
static uint16_t l7_bg_xor_flag;    /* 0x5016 */
static uint16_t l7_bg_last_tick_1; /* 0x52c0 */
static uint16_t l7_bg_last_tick_2; /* 0x52c2 */
static uint16_t l7_bg_anim_tick;   /* 0x52c4 */
static uint16_t l7_bg_anim_idx;    /* 0x52c6 */
static uint16_t l7_bg_cur_sprite;  /* 0x52c8 */

/* play_march_note — literal port. The PIT/speaker half is wired for real
 * now that sound.asm is ported: `out 0x43,0xB6` then set_speaker_freq,
 * exactly as the original does. NOTE the value being programmed is the same
 * word the routine compares against l7_bg_cur_sprite — in this routine the
 * sequence table's entries double as both the note divisor and the
 * repeat-detection key, which is why the variable is named for a sprite. */
static void play_march_note(void) {
    if (!sound_enabled) return;
    uint16_t tick = read_bios_tick();
    if (tick == l7_bg_anim_tick) return;
    l7_bg_anim_tick = tick;

    uint16_t bx = l7_bg_anim_idx;
    l7_bg_anim_idx = (uint16_t)(l7_bg_anim_idx + 2);
    uint16_t note = ds_word((uint16_t)(L7_BG_SPRITE_SEQ + bx));
    if (note == l7_bg_cur_sprite) {
        silence_speaker_stub();
        return;
    }
    l7_bg_cur_sprite = note;
    pit_out_43(0xb6);
    set_speaker_freq(note);
}

/* draw_march_frame — literal port. Walks a difficulty-selected,
 * null-terminated list of {dst_offset, pose_flag} pairs, blitting one of
 * two marching-pose sprites (xor'd against l7_bg_xor_flag, which
 * play_victory_march toggles once per outer pass) to each destination. */
static void draw_march_frame(void) {
    uint16_t bx = (uint16_t)((difficulty_level & 7) * 2);
    l7_bg_sprite_idx = ds_word((uint16_t)(L7_BG_PATTERN_TBL + bx));

    for (;;) {
        uint16_t entry_ptr = l7_bg_sprite_idx;
        uint16_t dst = ds_word(entry_ptr);
        if (dst == 0) return;

        uint16_t flag = ds_word((uint16_t)(entry_ptr + 2));
        uint16_t sel = (uint16_t)((flag ^ l7_bg_xor_flag) & 2);
        uint16_t sprite_ofs = ds_word((uint16_t)(L7_BG_SPRITE_PTRS + sel));

        blit_to_cga(&ds_pool[sprite_ofs], dst, 4, 0x23);
        play_march_note();
        l7_bg_sprite_idx = (uint16_t)(l7_bg_sprite_idx + 4);
    }
}

/* play_victory_march — literal port, the top-level entry point. Redraws
 * the marching background (toggling pose each pass) roughly every 5
 * ticks, for a total of 0x28 (40) ticks measured from the very start —
 * `l7_bg_last_tick_2` is captured once and never updated, unlike
 * `l7_bg_last_tick_1` which paces the inner per-pass wait. */
void play_victory_march(void) {
    if (difficulty_level < 2) return;

    l7_bg_xor_flag = 0;
    uint16_t tick = read_bios_tick();
    l7_bg_last_tick_1 = tick;
    l7_bg_last_tick_2 = tick;
    l7_bg_anim_tick = tick;
    l7_bg_anim_idx = 0;
    l7_bg_cur_sprite = 0;

    uint16_t elapsed_total;
    do {
        draw_march_frame();
        l7_bg_xor_flag ^= 2;

        uint16_t now;
        do {
            play_march_note();
            now = read_bios_tick();
            l7_sleep_ms(5);
        } while ((uint16_t)(now - l7_bg_last_tick_1) < 5);
        l7_bg_last_tick_1 = now;

        elapsed_total = (uint16_t)(now - l7_bg_last_tick_2);
    } while (elapsed_total < 0x28);

    silence_speaker_stub();
}
