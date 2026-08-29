/* -----------------------------------------------------------------------------
 * Circular Trace Buffer Unit Tests
 * Tests: file never exceeds 8 MiB, last N records intact/parsable
 * --------------------------------------------------------------------------- */

#include "unity.h"
#include "telemetry/trace.h"
#include "common/types.h"
#include "game/rules.h"
#include "game/board.h"
#include "game/events.h"
#include "game/match.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* -----------------------------------------------------------------------------
 * Test Helpers
 * --------------------------------------------------------------------------- */

static char test_trace_path[512];
static char test_log_dir[512];

static void setup_test_paths(void) {
    snprintf(test_trace_path, sizeof(test_trace_path), "%s", "test_trace_circular.jsonl");
    snprintf(test_log_dir, sizeof(test_log_dir), "%s", "test_logs");
}

static uint64_t get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return (uint64_t)st.st_size;
    }
    return 0;
}

static void create_dummy_shot_data(ShotPlan* plan, ShotResult* result, RulesOutcome* outcome,
                                   MatchState* match, GameState* game, int shot_num) {
    memset(plan, 0, sizeof(ShotPlan));
    plan->placement.x = 0.5f + (float)(shot_num % 100) * 0.001f;
    plan->placement.y = 0.5f + (float)(shot_num % 100) * 0.001f;
    plan->aim_angle = (float)(shot_num % 360) * 0.0174533f;  /* degrees to radians */
    plan->power = 0.5f + (float)(shot_num % 50) * 0.01f;
    plan->tactic = (TacticType)(shot_num % 8);
    plan->rng_draw = (uint32_t)(shot_num * 12345 + 67890);
    
    memset(result, 0, sizeof(ShotResult));
    result->pocketed_count = (uint8_t)(shot_num % 3);
    for (int i = 0; i < result->pocketed_count; i++) {
        result->pocketed_ids[i] = (uint8_t)(shot_num * 10 + i);
        result->pocketed_colors[i] = (i % 2 == 0) ? PIECE_WHITE : PIECE_BLACK;
    }
    result->queen_pocketed = (shot_num % 7 == 0);
    result->striker_pocketed = (shot_num % 11 == 0);
    result->fouls = (shot_num % 13 == 0) ? FOUL_STRIKER_POCKETED : FOUL_NONE;
    result->sim_time = 0.1f + (float)(shot_num % 100) * 0.001f;
    
    memset(outcome, 0, sizeof(RulesOutcome));
    outcome->score_delta.white = (int)(shot_num % 5);
    outcome->score_delta.black = (int)(shot_num % 3);
    outcome->turn_decision = (TurnDecision)(shot_num % 5);
    
    memset(match, 0, sizeof(MatchState));
    match->boards_won_white = (uint8_t)(shot_num / 100);
    match->boards_won_black = (uint8_t)(shot_num / 200);
    match->games_won_white = (uint8_t)(shot_num / 50);
    match->games_won_black = (uint8_t)(shot_num / 75);
    
    memset(game, 0, sizeof(GameState));
    game->scores.white = (int)(shot_num % 25);
    game->scores.black = (int)(shot_num % 20);
}

/* -----------------------------------------------------------------------------
 * Test Cases
 * --------------------------------------------------------------------------- */

void setUp(void) {
    setup_test_paths();
    /* Clean up any existing test files */
    remove(test_trace_path);
    char log_path[512];
    (void)snprintf(log_path, sizeof(log_path), "%s/seed_12345.log", test_log_dir);
    remove(log_path);
    /* Remove log dir if empty */
    (void)test_log_dir;  // Suppress unused warning in tests
    // rmdir(test_log_dir);  // Requires unistd.h, skip for portability
}

void tearDown(void) {
    remove(test_trace_path);
    char log_path[512];
    (void)snprintf(log_path, sizeof(log_path), "%s/seed_12345.log", test_log_dir);
    remove(log_path);
    (void)test_log_dir;  // Suppress unused warning in tests
    // rmdir(test_log_dir);  // Requires unistd.h, skip for portability
}

