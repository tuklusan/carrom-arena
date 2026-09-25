#ifndef CARROM_RADIO_STREAM_H
#define CARROM_RADIO_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Internet radio, network and decoding half (no audio device, no raylib): a worker thread connects to an MP3 stream over
 * HTTP(S), decodes it and fills a ring of 16-bit stereo frames that the caller drains. The worker reconnects by itself
 * after a failure (trying the next mirror) for as long as the stream is "active". */

typedef enum {
    RS_IDLE,          /* not active: no connection is held or attempted */
    RS_CONNECTING,    /* active, connecting or reconnecting */
    RS_PLAYING,       /* connected, data is flowing */
    RS_FAILED         /* active, but the connection failed: retrying after a pause */
} RsStatus;

/* The network side, replaceable for tests. read returns bytes read, 0 at end of stream, negative on error. */
typedef struct {
    void* (*open)(void* user, const char* url);      /* NULL on failure */
    int   (*read)(void* handle, uint8_t* buf, int max);
    void  (*close)(void* handle);
    void* user;
} RsSource;

typedef struct RadioStream RadioStream;

/* urls: mirrors tried in turn (at most 4). source NULL = the platform's HTTP(S) client. retry_ms: pause after a failure. */
RadioStream* rs_create(const char* const* urls, int url_count, const RsSource* source, int retry_ms);
void rs_destroy(RadioStream* rs);

void     rs_set_active(RadioStream* rs, bool active);   /* true: connect and stream; false: disconnect and drop the buffer */
bool     rs_is_active(const RadioStream* rs);
RsStatus rs_status(const RadioStream* rs);
int      rs_sample_rate(const RadioStream* rs);         /* Hz of the decoded audio, 0 until the first frame */
size_t   rs_available(RadioStream* rs);                 /* stereo frames waiting */
size_t   rs_read(RadioStream* rs, int16_t* out, size_t frames);   /* copies up to `frames` stereo frames, never blocks */

#ifdef __cplusplus
}
#endif

#endif /* CARROM_RADIO_STREAM_H */
