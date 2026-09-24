#ifndef CARROM_PLATFORM_H
#define CARROM_PLATFORM_H

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

/* File I/O */
typedef struct {
    void* handle;
    bool writing;
} PlatformFile;

PlatformFile* platform_fopen(const char* path, const char* mode);
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