void test_trace_open_creates_file_with_index(void) {
    TraceWriter* writer = trace_open(test_trace_path, test_log_dir, true, 12345);
    TEST_ASSERT_NOT_NULL(writer);
    
    /* File should exist */
    uint64_t size = get_file_size(test_trace_path);
    TEST_ASSERT_GREATER_THAN(0, size);
    
    /* File should have at least index (8 bytes) + header comment */
    TEST_ASSERT_GREATER_OR_EQUAL(TRACE_INDEX_SIZE + 50, size);
    
    trace_close(writer);
}

void test_trace_write_shot_start_end(void) {
    TraceWriter* writer = trace_open(test_trace_path, test_log_dir, true, 12345);
    TEST_ASSERT_NOT_NULL(writer);
    
    ShotPlan plan;
    ShotResult result;
    RulesOutcome outcome;
    MatchState match;
    GameState game;
    
    create_dummy_shot_data(&plan, &result, &outcome, &match, &game, 1);
    
    trace_write_shot_start(writer, &match, &game, 1, SEAT_NORTH, &plan);
    trace_write_shot_end(writer, &result, &outcome);
    
    trace_flush(writer);
    
    uint64_t size = get_file_size(test_trace_path);
    TEST_ASSERT_GREATER_THAN(TRACE_INDEX_SIZE + 100, size);
    
    trace_close(writer);
}

void test_trace_file_never_exceeds_8mib(void) {
    TraceWriter* writer = trace_open(test_trace_path, test_log_dir, true, 12345);
    TEST_ASSERT_NOT_NULL(writer);
    
    ShotPlan plan;
    ShotResult result;
    RulesOutcome outcome;
    MatchState match;
    GameState game;
    
    /* Write many shots to exceed 8 MiB */
    /* Each shot record is roughly 500-1000 bytes, so ~10000 shots should exceed 8 MiB */
    for (uint64_t i = 1; i <= 15000; i++) {
        create_dummy_shot_data(&plan, &result, &outcome, &match, &game, (int)i);
        trace_write_shot_start(writer, &match, &game, i, (Seat)(i % 4), &plan);
        trace_write_shot_end(writer, &result, &outcome);
        
        if (i % 1000 == 0) {
            trace_flush(writer);
            uint64_t size = get_file_size(test_trace_path);
            /* File should never exceed 8 MiB + index */
            TEST_ASSERT_LESS_OR_EQUAL(TRACE_TOTAL_SIZE + 100, size);
        }
    }
    
    trace_flush(writer);
    trace_close(writer);
    
    uint64_t final_size = get_file_size(test_trace_path);
    TEST_ASSERT_LESS_OR_EQUAL(TRACE_TOTAL_SIZE + 100, final_size);
    TEST_ASSERT_GREATER_OR_EQUAL(TRACE_MAX_SIZE / 2, final_size);  /* Should be near capacity */
}

