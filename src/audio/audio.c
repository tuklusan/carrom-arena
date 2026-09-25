#include "audio.h"
#include "audio_assets.h"
#define __USE_MINGW_ANSI_STDIO 1
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define ALIASES_PER_SOUND 4   /* overlapping plays of the same sample */
#define MAX_VARIANTS 4

typedef struct {
    Sound base;
    Sound alias[ALIASES_PER_SOUND];
    int next_alias;
    bool loaded;
} Voice;

static const struct { const char* prefix; int variants; } CUE_FILES[CUE_COUNT] = {
    [CUE_FLICK]          = { "flick_", 3 },
    [CUE_STRIKER_COIN]   = { "striker_coin_", 4 },
    [CUE_COIN_COIN]      = { "coin_coin_", 4 },
    [CUE_STRIKER_WALL]   = { "striker_wall_", 3 },
    [CUE_COIN_WALL]      = { "coin_wall_", 3 },
    [CUE_STRIKER_POCKET] = { "striker_pocket_", 3 },
    [CUE_COIN_POCKET]    = { "coin_pocket_", 4 },
    [CUE_QUEEN]          = { "queen_", 1 },
    [CUE_FOUL]           = { "foul_", 1 },
    [CUE_BOARD_WON]      = { "board_won_", 1 },
};

static Voice g_voice[CUE_COUNT][MAX_VARIANTS];
static int g_variants[CUE_COUNT];
static bool g_ready = false;
static bool g_muted = false;

static const AudioAsset* find_asset(const char* name) {
    for (int i = 0; i < audio_asset_count; i++) {
        if (strcmp(audio_assets[i].name, name) == 0) return &audio_assets[i];
    }
    return NULL;
}

bool audio_init(void) {
    if (g_ready) return true;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return false;
    int loaded = 0;
    for (int c = 0; c < CUE_COUNT; c++) {
        g_variants[c] = 0;
        for (int v = 0; v < CUE_FILES[c].variants && v < MAX_VARIANTS; v++) {
            char name[64];
            snprintf(name, sizeof(name), "%s%d", CUE_FILES[c].prefix, v + 1);
            const AudioAsset* a = find_asset(name);
            if (!a) continue;
            Wave w = LoadWaveFromMemory(".ogg", a->data, a->size);
            if (w.frameCount == 0) continue;
            Voice* vo = &g_voice[c][g_variants[c]];
            vo->base = LoadSoundFromWave(w);
            UnloadWave(w);
            for (int k = 0; k < ALIASES_PER_SOUND; k++) vo->alias[k] = LoadSoundAlias(vo->base);
            vo->loaded = true;
            g_variants[c]++;
            loaded++;
        }
    }
    g_ready = loaded > 0;
    if (!g_ready) CloseAudioDevice();
    return g_ready;
}

void audio_shutdown(void) {
    if (!g_ready) return;
    for (int c = 0; c < CUE_COUNT; c++) {
        for (int v = 0; v < g_variants[c]; v++) {
            Voice* vo = &g_voice[c][v];
            for (int k = 0; k < ALIASES_PER_SOUND; k++) UnloadSoundAlias(vo->alias[k]);
            UnloadSound(vo->base);
            vo->loaded = false;
        }
        g_variants[c] = 0;
    }
    CloseAudioDevice();
    g_ready = false;
}

bool audio_ready(void) { return g_ready; }

int audio_variant_count(AudioCue cue) {
    if (cue < 0 || cue >= CUE_COUNT) return 0;
    return g_ready ? g_variants[cue] : CUE_FILES[cue].variants;   /* the policy still rotates when silent */
}

void audio_play(AudioCue cue, float volume, int variant) {
    if (!g_ready || cue < 0 || cue >= CUE_COUNT || g_variants[cue] == 0) return;
    Voice* vo = &g_voice[cue][variant % g_variants[cue]];
    if (!vo->loaded) return;
    Sound s = vo->alias[vo->next_alias];
    vo->next_alias = (vo->next_alias + 1) % ALIASES_PER_SOUND;
    SetSoundVolume(s, volume);
    SetSoundPitch(s, 1.0f);   /* always natural pitch, whatever the game speed */
    PlaySound(s);
}

void audio_toggle_mute(void) {
    g_muted = !g_muted;
    if (g_ready) SetMasterVolume(g_muted ? 0.0f : 1.0f);
}

bool audio_is_muted(void) { return g_muted; }
