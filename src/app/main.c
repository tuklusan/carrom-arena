#include "app.h"
#include <stdio.h>

/* Main entry point - rendered mode by default */
int main(int argc, char* argv[]) {
    AppConfig config = app_parse_args(argc, argv);
    
    // If no mode specified, default to rendered
    if (config.mode == APP_MODE_RENDERED && argc == 1) {
        // Default behavior
    }
    
    AppContext* ctx = app_create(&config);
    if (!ctx) {
        fprintf(stderr, "Failed to create app context\n");
        return 1;
    }
    
    int result = app_run(ctx);
    app_destroy(ctx);
    return result;
}