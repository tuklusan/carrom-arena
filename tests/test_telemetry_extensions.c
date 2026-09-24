#include "unity.h"
#include "telemetry/trace.h"
#include "common/types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char* my_strdup(const char* s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s) + 1;
    char* d = malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

// Mock MatchState, GameState, ShotPlan, ShotResult, RulesOutcome for tracing
static MatchState mock_match __attribute__((unused)) = {0};
static GameState mock_game __attribute__((unused)) = {0};
static ShotPlan mock_plan __attribute__((unused)) = {0};
static ShotResult mock_result = {0};
static RulesOutcome mock_outcome = {0};

void setUp(void) {}
void tearDown(void) {}

void test_trace_write_near_miss(void) {
    TraceWriter* w = trace_open("test_telemetry_near_miss.jsonl", "tests/logs", true, 12345);
    TEST_ASSERT_NOT_NULL(w);
    
    trace_write_pocket_near_miss(w, 5, 2, 0.005f, 0.1f);
    trace_close(w);
    
    // Validate the JSONL record
    TraceRecordArray records = trace_read_last_records("test_telemetry_near_miss.jsonl", 1);
    
    size_t count = records.count;
    char* line0 = records.lines ? records.lines[0] : NULL;
    
    // Duplicate line0 to avoid using memory that trace_record_array_free will release
    char* line0_copy = line0 ? my_strdup(line0) : NULL;

    trace_record_array_free(&records);
    
    bool success = true;
    if (count != 1) success = false;
    if (line0_copy == NULL) success = false;
    else {
        if (strstr(line0_copy, "\"type\":\"POCKET_NEAR_MISS\"") == NULL) success = false;
        if (strstr(line0_copy, "\"piece_id\":5") == NULL) success = false;
        if (strstr(line0_copy, "\"pocket\":2") == NULL) success = false;
        if (strstr(line0_copy, "\"distance\":0.005000") == NULL) success = false;
        if (strstr(line0_copy, "\"speed\":0.100000") == NULL) success = false;
    }
    
    free(line0_copy);
    TEST_ASSERT_TRUE(success);

}

void test_trace_shot_end_fields(void) {
    TraceWriter* w = trace_open("test_telemetry_shot_end.jsonl", "tests/logs", true, 12345);
    TEST_ASSERT_NOT_NULL(w);
    
    // Setup mock result with a pocketed piece and final positions
    mock_result.pocketed_count = 1;
    mock_result.pocketed_ids[0] = 10;
    mock_result.pocketed_colors[0] = PIECE_WHITE;
    mock_result.pocketed_pocket_indices[0] = 0;
    
    // Set some specific pocket indices for testing the array
    for (int i = 0; i < 19; i++) mock_result.pocketed_pocket_indices[i] = 255; 
    mock_result.pocketed_pocket_indices[0] = 0;
    mock_result.pocketed_pocket_indices[1] = 1;

    mock_result.final_positions[0] = vec2(0.1f, 0.1f);
    mock_result.sim_time = 1.234f;
    
    trace_write_shot_end(w, &mock_result, &mock_outcome);
    trace_close(w);
    
    TraceRecordArray records = trace_read_last_records("test_telemetry_shot_end.jsonl", 1);
    
    size_t count = records.count;
    char* line0_raw = records.lines ? records.lines[0] : NULL;
    char* line0 = line0_raw ? my_strdup(line0_raw) : NULL;

    trace_record_array_free(&records);
    
    TEST_ASSERT_EQUAL_INT(1, count);
    
    // Check for pocketed data
    TEST_ASSERT_NOT_NULL(strstr(line0, "\"piece_id\":10"));
    TEST_ASSERT_NOT_NULL(strstr(line0, "\"pocket\":0"));
    
    // Check for final positions
    TEST_ASSERT_NOT_NULL(strstr(line0, "\"final_positions\":"));
    TEST_ASSERT_NOT_NULL(strstr(line0, "\"pos\":{\"x\":0.100000,\"y\":0.100000}"));
    
    // Check for pocket_indices array
    TEST_ASSERT_NOT_NULL(strstr(line0, "\"pocket_indices\":"));
    TEST_ASSERT_NOT_NULL(strstr(line0, "[0,1"));

    free(line0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_trace_write_near_miss);
    RUN_TEST(test_trace_shot_end_fields);
    return UNITY_END();
}
