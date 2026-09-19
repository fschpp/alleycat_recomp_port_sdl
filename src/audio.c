/* audio.c — the only SDL-aware part of the sound pipeline. Opens an audio
 * device and pumps include/speaker.h's emulated PC speaker into it. All the
 * actual sound generation lives in speaker.c (pure C, unit-testable). */

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "audio.h"
#include "speaker.h"

static SDL_AudioDeviceID audio_dev = 0;

static void SDLCALL audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    speaker_render((int16_t *)stream, (size_t)len / sizeof(int16_t));
}

bool audio_init(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;

    SDL_AudioSpec want = {
        .freq     = SPEAKER_SAMPLE_RATE,
        .format   = AUDIO_S16SYS,
        .channels = 1,
        /* ~11.6 ms. Small enough that the speaker's direct-PWM effects stay
         * responsive, large enough not to starve under load. */
        .samples  = 512,
        .callback = audio_callback,
    };
    SDL_AudioSpec have;

    speaker_reset();
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_dev == 0) return false;

    SDL_PauseAudioDevice(audio_dev, 0);
    return true;
}

void audio_shutdown(void) {
    if (audio_dev != 0) {
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
