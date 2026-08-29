#ifndef CARROM_EVENTS_H
#define CARROM_EVENTS_H

#include "types.h"
#include "platform/platform.h"
#include "telemetry/trace.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Game Event Emission
 * --------------------------------------------------------------------------- */

// Event logging (human-readable)
void events_log(const GameEvent* evt, PlatformFile* log_file);

// Event to JSON string (for trace)
char* event_to_json(const GameEvent* evt, char* buffer, size_t size);

// Emit event to trace writer
void events_emit_trace(const GameEvent* evt, TraceWriter* trace);

#ifdef __cplusplus
}
#endif

#endif // CARROM_EVENTS_H