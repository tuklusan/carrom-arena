#ifndef CARROM_EVENTS_H
#define CARROM_EVENTS_H

#include "types.h"
#include "telemetry/trace.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Game Event Emission
 * --------------------------------------------------------------------------- */


// Event to JSON string (for trace)
char* event_to_json(const GameEvent* evt, char* buffer, size_t size);


#ifdef __cplusplus
}
#endif

#endif // CARROM_EVENTS_H