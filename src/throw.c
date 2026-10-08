#include "bios_clock.h"
#include "throw.h"
#include "cat_state.h"
#include "cga.h"
#include "gen/ds_pool.h"
#include <string.h>
#include <time.h>

/* Offsets DS verificados con tools/asm_label.sh y por cómo se indexan. */
#define DS_THROW_SPRITE_SMALL 0x0460u /* 32 bytes (0x10 words), hasta 0x0480 */
#define DS_THROW_SPRITE_LARGE 0x0490u /* 64 bytes (0x20 words), hasta 0x04d0 */
#define DS_THROW_PATTERN_PTRS 0x04d0u /* 3 words: 0x480, 0x440, 0x450 */
#define DS_THROW_ROTATE_DIR   0x0541u /* 3 bytes: 00 09 0a */
#define DS_FLOOR_Y_BOTTOM     0x053au /* 3 bytes: 4b 6b 8b */
#define DS_FLOOR_Y_TOP        0x053du /* 3 bytes: 2e 4e 6e */
/* T13 — todas verificadas contra data_segment.bin y por cómo se indexan:
 *   throw_scroll_src 0x0517: 3 words (indexado con bx*2, piso 0..2): 03be 0641 0dbe
 *   throw_draw_col   0x051d: 3 words: 0140 068f 0b40 (termina justo en throw_draw_tmp 0x0523)
 *   throw_col_init   0x0526: 3 bytes: 03 00 03
 *   throw_col_step   0x0529: 3 bytes: ff 01 ff
 *   throw_y_param    0x052c: 3 bytes: 80 30 00
 *   throw_delay      0x0532: 8 bytes (indexado por difficulty_level, termina en 0x053a)
 *   throw_chance     0x2aba: 8 bytes (idem) */
#define DS_THROW_SCROLL_SRC   0x0517u
#define DS_THROW_DRAW_COL     0x051du
#define DS_THROW_COL_INIT     0x0526u
#define DS_THROW_COL_STEP     0x0529u
#define DS_THROW_Y_PARAM      0x052cu
#define DS_THROW_DELAY        0x0532u
#define DS_THROW_CHANCE       0x2abau

/* Desviacion consciente (bug 5, misma idea que JUMP_TICK_RELOAD de T80b): throw_timer cuenta LLAMADAS, no ticks, y
 * el valor de throw_delay esta calibrado para el lazo sin retardo del original. En el port (~7,5 llamadas/s) los
 * tendederos tardaban ~13 s entre pasos. La recarga se divide por THROW_TIMER_DIV (minimo 1). Es la unica
 * constante a tocar si la velocidad no convence en el PC real. */
#define THROW_TIMER_DIV 32u

uint8_t throw_obj_buf[64] = {0};
uint8_t throw_bits = 0;
uint8_t in_throw_range = 0;
uint8_t throw_timer = 0;
uint16_t throw_last_tick = 0;

/* int 0x1a AH=0 -> DX (palabra baja del contador BIOS, 18.2 Hz); mismo patrón
 * que alley.c / level7_epilogue.c. */
static uint16_t read_bios_tick(void) {
    return bios_clock_read();
}

static uint16_t ds_word_t(uint16_t ofs) {
    return (uint16_t)(ds_pool[ofs] | (ds_pool[ofs + 1] << 8));
}

/* rotate_throw_bits — rcr/rcl en cadena sobre 5 bytes de throw_col_data. */
bool rotate_throw_bits(bool carry_in) {
    uint16_t bx = ds_pool[DS_THROW_ROTATE_DIR + (current_floor & 3)]; /* bh = 0 */
    uint8_t cf = carry_in ? 1 : 0;
    if (bx == 0x9) {                          /* lab_064e: rcl, bx decrece */
        for (int cx = 5; cx > 0; cx--) {
            uint8_t b = throw_col_data[bx];
            uint8_t out = (uint8_t)(b >> 7);
            throw_col_data[bx] = (uint8_t)((b << 1) | cf);
            cf = out;
            bx--;
        }
    } else {                                  /* lab_0644: rcr, bx crece */
        for (int cx = 5; cx > 0; cx--) {
            uint8_t b = throw_col_data[bx];
            uint8_t out = (uint8_t)(b & 1);
            throw_col_data[bx] = (uint8_t)((b >> 1) | (cf << 7));
            cf = out;
            bx++;
        }
    }
    return cf != 0;
}

