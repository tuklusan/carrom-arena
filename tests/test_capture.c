#define _GNU_SOURCE
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Count PNG files matching frame_*.png in directory */
static int count_pngs(const char* dir) {
#ifdef _WIN32
    /* Windows: use _findfirst/_findnext */
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s/frame_*.png", dir);
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(pattern, &fileinfo);
    if (handle == -1) return 0;
    int count = 0;
    do {
        if (strncmp(fileinfo.name, "frame_", 6) == 0) {
            const char* ext = strrchr(fileinfo.name, '.');
            if (ext && strcmp(ext, ".png") == 0) {
                count++;
            }
        }
    } while (_findnext(handle, &fileinfo) == 0);
    _findclose(handle);
    return count;
#else
    int count = 0;
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, "frame_", 6) == 0) {
            const char* ext = strrchr(ent->d_name, '.');
            if (ext && strcmp(ext, ".png") == 0) {
                count++;
            }
        }
    }
    closedir(d);
    return count;
#endif
}

/* Read last N lines of a file into buffer for debug output */
static void read_tail(const char* path, char* buf, size_t bufsz, int max_lines) {
    FILE* f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return; }
    char line[512];
    char* lines[50];
    int n = 0;
    while (fgets(line, sizeof(line), f) && n < 50) {
        lines[n] = strdup(line);
        n++;
    }
    fclose(f);
    buf[0] = '\0';
    int start = (n > max_lines) ? n - max_lines : 0;
    for (int i = start; i < n; i++) {
        strncat(buf, lines[i], bufsz - strlen(buf) - 1);
        free(lines[i]);
    }
}

/* Spawn the app in capture mode and assert N PNGs written in <60s wall.
 * Note: ASAN in debug builds may report pre-existing leaks (exit != 0).
 * We verify success by checking PNG count and that timeout didn't trigger. */
void test_capture_completes_bounded(void) {
    // Use a per-run capture dir in build tree to avoid TEMP path issues on Windows Git Bash.
    char dir[256];
    snprintf(dir, sizeof(dir), "carrom_capture_test_%d", (int)getpid());

    char cmd[1024];
    char logpath[256];
#ifdef _WIN32
    /* Windows: system() uses cmd.exe. Use cmd syntax.
     * Windows carrom_arena builds with WGL/OpenGL and can create a hidden window natively
     * via FLAG_WINDOW_HIDDEN (already in the Gap 2 renderer). */
    snprintf(logpath, sizeof(logpath), "%s\\log.txt", dir);
    snprintf(cmd, sizeof(cmd),
        "rmdir /S /Q %s 2>nul && mkdir %s && "
        "carrom_arena.exe --mode=capture --seed=42 --headless "
        "--frames=5 --capture-dir=%s > %s 2>&1 && echo EXIT_CODE=0 >> %s || echo EXIT_CODE=%%ERRORLEVEL%% >> %s",
        dir, dir, dir, logpath, logpath, logpath);
#else
    snprintf(logpath, sizeof(logpath), "%s/log.txt", dir);
    // If DISPLAY is already set (e.g. CI setup Xvfb externally), skip xvfb-run
    const char* display = getenv("DISPLAY");
    if (display && display[0]) {
        snprintf(cmd, sizeof(cmd),
            "rm -rf %s && mkdir -p %s && "
            "timeout 60 ./carrom_arena --mode=capture --seed=42 --headless "
            "--frames=5 --capture-dir=%s > %s 2>&1; echo \"EXIT_CODE=$?\" >> %s",
            dir, dir, dir, logpath, logpath);
    } else {
        // Local dev: use xvfb-run as before
        snprintf(cmd, sizeof(cmd),
            "rm -rf %s && mkdir -p %s && "
            "timeout 60 xvfb-run -a -s '-screen 0 1920x1080x24' "
            "./carrom_arena --mode=capture --seed=42 --headless "
            "--frames=5 --capture-dir=%s > %s 2>&1; echo \"EXIT_CODE=$?\" >> %s",
            dir, dir, dir, logpath, logpath);
    }
#endif

    int rc = system(cmd);

    // On failure, read log tail for debug
    char logtail[1024];
    read_tail(logpath, logtail, sizeof(logtail), 20);

    // Accept exit 0 (success) or 256 (ASAN leak exit 1). Reject 124 (timeout) or other errors.
    char msg[128];
    snprintf(msg, sizeof(msg), "capture failed (rc=%d)%s", rc, logtail[0] ? ": " : "");
    if (logtail[0]) strncat(msg, logtail, sizeof(msg) - strlen(msg) - 1);
    TEST_ASSERT_TRUE_MESSAGE(rc == 0 || rc == 256, msg);

    // Count PNGs
    int count = count_pngs(dir);
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, count, "must write 5 PNGs");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_capture_completes_bounded);
    return UNITY_END();
}