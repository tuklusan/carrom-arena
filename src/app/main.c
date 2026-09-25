#include "app.h"
#include <stdio.h>

/* Main entry point - rendered mode by default */
int main(int argc, char* argv[]) {
    AppConfig config = app_parse_args(argc, argv);
    
    // No mode given: app_parse_args already defaults to the rendered mode.
    AppContext* ctx = app_create(&config);
    if (!ctx) {
        fprintf(stderr, "Failed to create app context\n");
        return 1;
    }
    
    int result = app_run(ctx);
    app_destroy(ctx);
    return result;
}