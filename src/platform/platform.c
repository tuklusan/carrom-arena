#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
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
    static LARGE_INTEGER freq;   /* constant for the life of the process: query it once */
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
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

void platform_yield(void) {
#if defined(_WIN32)
    SwitchToThread();
#else
    sched_yield();
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

#if !defined(_WIN32)
/* The build force-includes <math.h> before the feature-test macros above, which hides fdopen under strict -std=c17. */
extern FILE* fdopen(int fd, const char* mode);
#endif

FILE* platform_fopen_private(const char* path, const char* mode) {
#if defined(_WIN32)
    return fopen(path, mode);
#else
    int flags = O_CREAT | O_TRUNC | ((strchr(mode, '+') != NULL) ? O_RDWR : O_WRONLY);
    int fd = open(path, flags, 0600);
    if (fd < 0) return NULL;
    FILE* f = fdopen(fd, mode);
    if (!f) close(fd);
    return f;
#endif
}

static FILE* g_diag_file;

void platform_diag_open(const char* path) {
    platform_diag_close();
    g_diag_file = platform_fopen_private(path, "w");
}

void platform_diag_close(void) {
    if (g_diag_file) fclose(g_diag_file);
    g_diag_file = NULL;
}

void platform_diag_logf(const char* fmt, ...) {
    if (!g_diag_file) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(g_diag_file, fmt, args);
    va_end(args);
    fflush(g_diag_file);
}

void platform_fatal(const char* message) {
    platform_diag_logf("[FATAL] %s\n", message);
#if defined(_WIN32)
    MessageBoxA(NULL, message, "SANYALnet Labs Carrom Arena", MB_OK | MB_ICONERROR);
#else
    fprintf(stderr, "%s\n", message);
#endif
}

typedef struct { char name[260]; time_t mtime; } PruneEntry;

static int prune_newest_first(const void* a, const void* b) {
    time_t ta = ((const PruneEntry*)a)->mtime, tb = ((const PruneEntry*)b)->mtime;
    return (ta < tb) - (ta > tb);
}

void platform_prune_old_files(const char* dir, const char* const* prefixes, int prefix_count, int keep) {
    if (!dir || !prefixes || keep < 0) return;
    for (int p = 0; p < prefix_count; p++) {
        DIR* d = opendir(dir);
        if (!d) return;
        PruneEntry list[512];
        int n = 0;
        size_t plen = strlen(prefixes[p]);
        struct dirent* de;
        while ((de = readdir(d)) != NULL && n < 512) {
            if (strncmp(de->d_name, prefixes[p], plen) != 0 || strlen(de->d_name) >= sizeof(list[0].name)) continue;
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
            struct stat st;
            if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            strcpy(list[n].name, de->d_name);
            list[n].mtime = st.st_mtime;
            n++;
        }
        closedir(d);
        qsort(list, (size_t)n, sizeof(list[0]), prune_newest_first);
        for (int i = keep; i < n; i++) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir, list[i].name);
            remove(path);
        }
    }
}
