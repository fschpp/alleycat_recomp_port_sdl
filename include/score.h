#ifndef SCORE_H
#define SCORE_H

/* Ported from score.asm — see src/score.c and PROGRESS.md §5v.
 * NOTE: function names here follow what each function actually DOES
 * (content is self-consistent with update_high_score's clear comment),
 * not the original disassembly's labels, which turned out to be swapped
 * relative to their content — see §5v. */

void clear_score(void);
void clear_high_score(void);

/* add_score — adds a BCD digit (0-9) to the ones place of current_score,
 * propagating carries leftward. */
void add_score(unsigned char bcd_digit);

/* add_bcd_scores — adds the 7-byte BCD buffer at `src` into `dst`,
 * right-to-left with carry propagation (both MSB-first, matching
 * current_score/high_score's layout). */
void add_bcd_scores(unsigned char *dst, const unsigned char *src);

void update_high_score(void);

/* draw_lives — redraws the lives counter only if it changed since the
 * last call (matches the original's cached-compare-then-blit pattern). */
void draw_lives(void);

/* draw_current_score / draw_high_score_display — render the 7-digit BCD
 * buffers to their respective fixed screen positions. Named by content,
 * not by the original's (swapped) labels — see §5v. */
void draw_current_score(void);
void draw_high_score_display(void);

#endif
