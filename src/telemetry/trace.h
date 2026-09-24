#ifndef CARROM_TRACE_H
#define CARROM_TRACE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Telemetry Trace Writer (JSONL + Human-readable log) - Circular Buffer
 * Appendix A.8
 * Max file size: 8 MiB (8 * 1024 * 1024 bytes) with 8-byte index header
 * --------------------------------------------------------------------------- */

#define TRACE_MAX_SIZE (8 * 1024 * 1024)  /* 8 MiB data area */
#define TRACE_INDEX_SIZE 8                 /* 64-bit write position offset */
#define TRACE_TOTAL_SIZE (TRACE_MAX_SIZE + TRACE_INDEX_SIZE)

typedef struct TraceWriter TraceWriter;

/* Open trace files for a run. 
 * path: base path for JSONL trace (e.g., "traces/seed_12345.jsonl")
 * log_dir: directory for human-readable mirror log (e.g., "logs/")
 * verbose: also write human-readable .log mirror
 * seed: run seed for naming log file
 */
TraceWriter* trace_open(const char* path, const char* log_dir, bool verbose, uint64_t seed);
void trace_close(TraceWriter* writer);

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