/* check_throw_range — devuelve ZF. */
bool check_throw_range(uint16_t floor) {
    in_throw_range = 0;
    uint8_t al = cat_y_bottom;
    if (al < ds_pool[DS_FLOOR_Y_TOP + floor]) return true;     /* jc lab_0679: cmp al,al -> ZF=1 */
    if (al >= ds_pool[DS_FLOOR_Y_BOTTOM + floor]) return true; /* jnc lab_0679 */
    if (at_platform >= 1) {                                    /* cmp at_platform,1 / jnc */
        in_throw_range = 1;
        return true;                                           /* lab_0679 */
    }
    return false;  /* ret directo: cmp 0,1 deja ZF=0 */
}

uint8_t generate_throw_pattern(uint16_t *di_off) {
    uint16_t dx = (uint16_t)(cga_random() & 0x6);
    if ((uint8_t)dx == 6) return 0;                            /* sub al,al */
    uint16_t si = ds_word_t((uint16_t)(DS_THROW_PATTERN_PTRS + dx));
    uint16_t di = *di_off;
    for (int cx = 8; cx > 0; cx--) {                           /* lodsw / stosw / add di,2 */
        throw_obj_buf[di]     = ds_pool[si];
        throw_obj_buf[di + 1] = ds_pool[si + 1];
        si += 2;
        di += 4;
    }
    *di_off = di;
    return 1;
}

void generate_throw_object(uint8_t bl, uint8_t bh) {
    throw_bits = 0;
    for (int i = 0; i < 64; i += 2) {                          /* rep stosw 0xaaaa x 0x20 */
        throw_obj_buf[i] = 0xaa;
        throw_obj_buf[i + 1] = 0xaa;
    }
    uint16_t di = 0;                                           /* sub di,0x40 */
    memset(&throw_obj_buf[di + 4], 0x44, 4);                   /* words [di+4],[di+6] = 0x4444 */

    uint16_t dx = cga_random();
    uint8_t dl = (uint8_t)dx, dh = (uint8_t)(dx >> 8);
    if (dl < bl) return;                                       /* jc lab_06a4 */
    if (!(dh > bh)) return;                                    /* ja lab_06a5, si no ret */

    /* lab_06a5 */
    dl = (uint8_t)cga_random();
    if (dl < 0x18) {                                           /* lab_06c7: sprite grande */
        memcpy(throw_obj_buf, &ds_pool[DS_THROW_SPRITE_LARGE], 64);
        throw_bits = 0x3;
        return;
    }
    if (dl < 0x60) {                                           /* lab_06d0: sprite chico */
        memcpy(throw_obj_buf, &ds_pool[DS_THROW_SPRITE_SMALL], 32);
        throw_bits = 0x3;
        return;
    }
    {
        uint16_t d1 = di;                                      /* push di */
        uint8_t al = generate_throw_pattern(&d1);
        al = (uint8_t)(al << 1);
        throw_bits = al;
        uint16_t d2 = (uint16_t)(di + 2);                      /* pop di; add di,2 */
        al = generate_throw_pattern(&d2);
        throw_bits |= al;
    }
}

/* rep movsb sobre el segmento B800 (cga_mem). down = DF=1 (std): si/di decrecen.
 * Las direcciones se enmascaran con 0x3fff: el CGA real refleja 16 KB. */
