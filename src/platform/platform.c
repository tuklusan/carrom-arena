#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#if !defined(_WIN32)
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#else
#include <windows.h>
#include <direct.h>
#include <io.h>
#endif

/* Explicit declarations for functions that may not be declared with feature macros */
#if !defined(_WIN32)
extern int nanosleep(const struct timespec*, struct timespec*);
extern ssize_t readlink(const char*, char*, size_t);
#endif

/* Build ID injected by CMake */
const char* PLATFORM_BUILD_ID = BUILD_ID;

#if defined(_WIN32)
    #define mkdir_p(path) mkdir(path)
#else
    #define mkdir_p(path) mkdir(path, 0755)
#endif

double platform_time_now(void) {
#if defined(_WIN32)
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
#endif
}

void platform_sleep_ms(uint32_t ms) {
#if defined(_WIN32)
    Sleep(ms);
#else
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000 };
    nanosleep(&ts, NULL);
#endif
}

uint64_t platform_time_us(void) {
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t tt = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    // Convert from 100-nanosecond intervals since 1601 to microseconds since 1970
    return (tt / 10) - 11644473600000000ULL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
#endif
}

PlatformFile* platform_fopen(const char* path, const char* mode) {
    FILE* f = fopen(path, mode);
    if (!f) return NULL;
    
    PlatformFile* pf = malloc(sizeof(PlatformFile));
    if (!pf) { fclose(f); return NULL; }
    
    pf->handle = f;
    pf->writing = (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL);
    return pf;
}

size_t platform_fwrite(const void* ptr, size_t size, size_t count, PlatformFile* file) {
    if (!file || !file->handle) return 0;
    return fwrite(ptr, size, count, (FILE*)file->handle);
}

size_t platform_fread(void* ptr, size_t size, size_t count, PlatformFile* file) {
    if (!file || !file->handle) return 0;
    return fread(ptr, size, count, (FILE*)file->handle);
}

int platform_fclose(PlatformFile* file) {
    if (!file || !file->handle) return -1;
    int result = fclose((FILE*)file->handle);
    free(file);
    return result;
}

int platform_fprintf(PlatformFile* file, const char* format, ...) {
    if (!file || !file->handle) return -1;
    va_list args;
    va_start(args, format);
    int result = vfprintf((FILE*)file->handle, format, args);
    va_end(args);
    return result;
}

int platform_fflush(PlatformFile* file) {
    if (!file || !file->handle) return -1;
    return fflush((FILE*)file->handle);
}

bool platform_mkdir(const char* path) {
#if defined(_WIN32)
    return mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
}

bool platform_path_exists(const char* path) {
#if defined(_WIN32)
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES;
#else
    return access(path, F_OK) == 0;
#endif
}

bool platform_get_executable_path(char* buffer, size_t size) {
#if defined(_WIN32)
    DWORD len = GetModuleFileNameA(NULL, buffer, (DWORD)size);
    return len > 0 && len < size;
#elif defined(__APPLE__)
    uint32_t bufsize = (uint32_t)size;
    int ret = _NSGetExecutablePath(buffer, &bufsize);
    return ret == 0;
#else
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if (len > 0 && len < (ssize_t)size) {
        buffer[len] = '\0';
        return true;
    }
    return false;
#endif
}