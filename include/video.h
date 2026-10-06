#ifndef VIDEO_H
#define VIDEO_H

#include <stdbool.h>

/* CGA mode 4. Los colores de los 4 indices salen de palette_rgb() (palette.h, T44): el estado del
 * puerto 0x3D9 / de los registros de paleta PCjr que mantiene set_palette(). */
bool video_init(int window_scale);
void video_present(void);   /* uploads cga_mem -> texture, and renders it */
void video_shutdown(void);

#endif
