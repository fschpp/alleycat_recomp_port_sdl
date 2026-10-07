/* update_animation — game_loop.asm L158-817 (T74, PROGRESS.md §6bj).
 * Solo despacha: cada tramo vive en su archivo (animation_entry.c, animation.c/movement.c, animation_c.c,
 * animation_d.c) y se probo por separado en T70-T73. */
#include "update_animation.h"
#include "animation_entry.h"
#include "animation.h"
#include "animation_c.h"
#include "animation_d.h"
#include "movement.h"

void update_animation(void) {
    switch (update_animation_entry()) {
    case UA_RET:                                    /* lab_08fc: ret */
        return;
    case UA_L09F6:                                  /* nivel 2, fase normal: lab_09f6 .. lab_0a86 */
        update_cat_movement();
        update_cat_frame();                         /* lab_0ace .. lab_0bab: ret */
        return;
    case UA_L0A86:                                  /* nivel 2, fase de muerte */
        update_cat_dive();
        update_cat_frame();
        return;
    case UA_L0BAC:                                  /* niveles != 2 */
        break;
    }
    {
        ac_next_t n = update_animation_c1();        /* lab_0bac .. lab_0ce7 */
        switch (n) {
        case AC_RET:
            return;
        case AC_L0CE7:
        case AC_L0D29:
            update_animation_c2(n);                 /* lab_0ce7 .. lab_0e1f: ret */
            return;
        case AC_L0E23:
        case AC_L0E78:
            update_animation_d(n);                  /* lab_0e23 .. lab_0f86: ret */
            return;
        }
    }
}
