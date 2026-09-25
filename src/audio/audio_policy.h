#ifndef CARROM_AUDIO_POLICY_H
#define CARROM_AUDIO_POLICY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Audio policy: WHICH sound, how LOUD, which VARIANT, and whether to play it at all
 * (rate limiting). Pure logic with no audio device, so it is unit-tested and the same in
 * headless runs. Sounds always play at natural pitch, whatever the game speed.
 * --------------------------------------------------------------------------- */

typedef enum {
    CUE_FLICK = 0,
    CUE_STRIKER_COIN,
    CUE_COIN_COIN,
    CUE_STRIKER_WALL,
    CUE_COIN_WALL,
    CUE_STRIKER_POCKET,
    CUE_COIN_POCKET,
    CUE_QUEEN,
    CUE_FOUL,
    CUE_BOARD_WON,
    CUE_COUNT
} AudioCue;

typedef struct {
    double last_played[CUE_COUNT];
    unsigned counter[CUE_COUNT];
    bool ever[CUE_COUNT];
} AudioPolicy;

void audio_policy_init(AudioPolicy* p);

/* Map a physics SoundKind (physics.h) to its cue */
AudioCue audio_cue_for_sound_kind(int sound_kind);

/* Loudness 0..1 for a cue given the impact/launch speed (board units per second) */
float audio_policy_loudness(AudioCue cue, float speed);

/* Decide whether to play. On true, *volume (0..1) and *variant (0..variants-1) are set.
 * `now` is wall-clock seconds. Impacts closer together than the cue's minimum gap are dropped, as are
 * impacts too soft to hear. */
bool audio_policy_admit(AudioPolicy* p, AudioCue cue, float speed, double now, int variants, float* volume, int* variant);

#ifdef __cplusplus
}
#endif

#endif /* CARROM_AUDIO_POLICY_H */
