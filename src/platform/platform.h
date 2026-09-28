#ifndef CARROM_PLATFORM_H
#define CARROM_PLATFORM_H

#include <stdio.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Platform Abstraction Layer
 * Minimal cross-platform utilities for timing, file I/O, RNG seeding
 * --------------------------------------------------------------------------- */

/* High-resolution time in seconds */
double platform_time_now(void);

/* Sleep for milliseconds (best effort) */
void platform_sleep_ms(uint32_t ms);

/* Yield CPU time slice to OS scheduler (sched_yield on POSIX, SwitchToThread on Windows) */
void platform_yield(void);

/* Get current time as microseconds since epoch (for seeding) */
uint64_t platform_time_us(void);

/* Current process id (getpid on POSIX, GetCurrentProcessId on Windows). Used, together with the build id and a wall-clock timestamp, to mark where each run's data begins in a trace file that is now reused across every run rather than recreated per run. */
uint64_t platform_get_pid(void);

/* File I/O */
typedef struct {
    void* handle;
    bool writing;
} PlatformFile;

PlatformFile* platform_fopen(const char* path, const char* mode);

/* fopen for files the game CREATES (the trace, markers): owner-only permissions (0600) on POSIX instead of the umask
 * default of 0666. `mode` is a normal fopen mode ("w", "w+b", ...). */
FILE* platform_fopen_private(const char* path, const char* mode);

/* Debug log sink (2026-09-28): all debugging output - raylib's own log, per-frame phase notes, everything that used
 * to go to its own debug_<seed>.log file - is now handed to a caller-supplied sink instead of a file this layer
 * owns, so it can be folded into the one shared trace file as LOG records (see trace_diag_sink() in telemetry/trace.h)
 * without platform.c needing to know anything about the trace format. Lines logged before a sink is registered, or
 * after it is cleared, are dropped - the same "no file open yet" behaviour this always had, just generalised. */
typedef void (*PlatformDiagSink)(const char* line, void* userdata);
void platform_diag_set_sink(PlatformDiagSink sink, void* userdata);
void platform_diag_logf(const char* fmt, ...);

/* A fatal start-up problem the player must be told about: a message box on Windows (there is no console), stderr elsewhere.
 * Also written to the debug log. */
void platform_fatal(const char* message);

/* (platform_prune_old_files was removed 2026-09-28: it kept the newest N of each per-run-named file - flight_,
 * seed_, debug_ - now that the trace is a single fixed-name file reused forever, there is nothing left to prune.) */
int platform_fclose(PlatformFile* file);
int platform_fprintf(PlatformFile* file, const char* format, ...) __attribute__((format(printf, 2, 3)));
int platform_fflush(PlatformFile* file);

/* Directory operations */
bool platform_mkdir(const char* path);

/* Get executable path */

/* Build ID (from CMake) */
extern const char* PLATFORM_BUILD_ID;

#ifdef __cplusplus
}
#endif

#endif // CARROM_PLATFORM_H