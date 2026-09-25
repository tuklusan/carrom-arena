#include "audio_policy.h"
#include "physics/physics.h"
#include <math.h>
#include <string.h>

typedef struct {
    float ref_speed;   /* speed that plays at full volume */
    float floor_vol;   /* volume of the softest audible sound */
    float min_speed;   /* below this the impact is inaudible and skipped */
    double min_gap;    /* seconds between two plays of the same cue */
} CueSpec;

static const CueSpec SPEC[CUE_COUNT] = {
    [CUE_FLICK]          = { 5.0f, 0.45f, 0.00f, 0.050 },
    [CUE_STRIKER_COIN]   = { 3.0f, 0.30f, 0.10f, 0.035 },
    [CUE_COIN_COIN]      = { 2.0f, 0.25f, 0.10f, 0.035 },
    [CUE_STRIKER_WALL]   = { 3.0f, 0.30f, 0.12f, 0.045 },
    [CUE_COIN_WALL]      = { 2.0f, 0.25f, 0.12f, 0.045 },
    [CUE_STRIKER_POCKET] = { 2.0f, 0.55f, 0.00f, 0.080 },
    [CUE_COIN_POCKET]    = { 2.0f, 0.50f, 0.00f, 0.060 },
    [CUE_QUEEN]          = { 1.0f, 0.90f, 0.00f, 0.300 },
    [CUE_FOUL]           = { 1.0f, 0.85f, 0.00f, 0.300 },
    [CUE_BOARD_WON]      = { 1.0f, 0.90f, 0.00f, 0.300 },
    [CUE_STRIKER_BOUNCE] = { 2.0f, 0.30f, 0.00f, 0.100 },
};

void audio_policy_init(AudioPolicy* p) {
    memset(p, 0, sizeof(*p));
}

AudioCue audio_cue_for_sound_kind(int k) {
    switch (k) {
        case SOUND_FLICK:          return CUE_FLICK;
        case SOUND_STRIKER_COIN:   return CUE_STRIKER_COIN;
        case SOUND_COIN_COIN:      return CUE_COIN_COIN;
        case SOUND_STRIKER_WALL:   return CUE_STRIKER_WALL;
        case SOUND_COIN_WALL:      return CUE_COIN_WALL;
        case SOUND_STRIKER_POCKET: return CUE_STRIKER_POCKET;
        case SOUND_COIN_POCKET:    return CUE_COIN_POCKET;
        default:                   return CUE_COIN_COIN;
    }
}

float audio_policy_loudness(AudioCue cue, float speed) {
    if (cue < 0 || cue >= CUE_COUNT) return 0.0f;
    const CueSpec* s = &SPEC[cue];
    float t = speed / s->ref_speed;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return s->floor_vol + (1.0f - s->floor_vol) * powf(t, 0.6f);
}

bool audio_policy_admit(AudioPolicy* p, AudioCue cue, float speed, double now, int variants, float* volume, int* variant) {
    if (cue < 0 || cue >= CUE_COUNT || variants < 1) return false;
    const CueSpec* s = &SPEC[cue];
    if (speed < s->min_speed) return false;
    if (p->ever[cue] && now - p->last_played[cue] < s->min_gap) return false;
    p->ever[cue] = true;
    p->last_played[cue] = now;
    *volume = audio_policy_loudness(cue, speed);
    *variant = (int)(p->counter[cue]++ % (unsigned)variants);
    return true;
}
