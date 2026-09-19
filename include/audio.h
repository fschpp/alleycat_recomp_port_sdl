#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

/* SDL2 audio device for the emulated PC speaker (see include/speaker.h).
 * Returns false if no device could be opened — the game still runs, just
 * silently, exactly as it does with sound_enabled == 0. */
bool audio_init(void);
void audio_shutdown(void);

#endif
