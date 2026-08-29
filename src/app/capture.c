#include "app.h"
#include <stdio.h>

/* Capture mode entry point */
int main(int argc, char* argv[]) {
    AppConfig config = app_parse_args(argc, argv);
    config.mode = APP_MODE_CAPTURE;
    
    AppContext* ctx = app_create(&config);
    if (!ctx) {
        fprintf(stderr, "Failed to create app context\n");
        return 1;
    }
    
    int result = app_run(ctx);
    app_destroy(ctx);
    return result;
}