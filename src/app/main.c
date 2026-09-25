#include "app.h"
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#endif

/* Main entry point - rendered mode by default */
int main(int argc, char* argv[]) {
#if defined(_WIN32)
    /* The game is built as a windowed program (no console window of its own). When it is started from a terminal, borrow
     * that terminal so that --help, --version and the headless modes can still print their answers. */
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONOUT$", "w", stderr);
    }
#endif
    AppConfig config = app_parse_args(argc, argv);
    
    // No mode given: app_parse_args already defaults to the rendered mode.
    AppContext* ctx = app_create(&config);
    if (!ctx) {
        return 1;   /* app_create has already told the player why (platform_fatal) */
    }
    
    int result = app_run(ctx);
    app_destroy(ctx);
    return result;
}