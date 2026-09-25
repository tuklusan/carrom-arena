#ifndef CARROM_AUDIO_H
#define CARROM_AUDIO_H

#include <stdbool.h>
#include "audio_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sound output (raylib / miniaudio). The samples are embedded in the executable.
 * Every call is a harmless no-op when no audio device is available (Xvfb, CI, headless runs). */
bool audio_init(void);            /* true when a device is ready and the samples loaded */
void audio_shutdown(void);
bool audio_ready(void);
int  audio_variant_count(AudioCue cue);
void audio_play(AudioCue cue, float volume, int variant);
void audio_toggle_mute(void);     /* the M key */
bool audio_is_muted(void);

#ifdef __cplusplus
}
#endif

#endif /* CARROM_AUDIO_H */