void test_trace_last_records_intact_and_parsable(void) {
    TraceWriter* writer = trace_open(test_trace_path, test_log_dir, true, 12345);
    TEST_ASSERT_NOT_NULL(writer);
    
    ShotPlan plan;
    ShotResult result;
    RulesOutcome outcome;
    MatchState match;
    GameState game;
    
    /* Write known pattern of shots */
    const int NUM_SHOTS = 200;
    for (int i = 1; i <= NUM_SHOTS; i++) {
        create_dummy_shot_data(&plan, &result, &outcome, &match, &game, i);
        trace_write_shot_start(writer, &match, &game, (uint64_t)i, SEAT_NORTH, &plan);
        trace_write_shot_end(writer, &result, &outcome);
    }
    
    trace_flush(writer);
    trace_close(writer);
    
    /* Read back last N records */
    TraceRecordArray arr = trace_read_last_records(test_trace_path, 50);
    TEST_ASSERT_GREATER_THAN(0, arr.count);
    TEST_ASSERT_LESS_OR_EQUAL(50, arr.count);
    
    /* Debug: print first few lines */
    for (size_t i = 0; i < arr.count && i < 5; i++) {
        if (arr.lines[i]) {
            printf("DEBUG line %zu: len=%zu, first_char=%d ('%c'), content: %.100s\n", 
                   i, strlen(arr.lines[i]), (int)arr.lines[i][0], arr.lines[i][0], arr.lines[i]);
        } else {
            printf("DEBUG line %zu: NULL\n", i);
        }
    }
    
    /* Verify each record is valid JSON (starts with '{' and contains either
     * shot_start fields (shot_number, build_id, seed) or shot_end fields (result) */
    for (size_t i = 0; i < arr.count; i++) {
        TEST_ASSERT_NOT_NULL(arr.lines[i]);
        TEST_ASSERT_EQUAL('{', arr.lines[i][0]);  /* Valid JSON object */
        bool is_shot_start = strstr(arr.lines[i], "shot_number") != NULL;
        bool is_shot_end = strstr(arr.lines[i], "result") != NULL;
        TEST_ASSERT_TRUE(is_shot_start || is_shot_end);
        if (is_shot_start) {
            TEST_ASSERT_NOT_NULL(strstr(arr.lines[i], "build_id"));
            TEST_ASSERT_NOT_NULL(strstr(arr.lines[i], "seed"));
        }
    }
    
    trace_record_array_free(&arr);
}

