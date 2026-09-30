#include "radio.h"
#include "audio.h"
#include <emscripten.h>

/* Web build only (see src/CMakeLists.txt): replaces radio.c/radio_stream.c/radio_mp3.c entirely.
 * Those decode the AH.FM MP3 stream in C from a raw socket (WinHTTP on Windows, curl elsewhere) -
 * neither exists in a browser sandbox. Browsers already know how to stream and decode a live MP3
 * the moment a plain <audio> element's src is set (the same mechanism every "listen live" widget on
 * the web uses), no CORS headers required for playback itself, so that does all the work here; this
 * file is just the same radio.h contract wired to it instead. Confirmed against the real AH.FM
 * mirrors before writing this: loadstart -> waiting -> canplay -> playing, no CORS error. */

static bool g_available = false;
static bool g_user_paused = false;
static bool g_applied_muted = false;   /* last muted state actually applied to the <audio> element */

EM_JS(void, web_radio_create, (), {
    if (window.__carromRadio) return;
    var urls = ['https://eu.ah.fm/live', 'https://us.ah.fm/live', 'https://fr.ah.fm/live', 'https://fr2.ah.fm/live'];
    var a = new Audio();
    a.preload = 'none';
    a.volume = 0.175;   /* dropped 50% from 0.35, matches the native build (operator, 2026-09-30) */
    a.__urlIndex = 0;
    a.addEventListener('error', function () {
        /* one mirror failed - try the next, but only if the player still wants it playing */
        if (a.__wantPlaying) {
            a.__urlIndex = (a.__urlIndex + 1) % urls.length;
            a.src = urls[a.__urlIndex];
            a.play().catch(function () {});
        }
    });
    a.src = urls[0];
    window.__carromRadio = a;
});

EM_JS(void, web_radio_set_active, (int active), {
    var a = window.__carromRadio;
    if (!a) return;
    a.__wantPlaying = !!active;
    if (active) { a.play().catch(function () {}); }
    else { a.pause(); }
});

EM_JS(int, web_radio_is_playing, (), {
    var a = window.__carromRadio;
    return (a && !a.paused && a.readyState >= 3) ? 1 : 0;
});

EM_JS(void, web_radio_set_muted, (int muted), {
    var a = window.__carromRadio;
    if (a) a.muted = !!muted;
});

void radio_init(void) {
    if (g_available || !audio_ready()) return;
    web_radio_create();
    g_available = true;
    g_user_paused = false;
    web_radio_set_active(1);   /* plays by default */
}

void radio_shutdown(void) {
    if (!g_available) return;
    web_radio_set_active(0);
    g_available = false;
}

void radio_update(void) {
    /* Nothing to pump - the browser's own <audio> element handles buffering, decode and playback.
     * But unlike the native build, this <audio> element lives entirely outside raylib/miniaudio's
     * own audio graph, so audio_toggle_mute()'s SetMasterVolume() call (audio.c) never reaches it on
     * its own; mirror the mute flag onto the element here instead, once per change. */
    if (!g_available) return;
    bool muted = audio_is_muted();
    if (muted != g_applied_muted) {
        web_radio_set_muted(muted);
        g_applied_muted = muted;
    }
}

void radio_toggle(void) {
    if (!g_available) return;
    g_user_paused = !g_user_paused;
    web_radio_set_active(g_user_paused ? 0 : 1);
}

bool radio_available(void) { return g_available; }

bool radio_is_playing(void) { return g_available && !g_user_paused && web_radio_is_playing(); }
