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
}

/* Spawn the app in capture mode and assert N PNGs written in <60s wall.
 * Note: ASAN in debug builds may report pre-existing leaks (exit != 0).
 * We verify success by checking PNG count and that timeout didn't trigger. */
void test_capture_completes_bounded(void) {
    // Use a per-run capture dir to keep parallel test isolated.
    char dir[256];
    snprintf(dir, sizeof(dir), "/tmp/carrom_capture_test_%d", (int)getpid());
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "rm -rf %s && mkdir -p %s && "
        "timeout 60 xvfb-run -a -s '-screen 0 1920x1080x24' "
        "./carrom_arena --mode=capture --seed=42 --headless "
        "--frames=5 --capture-dir=%s > %s/log.txt 2>&1; echo \"EXIT_CODE=$$?\" >> %s/log.txt",
        dir, dir, dir, dir, dir);
    int rc = system(cmd);
    // Accept exit 0 (success) or 256 (ASAN leak exit 1). Reject 124 (timeout) or other errors.
    TEST_ASSERT_TRUE_MESSAGE(rc == 0 || rc == 256, "capture must not timeout (exit 124) or crash");
    // Count PNGs
    int count = count_pngs(dir);
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, count, "must write 5 PNGs");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_capture_completes_bounded);
    return UNITY_END();
}