#include <stdio.h>
#include <stdlib.h>
#include "telemetry/trace.h"
#include "platform/platform.h"

int main() {
    printf("Testing trace_open...\n");
    TraceWriter* w = trace_open("tests/debug_trace.jsonl", "tests/logs", true, 12345);
    if (w) {
        printf("Success!\n");
        trace_close(w);
    } else {
        printf("Failed: trace_open returned NULL\n");
    }
    return 0;
}
