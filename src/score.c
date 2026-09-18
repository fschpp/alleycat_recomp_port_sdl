#include "cat_state.h"
#include "cga.h"
#include "score.h"
#include "gen/digit_sprites.h"
#include <string.h>

/* CGA screen positions — these are immediate constants in the original
 * (mov di,label with no brackets loads the label's OWN address as a
 * literal value, same pattern already found for dat_1b02 in §5u), not
 * data reads. Verified in-range for CGA_MEM_SIZE (0x4000). */
#define LIVES_CGA_POS        0x1260
#define HIGH_SCORE_CGA_POS   0x12ca
#define CURRENT_SCORE_CGA_POS 0x143c

/* zero_score_buffer — literal port. */
static void zero_score_buffer(unsigned char *buf) {
    memset(buf, 0, 7);
}

void clear_score(void) {
    zero_score_buffer(current_score);
}

void clear_high_score(void) {
    zero_score_buffer(high_score);
}

/* render_score_digits — literal port. Draws 7 BCD digits left-to-right
 * from `buf`, each via the shared digit_sprite_data (1 word x 8 rows =
 * 16B per digit, indexed by digit*16), with an extra 2px gap inserted
 * after the 3rd digit (a thousands-grouping visual gap). */
static void render_score_digits(const unsigned char *buf, uint16_t draw_pos) {
    score_draw_pos = draw_pos;
    score_buf_ptr = buf;
    score_digit_idx = 0;

    for (;;) {
        unsigned char digit = *score_buf_ptr;
        const uint8_t *sprite = &digit_sprite_data[(size_t)digit * 16];
        blit_to_cga(sprite, score_draw_pos, 1, 8);

        score_draw_pos = (uint16_t)(score_draw_pos + 2);
        score_buf_ptr++;
        score_digit_idx++;

        if (score_digit_idx == 7) break;
        if (score_digit_idx == 3) score_draw_pos = (uint16_t)(score_draw_pos + 2);
    }
}

/* draw_current_score / draw_high_score_display — see score.h's note:
 * named by content (verified against update_high_score's clear
 * current/high buffer comment), not by the original's swapped labels. */
void draw_current_score(void) {
    render_score_digits(current_score, CURRENT_SCORE_CGA_POS);
}

void draw_high_score_display(void) {
    render_score_digits(high_score, HIGH_SCORE_CGA_POS);
}

/* draw_lives — literal port. Only redraws when lives_count changed since
 * the last call, using the same shared digit sprite sheet. */
void draw_lives(void) {
    if (lives_count == lives_display) return;
    lives_display = lives_count;
    const uint8_t *sprite = &digit_sprite_data[(size_t)lives_count * 16];
    blit_to_cga(sprite, LIVES_CGA_POS, 1, 8);
}

/* add_score — literal port of the BCD-add-with-carry-propagation loop.
 * The original indexes via a mislabeled "lives_display"-based pointer
 * that actually lands on current_score[1..6] (verified in §5v — the
 * label is an artifact, not really lives_display); this port just
 * operates on current_score directly, which is what it always meant. */
void add_score(unsigned char bcd_digit) {
    unsigned char carry = bcd_digit;
    for (int i = 5; i >= 0; i--) {
        unsigned char sum = (unsigned char)(current_score[i + 1] + carry);
        /* These buffer bytes are unpacked decimal digits (0-9 each), not
         * packed BCD nibbles — the original's AAA instruction, applied
         * to values in this restricted 0-9(+carry) domain, is exactly
         * equivalent to plain "if sum>=10, subtract 10 and carry 1."
         * (An earlier draft of this function tried to approximate AAA's
         * nibble/aux-carry check directly and got it wrong for sums like
         * 9+9+1=19, whose low nibble (3) isn't >9 despite needing a
         * carry — decimal sum>=10 is the correct, simpler equivalent.) */
        if (sum >= 10) {
            current_score[i + 1] = (unsigned char)(sum - 10);
            carry = 1;
        } else {
            current_score[i + 1] = sum;
            carry = 0;
        }
        if (carry == 0) break;
    }
    draw_current_score();
}

/* add_bcd_scores — literal port, right-to-left BCD add with carry
 * (matches current_score/high_score's 7-byte MSB-first layout). */
void add_bcd_scores(unsigned char *dst, const unsigned char *src) {
    unsigned char carry = 0;
    for (int i = 6; i >= 0; i--) {
        unsigned char sum = (unsigned char)(dst[i] + src[i] + carry);
        /* same decimal-digit equivalence as add_score, see its comment */
        if (sum >= 10) {
            dst[i] = (unsigned char)(sum - 10);
            carry = 1;
        } else {
            dst[i] = sum;
            carry = 0;
        }
    }
}

/* update_high_score — literal port: lexicographically compares the two
 * 7-digit BCD buffers (matching the original's byte-by-byte `loopz`
 * comparison scan), copying current -> high if current is larger. */
void update_high_score(void) {
    int i;
    for (i = 0; i < 7; i++) {
        if (current_score[i] != high_score[i]) break;
    }
    if (i < 7 && current_score[i] > high_score[i]) {
        memcpy(high_score, current_score, 7);
    }
}
