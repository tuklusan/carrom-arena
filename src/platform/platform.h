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

/* Get current time as microseconds since epoch (for seeding) */
uint64_t platform_time_us(void);

/* File I/O */
typedef struct {
    void* handle;
    bool writing;
} PlatformFile;

PlatformFile* platform_fopen(const char* path, const char* mode);
size_t platform_fwrite(const void* ptr, size_t size, size_t count, PlatformFile* file);
size_t platform_fread(void* ptr, size_t size, size_t count, PlatformFile* file);
int platform_fclose(PlatformFile* file);
int platform_fprintf(PlatformFile* file, const char* format, ...) __attribute__((format(printf, 2, 3)));
int platform_fflush(PlatformFile* file);

/* Directory operations */
bool platform_mkdir(const char* path);
bool platform_path_exists(const char* path);

/* Get executable path */
bool platform_get_executable_path(char* buffer, size_t size);

/* Build ID (from CMake) */
extern const char* PLATFORM_BUILD_ID;

#ifdef __cplusplus
}
#endif

#endif // CARROM_PLATFORM_H