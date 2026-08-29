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

/* Flush any buffered data to disk */
void trace_flush(TraceWriter* writer);

/* Replay/Validation */
bool trace_validate_determinism(const char* trace1, const char* trace2);

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