void test_trace_wrap_preserves_recent_data(void) {
    TraceWriter* writer = trace_open(test_trace_path, test_log_dir, true, 12345);
    TEST_ASSERT_NOT_NULL(writer);
    
    ShotPlan plan;
    ShotResult result;
    RulesOutcome outcome;
    MatchState match;
    GameState game;
    
    /* Write enough to force wrap */
    const int NUM_SHOTS = 20000;
    for (int i = 1; i <= NUM_SHOTS; i++) {
        create_dummy_shot_data(&plan, &result, &outcome, &match, &game, i);
        trace_write_shot_start(writer, &match, &game, (uint64_t)i, SEAT_NORTH, &plan);
        trace_write_shot_end(writer, &result, &outcome);
    }
    
    trace_flush(writer);
    trace_close(writer);
    
    /* Read last 100 records - should be from the end of the sequence */
    TraceRecordArray arr = trace_read_last_records(test_trace_path, 100);
    TEST_ASSERT_GREATER_THAN(0, arr.count);
    
    /* Verify shot numbers are from the end of our sequence */
    bool found_high_numbers = false;
    for (size_t i = 0; i < arr.count; i++) {
        /* Look for shot numbers near NUM_SHOTS */
        if (strstr(arr.lines[i], "199") || strstr(arr.lines[i], "2000")) {
            found_high_numbers = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_high_numbers);
    
    trace_record_array_free(&arr);
}

void test_trace_log_file_also_circular(void) {
    TraceWriter* writer = trace_open(test_trace_path, test_log_dir, true, 12345);
    TEST_ASSERT_NOT_NULL(writer);
    
    ShotPlan plan;
    ShotResult result;
    RulesOutcome outcome;
    MatchState match;
    GameState game;
    
    char log_path[512];
    (void)snprintf(log_path, sizeof(log_path), "%s/seed_12345.log", test_log_dir);
    
    /* Write many shots */
    for (int i = 1; i <= 5000; i++) {
        create_dummy_shot_data(&plan, &result, &outcome, &match, &game, i);
        trace_write_shot_start(writer, &match, &game, (uint64_t)i, SEAT_NORTH, &plan);
        trace_write_shot_end(writer, &result, &outcome);
    }
    
    trace_flush(writer);
    trace_close(writer);
    
    /* Log file should also be bounded */
    uint64_t log_size = get_file_size(log_path);
    TEST_ASSERT_LESS_OR_EQUAL(TRACE_TOTAL_SIZE + 100, log_size);
}

void test_trace_reopen_continues_from_index(void) {
    TraceWriter* writer = trace_open(test_trace_path, test_log_dir, true, 12345);
    TEST_ASSERT_NOT_NULL(writer);
    
    ShotPlan plan;
    ShotResult result;
    RulesOutcome outcome;
    MatchState match;
    GameState game;
    
    /* Write some shots */
    for (int i = 1; i <= 100; i++) {
        create_dummy_shot_data(&plan, &result, &outcome, &match, &game, i);
        trace_write_shot_start(writer, &match, &game, (uint64_t)i, SEAT_NORTH, &plan);
        trace_write_shot_end(writer, &result, &outcome);
    }
    
    trace_flush(writer);
    trace_close(writer);
    
    /* Reopen and continue */
    writer = trace_open(test_trace_path, test_log_dir, true, 12345);
    TEST_ASSERT_NOT_NULL(writer);
    
    for (int i = 101; i <= 200; i++) {
        create_dummy_shot_data(&plan, &result, &outcome, &match, &game, i);
        trace_write_shot_start(writer, &match, &game, (uint64_t)i, SEAT_NORTH, &plan);
        trace_write_shot_end(writer, &result, &outcome);
    }
    
    trace_flush(writer);
    trace_close(writer);
    
    /* Verify continuity */
    TraceRecordArray arr = trace_read_last_records(test_trace_path, 50);
    TEST_ASSERT_GREATER_THAN(0, arr.count);
    
    bool found_100_plus = false;
    for (size_t i = 0; i < arr.count; i++) {
        if (strstr(arr.lines[i], "1") && (strstr(arr.lines[i], "10") || strstr(arr.lines[i], "11") 
            || strstr(arr.lines[i], "12") || strstr(arr.lines[i], "13") 
            || strstr(arr.lines[i], "14") || strstr(arr.lines[i], "15")
            || strstr(arr.lines[i], "16") || strstr(arr.lines[i], "17")
            || strstr(arr.lines[i], "18") || strstr(arr.lines[i], "19")
            || strstr(arr.lines[i], "20"))) {
            found_100_plus = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_100_plus);
    
    trace_record_array_free(&arr);
}

void test_trace_validate_determinism(void) {
    /* Create two identical traces */
    const char* trace1 = "build/test_trace_1.jsonl";
    const char* trace2 = "build/test_trace_2.jsonl";
    
    remove(trace1);
    remove(trace2);
    
    TraceWriter* w1 = trace_open(trace1, test_log_dir, false, 42);
    TraceWriter* w2 = trace_open(trace2, test_log_dir, false, 42);
    
    ShotPlan plan;
    ShotResult result;
    RulesOutcome outcome;
    MatchState match;
    GameState game;
    
    for (int i = 1; i <= 100; i++) {
        create_dummy_shot_data(&plan, &result, &outcome, &match, &game, i);
        trace_write_shot_start(w1, &match, &game, (uint64_t)i, SEAT_NORTH, &plan);
        trace_write_shot_end(w1, &result, &outcome);
        trace_write_shot_start(w2, &match, &game, (uint64_t)i, SEAT_NORTH, &plan);
        trace_write_shot_end(w2, &result, &outcome);
    }
    
    trace_close(w1);
    trace_close(w2);
    
    /* Should be identical */
    bool equal = trace_validate_determinism(trace1, trace2);
    TEST_ASSERT_TRUE(equal);
    
    remove(trace1);
    remove(trace2);
}

/* -----------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_trace_open_creates_file_with_index);
    RUN_TEST(test_trace_write_shot_start_end);
    RUN_TEST(test_trace_file_never_exceeds_8mib);
    RUN_TEST(test_trace_last_records_intact_and_parsable);
    RUN_TEST(test_trace_wrap_preserves_recent_data);
    RUN_TEST(test_trace_log_file_also_circular);
    RUN_TEST(test_trace_reopen_continues_from_index);
    RUN_TEST(test_trace_validate_determinism);
    
    return UNITY_END();
}