#ifndef CARROM_APP_H
#define CARROM_APP_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Application Modes (Verification Seams - Appendix A.9)
 * --------------------------------------------------------------------------- */
typedef enum {
    APP_MODE_RENDERED,      // Normal rendered arena mode (raylib)
    APP_MODE_DIAGNOSTIC,    // Deterministic single-seed diagnostic
    APP_MODE_SOAK,          // Headless accelerated soak test
    APP_MODE_CAPTURE        // Graphical verification/capture mode
} AppMode;

/* -----------------------------------------------------------------------------
 * App Configuration
 * --------------------------------------------------------------------------- */
typedef struct {
    AppMode mode;
    uint64_t seed;              // Master RNG seed (0 = random from time)
    uint32_t boards;            // Number of boards (soak mode)
    uint32_t seeds;             // Number of seeds (soak mode)
    uint32_t matches;           // Matches per board/seed (soak mode)
    uint32_t frames;            // Frames to capture (capture mode)
    const char* trace_dir;      // Output directory for traces/logs
    const char* capture_dir;    // Output directory for frame captures
    bool verbose;               // Verbose logging
    bool headless;              // Force headless (no raylib window)
    int window_width;           // Window width (rendered/capture)
    int window_height;          // Window height (rendered/capture)
    const char* replay_file;    // Trace file to replay
} AppConfig;

/* Default configuration */
static inline AppConfig app_config_default(void) {
    return (AppConfig){
        .mode = APP_MODE_RENDERED,
        .seed = 0,
        .boards = 100,
        .seeds = 100,
        .matches = 10,
        .frames = 300,
        .trace_dir = "traces",
        .capture_dir = "captures",
        .verbose = false,
        .headless = false,
        .window_width = 1280,
        .window_height = 720,
        .replay_file = NULL
    };
}

/* -----------------------------------------------------------------------------
 * Core Simulation Loop (shared by all modes)
 * --------------------------------------------------------------------------- */
typedef struct AppContext AppContext;

AppContext* app_create(const AppConfig* config);
void app_destroy(AppContext* ctx);
int app_run(AppContext* ctx);  // Returns exit code

/* Mode-specific entry points */
int app_run_rendered(AppContext* ctx);
int app_run_diagnostic(AppContext* ctx);
int app_run_soak(AppContext* ctx);
int app_run_capture(AppContext* ctx);

/* CLI Parsing */
AppConfig app_parse_args(int argc, char* argv[]);
void app_print_usage(const char* prog_name);
void app_print_version(void);

#ifdef __cplusplus
}
#endif

#endif // CARROM_APP_H