#include "radio_stream.h"
#include "platform/platform.h"
#include "minimp3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#else
#include <pthread.h>
#endif

#define RS_RING_FRAMES  (48000 * 4)     /* four seconds of stereo frames */
#define RS_IN_SIZE      32768
#define RS_MAX_URLS     4
#define RS_URL_LEN      256

/* -----------------------------------------------------------------------------
 * Threads and locks
 * --------------------------------------------------------------------------- */
#if defined(_WIN32)
typedef HANDLE RsThread;
typedef CRITICAL_SECTION RsLock;
#define RS_THREAD_RET DWORD WINAPI
static void rs_lock_init(RsLock* l) { InitializeCriticalSection(l); }
static void rs_lock_free(RsLock* l) { DeleteCriticalSection(l); }
static void rs_lock(RsLock* l) { EnterCriticalSection(l); }
static void rs_unlock(RsLock* l) { LeaveCriticalSection(l); }
#else
typedef pthread_t RsThread;
typedef pthread_mutex_t RsLock;
#define RS_THREAD_RET void*
static void rs_lock_init(RsLock* l) { pthread_mutex_init(l, NULL); }
static void rs_lock_free(RsLock* l) { pthread_mutex_destroy(l); }
static void rs_lock(RsLock* l) { pthread_mutex_lock(l); }
static void rs_unlock(RsLock* l) { pthread_mutex_unlock(l); }
#endif

struct RadioStream {
    char urls[RS_MAX_URLS][RS_URL_LEN];
    int url_count, url_index;
    RsSource src;
    int retry_ms;

    RsThread thread;
    bool thread_started;
    RsLock lock;
    volatile int quit;
    volatile int active;
    volatile int status;
    volatile int hz;

    int16_t* ring;            /* stereo frames, interleaved */
    size_t rd, count;         /* read index and number of frames held (guarded by lock) */
};

/* -----------------------------------------------------------------------------
 * The platform's HTTP(S) client
 * --------------------------------------------------------------------------- */
#if defined(_WIN32)
typedef struct { HINTERNET session, connect, request; } WinNet;

static void win_free(WinNet* w) {
    if (w->request) WinHttpCloseHandle(w->request);
    if (w->connect) WinHttpCloseHandle(w->connect);
    if (w->session) WinHttpCloseHandle(w->session);
    free(w);
}

static void* net_open(void* user, const char* url) {
    (void)user;
    wchar_t wurl[RS_URL_LEN * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, (int)(sizeof(wurl) / sizeof(wurl[0]))) == 0) return NULL;
    wchar_t host[256], path[512];
    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;  uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;   uc.dwUrlPathLength = 512;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) return NULL;

    WinNet* w = calloc(1, sizeof(WinNet));
    if (!w) return NULL;
    w->session = WinHttpOpen(L"CarromArena/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!w->session) { win_free(w); return NULL; }
    WinHttpSetTimeouts(w->session, 8000, 8000, 8000, 10000);
    w->connect = WinHttpConnect(w->session, host, uc.nPort, 0);
    if (!w->connect) { win_free(w); return NULL; }
    w->request = WinHttpOpenRequest(w->connect, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                    uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!w->request) { win_free(w); return NULL; }
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpSendRequest(w->request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(w->request, NULL) ||
        !WinHttpQueryHeaders(w->request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                             &status, &size, WINHTTP_NO_HEADER_INDEX) ||
        status != 200) {
        win_free(w);
        return NULL;
    }
    return w;
}

static int net_read(void* handle, uint8_t* buf, int max) {
    WinNet* w = (WinNet*)handle;
    DWORD got = 0;
    if (!WinHttpReadData(w->request, buf, (DWORD)max, &got)) return -1;
    return (int)got;
}

static void net_close(void* handle) { win_free((WinNet*)handle); }

#else
/* Development hosts (Linux, macOS): the system's curl does the TLS. */
static void* net_open(void* user, const char* url) {
    (void)user;
    char cmd[RS_URL_LEN + 96];
    snprintf(cmd, sizeof(cmd), "curl -sSfL --connect-timeout 8 -A CarromArena/1.0 '%s' 2>/dev/null", url);
    extern FILE* popen(const char*, const char*);
    return popen(cmd, "r");
}

static int net_read(void* handle, uint8_t* buf, int max) {
    size_t n = fread(buf, 1, (size_t)max, (FILE*)handle);
    return (n == 0) ? 0 : (int)n;
}

static void net_close(void* handle) {
    extern int pclose(FILE*);
    pclose((FILE*)handle);
}
#endif

/* -----------------------------------------------------------------------------
 * Ring
 * --------------------------------------------------------------------------- */
static size_t ring_free(RadioStream* rs) {
    rs_lock(&rs->lock);
    size_t f = RS_RING_FRAMES - rs->count;
    rs_unlock(&rs->lock);
    return f;
}

static void ring_flush(RadioStream* rs) {
    rs_lock(&rs->lock);
    rs->rd = 0;
    rs->count = 0;
    rs_unlock(&rs->lock);
}

static void ring_push(RadioStream* rs, const int16_t* pcm, int frames, int channels) {
    rs_lock(&rs->lock);
    for (int i = 0; i < frames && rs->count < RS_RING_FRAMES; i++) {
        size_t w = (rs->rd + rs->count) % RS_RING_FRAMES;
        int16_t l = pcm[i * channels];
        int16_t r = (channels > 1) ? pcm[i * channels + 1] : l;
        rs->ring[w * 2] = l;
        rs->ring[w * 2 + 1] = r;
        rs->count++;
    }
    rs_unlock(&rs->lock);
}

/* -----------------------------------------------------------------------------
 * Worker
 * --------------------------------------------------------------------------- */
