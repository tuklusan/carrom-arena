#ifndef CARROM_TRACE_H
#define CARROM_TRACE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Telemetry Trace Writer (JSONL + Human-readable log) - Circular Buffer
 * Appendix A.8
 * Max file size: 8 MiB (8 * 1024 * 1024 bytes) with a 16-byte index header.
 *
 * ONE FILE, REUSED FOR EVERY RUN (2026-09-28): the trace is opened at a single, fixed, obvious path
 * (traces/trace.jsonl - see app_setup_trace() in app/app.c), created only if it does not already exist; every
 * later run reopens and continues the SAME ring rather than starting a fresh file. Because many runs' records
 * now share one buffer, trace_open() writes a RUN_START marker (trace_write_run_start()) the instant it opens -
 * a timestamp, the build id and the process id - so a reader can find exactly where the latest run's own data
 * begins by scanning for the last RUN_START record, instead of guessing from file position alone.
 *
 * The index header is now 16 bytes: the first 8 are the write position (as before); the second 8 are an
 * explicit wrapped flag (0 or 1), persisted on every write. A reader used to infer "has this ring actually
 * wrapped" from the on-disk FILE SIZE - but trace_init_file() zero-fills the file to its full size the instant
 * it is created, so that size is always the full 8 MiB+header from the first moment the file exists, regardless
 * of how much real data has ever been written. That made the inference always true, even for a file that had
 * only ever written a few hundred KB - harmless in practice (the unwritten tail is zero bytes, which can never
 * look like a JSONL record start), but wrong, wasteful (every read reconstructs and scans a full 8 MiB logical
 * buffer even when almost none of it is real), and exactly the kind of accidental-safety-net bug this project
 * has hit before in this same reader. The flag is now real, persisted state instead of a guess. */

#define TRACE_MAX_SIZE (8 * 1024 * 1024)  /* 8 MiB data area */
#define TRACE_INDEX_SIZE 16                /* 64-bit write position offset + 64-bit wrapped flag */
#define TRACE_TOTAL_SIZE (TRACE_MAX_SIZE + TRACE_INDEX_SIZE)

typedef struct TraceWriter TraceWriter;

/* Open THE trace file for a run - always the one fixed path (traces/trace.jsonl - see app_setup_trace() in
 * app/app.c), created if it does not exist, reused and continued otherwise. `seed` is this run's own seed, kept
 * only to stamp records (pre_state_hash, RUN_START) - it plays no part in the path any more. */
TraceWriter* trace_open(const char* path, uint64_t seed);
void trace_close(TraceWriter* writer);

/* Marks where a run's own data begins in the (now shared, cross-run) ring: timestamp, build id, process id and the
 * run's seed. Called once by trace_open() right after a successful open, so every run - whether it created the file
 * or reopened an existing one - leaves a marker a reader can search for. */
void trace_write_run_start(TraceWriter* writer);

/* Install with platform_diag_set_sink(trace_diag_sink, writer): folds platform_diag_logf() output (raylib's own
 * log, per-frame phase notes - everything that used to go to a separate debug_<seed>.log) into this same trace file
 * as LOG records, instead of a second file. */
void trace_diag_sink(const char* line, void* userdata);

/* Folds in what the separate binary flight recorder used to call an EVENT record (phase/turn/speed/pause/layout/
 * sound/pocket/shot boundaries...) as JSONL here instead. `kind` is the caller's own small integer enum (see
 * app.c's APP_EV_*); `kind_name` makes the record self-describing without that enum in hand. */
void trace_write_app_event(TraceWriter* writer, int kind, const char* kind_name, float sim_time,
                           float a, float b, float c, float d);

/* Write operations - automatically handles ring buffer wrapping */
void trace_write_shot_start(TraceWriter* writer, const MatchState* match, const GameState* game, 
                            uint64_t shot_number, Seat seat, const ShotPlan* plan);
void trace_write_shot_end(TraceWriter* writer, const ShotResult* result, const RulesOutcome* outcome);
void trace_write_event(TraceWriter* writer, const GameEvent* evt);

/* Diagnostic: write per-frame physics state (striker velocity) */
void trace_write_physics_state(TraceWriter* writer, uint64_t frame, uint64_t shot_number, 
                               float sim_time, const Vec2* striker_vel, const Vec2* striker_pos,
                               const char* phase);

/* Immediate record when physics pockets a piece */
void trace_write_pocket(TraceWriter* writer, uint64_t shot_number, uint8_t piece_id, int color,
                        uint8_t pocket_index, float sim_time);

/* Mid-shot snapshot. pos/vel/alive are indexed by piece id (MAX_PIECES entries); pocketed[] lists
 * pocketed ids so far. SHOT_PROGRESS lists only pieces moving faster than 0.02; SHOT_INTERRUPTED
 * (interrupted=true) lists every live piece and flushes. */
void trace_write_shot_snapshot(TraceWriter* writer, bool interrupted, uint64_t shot_number, float sim_time,
                               const char* phase, const Vec2* striker_pos, const Vec2* striker_vel,
                               const Vec2* pos, const Vec2* vel, const bool* alive,
                               const uint8_t* pocketed, int pocketed_count);

/* Flush any buffered data to disk */
void trace_flush(TraceWriter* writer);

/* Record a pocket near miss event */
void trace_write_pocket_near_miss(TraceWriter* writer, uint8_t piece_id, uint8_t pocket_index, float distance, float speed);

/* Replay/Validation */

/* Utility: read last N complete JSONL records from a circular trace file */
typedef struct {
    char** lines;
    size_t count;
    size_t capacity;
} TraceRecordArray;

TraceRecordArray trace_read_last_records(const char* path, size_t max_records);
void trace_record_array_free(TraceRecordArray* arr);

#ifdef __cplusplus
}
#endif

#endif // CARROM_TRACE_H