#include "flight.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIGHT_MAGIC "CARF"
#define REC_SYNC 0xA5
#define REC_HEADER 6   /* sync, type, len(2), checksum(2) */

struct FlightRecorder {
    FILE* fp;
    uint64_t total;    /* bytes of records written so far (monotonic) */
    uint64_t seed;
    uint32_t since_flush;
};

#pragma pack(push, 1)
typedef struct {
    char magic[4];
    uint32_t version;
    uint32_t ring_size;
    uint32_t frame_size;
    uint64_t total_written;
    uint64_t seed;
    uint8_t reserved[32];
} FileHeader;
#pragma pack(pop)

static uint16_t checksum16(const uint8_t* p, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; i++) { a = (a + p[i]) % 251u; b = (b + a) % 251u; }
    return (uint16_t)((b << 8) | a);
}

static void write_header(FlightRecorder* fr) {
    FileHeader h;
    memset(&h, 0, sizeof(h));
    memcpy(h.magic, FLIGHT_MAGIC, 4);
    h.version = FLIGHT_VERSION;
    h.ring_size = FLIGHT_RING_SIZE;
    h.frame_size = (uint32_t)sizeof(FlightFrame);
    h.total_written = fr->total;
    h.seed = fr->seed;
    fseek(fr->fp, 0, SEEK_SET);
    fwrite(&h, sizeof(h), 1, fr->fp);
}

FlightRecorder* flight_open(const char* path, uint64_t seed) {
    FlightRecorder* fr = calloc(1, sizeof(FlightRecorder));
    if (!fr) return NULL;
    fr->fp = fopen(path, "wb+");
    if (!fr->fp) { free(fr); return NULL; }
    fr->seed = seed;
    write_header(fr);
    fflush(fr->fp);
    return fr;
}

void flight_flush(FlightRecorder* fr) {
    if (!fr || !fr->fp) return;
    write_header(fr);
    fflush(fr->fp);
    fr->since_flush = 0;
}

void flight_close(FlightRecorder* fr) {
    if (!fr) return;
    if (fr->fp) {
        write_header(fr);
        fclose(fr->fp);
    }
    free(fr);
}

/* Write raw bytes into the ring at the current logical position, wrapping at the end */
static void ring_write(FlightRecorder* fr, const uint8_t* data, size_t n) {
    while (n > 0) {
        size_t off = (size_t)(fr->total % FLIGHT_RING_SIZE);
        size_t room = FLIGHT_RING_SIZE - off;
        size_t chunk = (n < room) ? n : room;
        fseek(fr->fp, (long)(FLIGHT_HEADER_SIZE + off), SEEK_SET);
        fwrite(data, 1, chunk, fr->fp);
        fr->total += chunk;
        data += chunk;
        n -= chunk;
    }
}

static void write_record(FlightRecorder* fr, uint8_t type, const void* payload, size_t len, bool flush_now) {
    if (!fr || !fr->fp || len > 0xFFFF) return;
    uint8_t hdr[REC_HEADER];
    uint16_t chk = checksum16((const uint8_t*)payload, len);
    hdr[0] = REC_SYNC;
    hdr[1] = type;
    hdr[2] = (uint8_t)(len & 0xFF);
    hdr[3] = (uint8_t)(len >> 8);
    hdr[4] = (uint8_t)(chk & 0xFF);
    hdr[5] = (uint8_t)(chk >> 8);
    ring_write(fr, hdr, REC_HEADER);
    ring_write(fr, (const uint8_t*)payload, len);
    fr->since_flush++;
    if (flush_now || fr->since_flush >= 8) flight_flush(fr);
}

void flight_write_frame(FlightRecorder* fr, const FlightFrame* frame) {
    write_record(fr, FLIGHT_REC_FRAME, frame, sizeof(*frame), false);
}

void flight_write_event(FlightRecorder* fr, double wall, float sim_time, uint16_t kind, float a, float b, float c, float d) {
    FlightEvent ev = { wall, sim_time, kind, a, b, c, d };
    write_record(fr, FLIGHT_REC_EVENT, &ev, sizeof(ev), true);
}

void flight_write_text(FlightRecorder* fr, double wall, const char* text) {
    char buf[300];
    memcpy(buf, &wall, sizeof(wall));
    size_t n = strlen(text);
    if (n > sizeof(buf) - sizeof(wall) - 1) n = sizeof(buf) - sizeof(wall) - 1;
    memcpy(buf + sizeof(wall), text, n);
    buf[sizeof(wall) + n] = '\0';
    write_record(fr, FLIGHT_REC_TEXT, buf, sizeof(wall) + n + 1, true);
}

/* -----------------------------------------------------------------------------
 * Reader
 * --------------------------------------------------------------------------- */
