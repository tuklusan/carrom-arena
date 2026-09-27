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
    APP_MODE_SOAK           // Headless accelerated soak test
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
    const char* trace_dir;      // Output directory for traces/logs
    bool verbose;               // Verbose logging
    int window_width;           // Window width
    int window_height;          // Window height
    float playback_speed;       // Simulation speed multiplier (0.05-4.0)
    uint32_t ai_budget_ms;      // AI decision time budget in milliseconds (default 250)
    bool debug_phase;           // Enable per-frame phase debug logging
    bool no_radio;              // Do not start the internet radio
} AppConfig;

/* Default configuration */
static inline AppConfig app_config_default(void) {
    return (AppConfig){
        .mode = APP_MODE_RENDERED,
        .seed = 0,
        .boards = 100,
        .seeds = 100,
        .matches = 10,
        .trace_dir = "traces",
        .verbose = false,
        .window_width = 560,
        .window_height = 560,
        .playback_speed = 1.0f,   // real-time (1x) default; keys change it, 0.05x-4x
        .ai_budget_ms = 150,      // R5: Default AI budget: 150ms (lowered from 250)
        .debug_phase = false,     // Per-frame phase debug logging
        .no_radio = false
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

/* CLI Parsing */
AppConfig app_parse_args(int argc, char* argv[]);
void app_print_usage(const char* prog_name);
void app_print_version(void);

#ifdef __cplusplus
}
#endif

#endif // CARROM_APP_H