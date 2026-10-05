#include "throw.h"
#include "cat_state.h"
#include "cga.h"
#include "gen/ds_pool.h"
#include <string.h>

/* Offsets DS verificados con tools/asm_label.sh y por cómo se indexan. */
#define DS_THROW_SPRITE_SMALL 0x0460u /* 32 bytes (0x10 words), hasta 0x0480 */
#define DS_THROW_SPRITE_LARGE 0x0490u /* 64 bytes (0x20 words), hasta 0x04d0 */
#define DS_THROW_PATTERN_PTRS 0x04d0u /* 3 words: 0x480, 0x440, 0x450 */
#define DS_THROW_ROTATE_DIR   0x0541u /* 3 bytes: 00 09 0a */
#define DS_FLOOR_Y_BOTTOM     0x053au /* 3 bytes: 4b 6b 8b */
#define DS_FLOOR_Y_TOP        0x053du /* 3 bytes: 2e 4e 6e */

uint8_t throw_obj_buf[64] = {0};
uint8_t throw_bits = 0;
uint8_t in_throw_range = 0;

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