static bool header_ok(const uint8_t* p, size_t avail, size_t* len_out) {
    if (avail < REC_HEADER || p[0] != REC_SYNC) return false;
    if (p[1] < 1 || p[1] > 3) return false;
    size_t len = (size_t)p[2] | ((size_t)p[3] << 8);
    if (REC_HEADER + len > avail) return false;
    uint16_t chk = (uint16_t)(p[4] | (p[5] << 8));
    if (checksum16(p + REC_HEADER, len) != chk) return false;
    *len_out = len;
    return true;
}

int flight_read(const char* path, FlightVisitor visitor, void* user, uint64_t* out_total_written, uint64_t* out_seed) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;
    FileHeader h;
    if (fread(&h, sizeof(h), 1, fp) != 1 || memcmp(h.magic, FLIGHT_MAGIC, 4) != 0 || h.ring_size != FLIGHT_RING_SIZE) {
        fclose(fp);
        return -1;
    }
    if (out_total_written) *out_total_written = h.total_written;
    if (out_seed) *out_seed = h.seed;

    size_t valid = (h.total_written < FLIGHT_RING_SIZE) ? (size_t)h.total_written : FLIGHT_RING_SIZE;
    uint8_t* ring = malloc(FLIGHT_RING_SIZE);
    uint8_t* lin = malloc(FLIGHT_RING_SIZE);
    if (!ring || !lin) { free(ring); free(lin); fclose(fp); return -1; }
    fseek(fp, FLIGHT_HEADER_SIZE, SEEK_SET);
    size_t got = fread(ring, 1, FLIGHT_RING_SIZE, fp);
    fclose(fp);
    if (got < valid) valid = got;

    /* chronological order: oldest byte first */
    size_t n = 0;
    if (h.total_written > FLIGHT_RING_SIZE) {
        size_t start = (size_t)(h.total_written % FLIGHT_RING_SIZE);
        memcpy(lin, ring + start, FLIGHT_RING_SIZE - start);
        memcpy(lin + (FLIGHT_RING_SIZE - start), ring, start);
        n = FLIGHT_RING_SIZE;
    } else {
        memcpy(lin, ring, valid);
        n = valid;
    }

    /* find the first record boundary: a valid header whose successor chain also parses to the end */
    size_t pos = 0;
    if (h.total_written > FLIGHT_RING_SIZE) {
        bool found = false;
        for (; pos + REC_HEADER <= n && !found; pos++) {
            size_t len;
            if (!header_ok(lin + pos, n - pos, &len)) continue;
            size_t nxt = pos + REC_HEADER + len, hops = 0;
            bool chain = true;
            while (nxt < n && hops < 4) {
                size_t l2;
                if (!header_ok(lin + nxt, n - nxt, &l2)) { chain = (n - nxt) < REC_HEADER || (lin[nxt] == REC_SYNC); break; }
                nxt += REC_HEADER + l2;
                hops++;
            }
            if (chain) { found = true; pos--; }
        }
        if (!found) { free(ring); free(lin); return 0; }
    }

    int count = 0;
    while (pos + REC_HEADER <= n) {
        size_t len;
        if (!header_ok(lin + pos, n - pos, &len)) break;   /* partial last record */
        visitor(user, lin[pos + 1], lin + pos + REC_HEADER, len);
        pos += REC_HEADER + len;
        count++;
    }
    free(ring);
    free(lin);
    return count;
}

const char* flight_event_name(uint16_t kind) {
    switch (kind) {
        case FLIGHT_EV_START: return "START";
        case FLIGHT_EV_PHASE: return "PHASE";
        case FLIGHT_EV_PLAN: return "PLAN";
        case FLIGHT_EV_SHOT_START: return "SHOT_START";
        case FLIGHT_EV_POCKET: return "POCKET";
        case FLIGHT_EV_STRIKER_POCKET: return "STRIKER_POCKET";
        case FLIGHT_EV_SHOT_END: return "SHOT_END";
        case FLIGHT_EV_SPEED: return "SPEED";
        case FLIGHT_EV_PAUSE: return "PAUSE";
        case FLIGHT_EV_STASH: return "STASH";
        case FLIGHT_EV_TURN: return "TURN";
        case FLIGHT_EV_CLOSE: return "CLOSE";
        case FLIGHT_EV_LAYOUT: return "LAYOUT";
        case FLIGHT_EV_SOUND: return "SOUND";
        case FLIGHT_EV_MUTE: return "MUTE";
        default: return "?";
    }
}

const char* flight_phase_name(int phase) {
    static const char* names[] = { "IDLE", "THINKING", "PLACEMENT", "AIM_PREVIEW", "AIMING", "SHOT_EXECUTION",
                                   "SETTLING", "RESOLVING", "BOARD_OVER", "GAME_OVER", "MATCH_OVER" };
    return (phase >= 0 && phase < 11) ? names[phase] : "?";
}