static void nap(RadioStream* rs, int ms) {
    while (ms > 0 && !rs->quit) {
        int step = (ms < 25) ? ms : 25;
        platform_sleep_ms((uint32_t)step);
        ms -= step;
    }
}

static RS_THREAD_RET worker_main(void* arg) {
    RadioStream* rs = (RadioStream*)arg;
    mp3dec_t dec;
    uint8_t* in = malloc(RS_IN_SIZE);
    int16_t* pcm = malloc(sizeof(int16_t) * MINIMP3_MAX_SAMPLES_PER_FRAME);
    if (!in || !pcm) { free(in); free(pcm); rs->status = RS_FAILED; return 0; }

    while (!rs->quit) {
        if (!rs->active) {
            rs->status = RS_IDLE;
            nap(rs, 50);
            continue;
        }
        rs->status = RS_CONNECTING;
        void* h = rs->src.open(rs->src.user, rs->urls[rs->url_index]);
        if (h) {
            mp3dec_init(&dec);
            int in_len = 0;
            bool got_data = false;
            ring_flush(rs);
            while (!rs->quit && rs->active) {
                int n = rs->src.read(h, in + in_len, RS_IN_SIZE - in_len);
                if (n <= 0) break;                       /* error or end of stream: reconnect */
                if (!got_data) { got_data = true; rs->status = RS_PLAYING; }
                in_len += n;
                int pos = 0;
                for (;;) {
                    mp3dec_frame_info_t info;
                    int samples = mp3dec_decode_frame(&dec, in + pos, in_len - pos, pcm, &info);
                    if (info.frame_bytes == 0) break;    /* needs more data */
                    pos += info.frame_bytes;
                    if (samples > 0 && info.channels > 0) {
                        rs->hz = info.hz;
                        while (!rs->quit && rs->active && ring_free(rs) < (size_t)samples) platform_sleep_ms(10);   /* the listener is behind */
                        ring_push(rs, pcm, samples, info.channels);
                    }
                }
                if (pos > 0 && pos < in_len) memmove(in, in + pos, (size_t)(in_len - pos));
                in_len -= pos;
                if (in_len >= RS_IN_SIZE) in_len = 0;    /* cannot happen with a valid stream; never wedge on garbage */
            }
            rs->src.close(h);
        }
        if (rs->quit) break;
        if (rs->active) {
            rs->status = RS_FAILED;
            rs->url_index = (rs->url_index + 1) % rs->url_count;   /* next mirror */
            nap(rs, rs->retry_ms);
        }
    }
    free(in);
    free(pcm);
    return 0;
}

/* -----------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */
RadioStream* rs_create(const char* const* urls, int url_count, const RsSource* source, int retry_ms) {
    if (!urls || url_count < 1) return NULL;
    RadioStream* rs = calloc(1, sizeof(RadioStream));
    if (!rs) return NULL;
    rs->ring = calloc((size_t)RS_RING_FRAMES * 2, sizeof(int16_t));
    if (!rs->ring) { free(rs); return NULL; }
    rs->url_count = (url_count > RS_MAX_URLS) ? RS_MAX_URLS : url_count;
    for (int i = 0; i < rs->url_count; i++) snprintf(rs->urls[i], RS_URL_LEN, "%s", urls[i]);
    if (source) rs->src = *source;
    else rs->src = (RsSource){ net_open, net_read, net_close, NULL };
    rs->retry_ms = (retry_ms > 0) ? retry_ms : 4000;
    rs->status = RS_IDLE;
    rs_lock_init(&rs->lock);
#if defined(_WIN32)
    rs->thread = CreateThread(NULL, 0, worker_main, rs, 0, NULL);
    rs->thread_started = (rs->thread != NULL);
#else
    rs->thread_started = (pthread_create(&rs->thread, NULL, worker_main, rs) == 0);
#endif
    if (!rs->thread_started) {
        rs_lock_free(&rs->lock);
        free(rs->ring);
        free(rs);
        return NULL;
    }
    return rs;
}

void rs_destroy(RadioStream* rs) {
    if (!rs) return;
    rs->active = 0;
    rs->quit = 1;
#if defined(_WIN32)
    WaitForSingleObject(rs->thread, 15000);   /* a blocked read gives up within the 10 s network timeout */
    CloseHandle(rs->thread);
#else
    pthread_join(rs->thread, NULL);
#endif
    rs_lock_free(&rs->lock);
    free(rs->ring);
    free(rs);
}

void rs_set_active(RadioStream* rs, bool active) {
    if (!rs) return;
    if (!active) {
        rs->active = 0;
        ring_flush(rs);
    } else {
        rs->active = 1;
    }
}

bool rs_is_active(const RadioStream* rs) { return rs && rs->active; }

RsStatus rs_status(const RadioStream* rs) {
    if (!rs || !rs->active) return RS_IDLE;
    return (RsStatus)rs->status;
}

int rs_sample_rate(const RadioStream* rs) { return rs ? rs->hz : 0; }

size_t rs_available(RadioStream* rs) {
    if (!rs) return 0;
    rs_lock(&rs->lock);
    size_t n = rs->count;
    rs_unlock(&rs->lock);
    return n;
}

size_t rs_read(RadioStream* rs, int16_t* out, size_t frames) {
    if (!rs) return 0;
    rs_lock(&rs->lock);
    size_t n = (frames < rs->count) ? frames : rs->count;
    for (size_t i = 0; i < n; i++) {
        size_t r = (rs->rd + i) % RS_RING_FRAMES;
        out[i * 2] = rs->ring[r * 2];
        out[i * 2 + 1] = rs->ring[r * 2 + 1];
    }
    rs->rd = (rs->rd + n) % RS_RING_FRAMES;
    rs->count -= n;
    rs_unlock(&rs->lock);
    return n;
}
