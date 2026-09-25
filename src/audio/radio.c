#include "radio.h"
#include "radio_stream.h"
#include "audio.h"
#define __USE_MINGW_ANSI_STDIO 1
#include <raylib.h>
#include <string.h>

#define RADIO_FRAMES_PER_UPDATE 4096   /* raylib streams are fed one buffer at a time */
#define RADIO_PREBUFFER_FRAMES  (RADIO_FRAMES_PER_UPDATE * 6)   /* about half a second before the first sound */
#define RADIO_VOLUME 0.35f

static RadioStream* g_rs;
static AudioStream g_stream;
static bool g_stream_ready;
static int g_stream_hz;
static bool g_user_paused;
static bool g_audible;          /* the device is playing radio audio right now */
static bool g_prebuffering;

/* AH.FM's MP3 mirrors (the plain-HTTP addresses only redirect to these) */
static const char* const RADIO_URLS[] = {
    "https://eu.ah.fm/live",
    "https://us.ah.fm/live",
    "https://fr.ah.fm/live",
    "https://fr2.ah.fm/live",
};

static void stream_close(void) {
    if (g_stream_ready) {
        StopAudioStream(g_stream);
        UnloadAudioStream(g_stream);
        g_stream_ready = false;
    }
    g_audible = false;
    g_prebuffering = true;
}

void radio_init(void) {
    if (g_rs || !audio_ready()) return;
    g_rs = rs_create(RADIO_URLS, (int)(sizeof(RADIO_URLS) / sizeof(RADIO_URLS[0])), NULL, 4000);
    g_user_paused = false;
    g_prebuffering = true;
    if (g_rs) rs_set_active(g_rs, true);   /* plays by default */
}

void radio_shutdown(void) {
    if (!g_rs) return;
    rs_destroy(g_rs);
    g_rs = NULL;
    stream_close();
}

bool radio_available(void) { return g_rs != NULL; }

bool radio_is_playing(void) { return g_rs != NULL && g_audible; }

void radio_toggle(void) {
    if (!g_rs) return;
    if (!g_user_paused && g_audible) {
        /* the player pauses: stay paused until the player presses play */
        g_user_paused = true;
        rs_set_active(g_rs, false);
        if (g_stream_ready) PauseAudioStream(g_stream);
        g_audible = false;
        g_prebuffering = true;
    } else {
        /* play pressed (or pressed while the stream is down: the player wants it, keep trying) */
        g_user_paused = false;
        g_prebuffering = true;
        rs_set_active(g_rs, true);
    }
}

void radio_update(void) {
    if (!g_rs || !audio_ready()) return;

    if (g_user_paused) {
        g_audible = false;
        return;
    }

    /* the worker reconnects by itself after a failure; audio resumes when data arrives again */
    RsStatus st = rs_status(g_rs);
    int hz = rs_sample_rate(g_rs);
    if (st != RS_PLAYING || hz <= 0) {
        if (g_stream_ready && g_audible) { PauseAudioStream(g_stream); g_prebuffering = true; }
        g_audible = false;
        return;
    }

    if (g_stream_ready && hz != g_stream_hz) stream_close();
    if (!g_stream_ready) {
        SetAudioStreamBufferSizeDefault(RADIO_FRAMES_PER_UPDATE);
        g_stream = LoadAudioStream((unsigned int)hz, 16, 2);
        if (!IsAudioStreamValid(g_stream)) return;
        SetAudioStreamVolume(g_stream, RADIO_VOLUME);
        g_stream_hz = hz;
        g_stream_ready = true;
        g_prebuffering = true;
    }

    if (g_prebuffering) {
        if (rs_available(g_rs) < RADIO_PREBUFFER_FRAMES) return;
        g_prebuffering = false;
        if (IsAudioStreamPlaying(g_stream)) ResumeAudioStream(g_stream);
        else PlayAudioStream(g_stream);
        g_audible = true;
    }

    while (IsAudioStreamProcessed(g_stream)) {
        static short chunk[RADIO_FRAMES_PER_UPDATE * 2];
        size_t got = rs_read(g_rs, (int16_t*)chunk, RADIO_FRAMES_PER_UPDATE);
        if (got < RADIO_FRAMES_PER_UPDATE) memset(chunk + got * 2, 0, (RADIO_FRAMES_PER_UPDATE - got) * 2 * sizeof(short));   /* underrun: a gap, never a stale loop */
        UpdateAudioStream(g_stream, chunk, RADIO_FRAMES_PER_UPDATE);
        if (got == 0) break;
    }
    g_audible = true;
}