static void cga_rep_movsb(uint16_t si, uint16_t di, uint16_t cx, bool down) {
    while (cx--) {
        cga_mem[di & (CGA_MEM_SIZE - 1)] = cga_mem[si & (CGA_MEM_SIZE - 1)];
        if (down) { si--; di--; } else { si++; di++; }
    }
}

/* update_thrown_objects — port literal de throw.asm L5-167 (etiquetas lab_XXXX
 * como en el ASM). bx del original = piso (word) en casi todo el recorrido.
 *
 * Simplificaciones documentadas:
 *  - check_vsync (L12) = no-op con ZF=1: el `jnz lab_04a6` nunca salta.
 *  - int 0x1a -> read_bios_tick().
 *  - std/cld (L133-139): el original deja DF=1 al salir en los pisos 0 y 2; en C
 *    no hay DF, solo se respeta la dirección de la copia (equivale a memmove).
 *  - `push ds`/`pop ds` y ES=B800 se resuelven usando cga_mem directamente. */
void update_thrown_objects(void) {
    uint16_t bx;
    uint8_t al, dl;
    bool cf;

    throw_timer--;                                   /* dec byte [throw_timer] */
    if (throw_timer != 0) return;                    /* jz lab_04a7 / lab_04a6: ret */
    /* lab_04a7 */
    throw_timer++;                                   /* inc byte [throw_timer] */
    /* call check_vsync; jnz lab_04a6  -> ZF=1 siempre, no salta */
    if (transitioning != 0) return;                  /* jnz lab_04a6 */
    if (gravity_y != 0) return;                      /* jnz lab_04a6 */
    {
        uint16_t dx = read_bios_tick();              /* sub ah,ah; int 0x1a */
        if (dx == throw_last_tick) return;           /* jz lab_04a6 */
        throw_last_tick = dx;
    }
    al = ds_pool[DS_THROW_DELAY + difficulty_level]; /* mov bx,[difficulty_level]; mov al,[bx+throw_delay] */
    if (!(cat_y > 0x60)) {                           /* cmp cat_y,0x60 / ja lab_04df */
        al >>= 1;
        al >>= 1;
    }
    /* lab_04df */
    al = (uint8_t)(al / THROW_TIMER_DIV);            /* desviacion: ver THROW_TIMER_DIV */
    if (al == 0) al = 1;
    throw_timer = al;
    bx = current_floor;
    if (check_throw_range(bx)) goto lab_050c;        /* jz lab_050c (ZF=1) */

    /* ZF=0: el gato está en este piso y at_platform==0 */
    al = (uint8_t)(window_column + ds_pool[DS_THROW_COL_STEP + bx]);
    if (al < 0x4) return;                            /* jc lab_04a6 */
    do {                                             /* lab_04f6 */
        dl = (uint8_t)(cga_random() & 0x3);          /* call random; and dl,3 */
    } while (dl == (uint8_t)current_floor || dl == 0x3); /* jz lab_04f6 x2 */
    bx = (uint16_t)((bx & 0xff00u) | dl);            /* mov bl,dl */
    goto lab_0535;

lab_050c:
    al = ds_pool[DS_THROW_COL_STEP + bx];
    window_column = (uint8_t)(window_column + al);
    if (window_column < 0x4) goto lab_0583;          /* jc lab_0583 */
    dl = (uint8_t)cga_random();
    if (dl > 0x40) goto lab_0539;                    /* ja lab_0539 (sin signo) */
lab_0523:
    dl = (uint8_t)(cga_random() & 0x3);
    if (dl == 0x3) goto lab_0523;                    /* jz lab_0523 */
    bx = (uint16_t)((bx & 0xff00u) | dl);            /* mov bl,dl */
    if (!check_throw_range(bx)) goto lab_0523;       /* jnz lab_0523 (ZF=0) */
lab_0535:
    current_floor = bx;
lab_0539:
    window_column = ds_pool[DS_THROW_COL_INIT + bx];
    /* reloc_2: ES = segmento de datos (solo marcador) */
    {
        uint8_t y_param = ds_pool[DS_THROW_Y_PARAM + bx];            /* mov ah,[bx+throw_y_param] */
        uint8_t chance  = ds_pool[DS_THROW_CHANCE + difficulty_level]; /* mov bl,[bx+throw_chance] */
        generate_throw_object(chance, y_param);                      /* bh = ah */
    }
    if (current_floor == 0x1) goto lab_056e;
    cf = (throw_bits & 1) != 0;                      /* shr byte [throw_bits],1 */
    throw_bits >>= 1;
    rotate_throw_bits(cf);
    cf = (throw_bits & 1) != 0;                      /* shr byte [throw_bits],1 */
    throw_bits >>= 1;
    goto lab_057c;
lab_056e:
    al = throw_bits;
    al >>= 1;
    cf = (al & 1) != 0;                              /* CF del 2.º shr = bit 1 original */
    al >>= 1;
    rotate_throw_bits(cf);
    cf = (throw_bits & 1) != 0;                      /* shr byte [throw_bits],1 (una sola vez) */
    throw_bits >>= 1;
lab_057c:
    rotate_throw_bits(cf);
    bx = current_floor;

lab_0583:
    if (in_throw_range == 0) goto lab_05d0;          /* jz lab_05d0 */
    {
        uint16_t ax = (uint16_t)cat_x;
        if ((uint8_t)bx == 0x1) goto lab_05bf;
        cat_draw_pos++;                              /* inc word [cat_draw_pos] */
        ax = (uint16_t)(ax + 4);
        if (ax < 0x123) goto lab_05cd;               /* cmp ax,0x123 / jc */
        goto lab_059e;
lab_05bf:
        cat_draw_pos--;
        {
            bool borrow = ax < 4;                    /* sub ax,4 / jc lab_059e */
            ax = (uint16_t)(ax - 4);
            if (borrow) goto lab_059e;
        }
        if (ax < 0x8) goto lab_059e;                 /* cmp ax,8 / jc */
lab_05cd:
        cat_x = (int16_t)ax;
        goto lab_05d0;
    }
lab_059e:
    transition_timer = 0x11;
    in_level_mode = 0x1;
    anim_counter = 0x1;
    anim_step = 0x18;
    scroll_speed = 0x1;
    at_platform = 0x0;

lab_05d0:
    {
        uint16_t b2 = (uint16_t)(bx << 1);           /* shl bx,1 */
        uint16_t draw_tmp = ds_word_t((uint16_t)(DS_THROW_DRAW_COL + b2));   /* -> throw_draw_tmp */
        uint16_t si = ds_word_t((uint16_t)(DS_THROW_SCROLL_SRC + b2));
        uint16_t di = si;
        bool down;
        if (b2 == 0x2) { down = false; di--; }       /* cld; dec di  (piso 1: scroll a la izquierda) */
        else           { down = true;  di++; }       /* std; inc di  (pisos 0/2: a la derecha) */
        /* lab_05f3 */
        cga_rep_movsb(si, di, 0x27f, down);                                   /* banco 0 */
        cga_rep_movsb((uint16_t)(si + 0x2000), (uint16_t)(di + 0x2000), 0x280, down); /* banco 1 */

        /* dibujo de la columna: 16 filas, 1 byte por fila, intercalando bancos */
        di = draw_tmp;
        {
            uint16_t bo = window_column;             /* mov bl,[window_column]; sub bh,bh */
            for (int cx = 0x10; cx > 0; cx--) {      /* lab_061b */
                cga_mem[di & (CGA_MEM_SIZE - 1)] = (bo < sizeof(throw_obj_buf)) ? throw_obj_buf[bo] : 0;
                bo += 4;
                di ^= 0x2000;
                if ((di & 0x2000) == 0) di += 0x50;  /* test di,0x2000 / jnz lab_0630 */
            }
        }
    }
}

/* reset_window_state — throw.asm L277-280. */
void reset_window_state(void) {
    anim_last_tick = 0;
    pcjr_delay = 0;
}
