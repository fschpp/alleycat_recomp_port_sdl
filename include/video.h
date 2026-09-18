#ifndef VIDEO_H
#define VIDEO_H

#include <stdbool.h>

/* CGA mode 4, palette 1, high intensity: black, cyan, magenta, white.
 * (Alley Cat's title screen is unmistakably this palette - the original
 * sets this via the mode-select byte written in entry.asm.) */
bool video_init(int window_scale);
void video_present(void);   /* uploads cga_mem -> texture, and renders it */
void video_shutdown(void);

#endif
