#ifndef CARROM_FLIGHT_H
#define CARROM_FLIGHT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Flight recorder: a circular binary log of everything needed to reconstruct what happened
 * and what was on screen. One FRAME record per rendered frame (all piece and striker
 * physics state, the drawn positions of the striker, players and aim line, timers, phase,
 * layout), plus EVENT records (phase changes, plans, shots, pockets, keys, speed changes)
 * and TEXT notes.
 *
 * File layout: 64-byte header, then a ring of FLIGHT_RING_SIZE bytes. Records are
 *   [0xA5][type u8][payload length u16][checksum u16][payload]
 * and may wrap around the end of the ring; the reader re-synchronises on the sync byte and
 * verifies checksums, so the oldest partially overwritten record is skipped.
 * Little-endian, packed. Read it with the `flight_dump` tool.
 * --------------------------------------------------------------------------- */

#define FLIGHT_RING_SIZE (8u * 1024u * 1024u)
#define FLIGHT_HEADER_SIZE 64
#define FLIGHT_VERSION 1
#define FLIGHT_PIECES 19

enum { FLIGHT_REC_FRAME = 1, FLIGHT_REC_EVENT = 2, FLIGHT_REC_TEXT = 3 };

/* Event kinds (payload a,b,c,d are floats whose meaning depends on the kind) */
enum {
    FLIGHT_EV_START = 1,        /* a=seed low 32 bits, b=window w, c=window h */
    FLIGHT_EV_PHASE = 2,        /* a=from phase, b=to phase, c=turn seat */
    FLIGHT_EV_PLAN = 3,         /* a=aim angle, b=power, c=placement x, d=placement y   (text: tactic in TEXT) */
    FLIGHT_EV_SHOT_START = 4,   /* a=shot number, b=seat, c=aim angle, d=power */
    FLIGHT_EV_POCKET = 5,       /* a=piece id, b=pocket index, c=speed at capture, d=sim time */
    FLIGHT_EV_STRIKER_POCKET = 6, /* a=pocket index, b=speed at capture */
    FLIGHT_EV_SHOT_END = 7,     /* a=turn decision, b=pieces pocketed, c=striker pocketed, d=sim time */
    FLIGHT_EV_SPEED = 8,        /* a=new playback speed */
    FLIGHT_EV_PAUSE = 9,        /* a=paused (0/1) */
    FLIGHT_EV_STASH = 10,       /* a=piece id, b=pocket index, c=stash x, d=stash y */
    FLIGHT_EV_TURN = 11,        /* a=new seat, b=team */
    FLIGHT_EV_CLOSE = 12,       /* window closed / shutdown; a=phase */
    FLIGHT_EV_LAYOUT = 13,      /* a=board x, b=board y, c=board size, d=window w */
    FLIGHT_EV_SOUND = 14,       /* a=cue, b=speed, c=volume, d=variant (a sound that was actually played) */
    FLIGHT_EV_MUTE = 15         /* a=muted (0/1) */
};

#pragma pack(push, 1)
typedef struct {
    float x, y, vx, vy;
    uint8_t flags;   /* bit0 on_board (game state), bit1 pocketed (game state), bit2 physics body alive,
                        bit3 fall animation active, bit4 drawn from physics */
} FlightPiece;

typedef struct {
    double wall;               /* seconds since program start (wall clock) */
    uint64_t frame;
    float sim_time, playback_speed, alpha, frame_dt;
    float placement_timer, thinking_timer, aim_timer;
    uint8_t phase, turn_seat, flags, n_falling;   /* flags: bit0 paused, bit1 aim line shown, bit2 striker pocketed (physics),
                                                     bit3 striker on baseline (game), bit4 striker drawn */
    float striker_pos[2], striker_vel[2], striker_vis[2];
    float figures[4];          /* baseline coordinate of the N, E, S, W players as drawn */
    float aim_angle, aim_power, aim_line[4];   /* aim plan; drawn line start x,y end x,y (world units) */
    float layout[5];           /* board x, board y, board size (px), window w, h */
    uint16_t score_white, score_black;
    FlightPiece piece[FLIGHT_PIECES];
} FlightFrame;

typedef struct {
    double wall;
    float sim_time;
    uint16_t kind;
    float a, b, c, d;
} FlightEvent;
#pragma pack(pop)

typedef struct FlightRecorder FlightRecorder;

FlightRecorder* flight_open(const char* path, uint64_t seed);
void flight_close(FlightRecorder* fr);
void flight_flush(FlightRecorder* fr);
void flight_write_frame(FlightRecorder* fr, const FlightFrame* frame);
void flight_write_event(FlightRecorder* fr, double wall, float sim_time, uint16_t kind, float a, float b, float c, float d);
void flight_write_text(FlightRecorder* fr, double wall, const char* text);

/* Reading. The visitor gets records oldest first. Returns the number of records visited (or -1 on error). */
typedef void (*FlightVisitor)(void* user, int type, const void* payload, size_t len);
int flight_read(const char* path, FlightVisitor visitor, void* user, uint64_t* out_total_written, uint64_t* out_seed);
const char* flight_event_name(uint16_t kind);
const char* flight_phase_name(int phase);

#ifdef __cplusplus
}
#endif

#endif /* CARROM_FLIGHT_H */
