#include "unity.h"
#include "common/types.h"
#include "common/vecmath.h"
#include "game/board.h"
#include "physics/physics.h"
#include "render/piece_draw.h"
#include "telemetry/trace.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_draw_pos_never_lerps_pocketed_piece(void) {
    Vec2 prev = {0.30f, 0.30f}, curr = {0.31f, 0.31f}, board_pos = {0.2f, 0.2f}, out = {9, 9};
    const float alphas[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_FALSE(board_view_piece_draw_pos(true, true, true, board_pos, prev, curr, alphas[i], &out));
        TEST_ASSERT_FALSE(board_view_piece_draw_pos(false, false, true, board_pos, prev, curr, alphas[i], &out));
    }
    TEST_ASSERT_TRUE(board_view_piece_draw_pos(true, false, true, board_pos, prev, curr, 0.5f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.305f, out.x);
    TEST_ASSERT_TRUE(board_view_piece_draw_pos(true, false, false, board_pos, prev, curr, 0.5f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, out.x);
}

void test_physics_reports_pocketed_while_other_piece_moves(void) {
    GameState game;
    game_state_init(&game, 4242);
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, NULL);
    game.board.pieces[0].position = POCKET_CENTERS[0];
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &game.board, SEAT_NORTH);
    physics_step(pw, PHYSICS_DT);

    TEST_ASSERT_TRUE(physics_is_piece_pocketed(pw, 0));
    Vec2 v;
    TEST_ASSERT_FALSE(physics_get_piece_velocity(pw, 0, &v));
    TEST_ASSERT_FALSE(physics_is_piece_pocketed(pw, 5));
    TEST_ASSERT_TRUE(physics_get_piece_velocity(pw, 5, &v));
    physics_destroy(pw);
}

static char* read_all(const char* path) {
    static char buf[16384];
    TraceRecordArray r = trace_read_last_records(path, 10);
    buf[0] = '\0';
    for (size_t i = 0; i < r.count; i++) {
        strncat(buf, r.lines[i], sizeof(buf) - strlen(buf) - 2);
        strcat(buf, "\n");
    }
    trace_record_array_free(&r);
    return buf;
}

void test_trace_pocket_progress_interrupted(void) {
    const char* path = "test_pocket_visibility.jsonl";
    TraceWriter* w = trace_open(path, "tests/logs", false, 99);
    TEST_ASSERT_NOT_NULL(w);
    Vec2 pos[MAX_PIECES] = {{0}}, vel[MAX_PIECES] = {{0}}, sp = {0.1f, 0.2f}, sv = {1.0f, 0.0f};
    bool alive[MAX_PIECES] = {false};
    alive[3] = true; pos[3] = (Vec2){0.4f, 0.4f}; vel[3] = (Vec2){0.5f, 0.0f};
    alive[4] = true; pos[4] = (Vec2){-0.4f, 0.4f};  /* at rest */
    uint8_t pocketed[2] = {0, 18};

    trace_write_pocket(w, 7, 18, PIECE_QUEEN, 2, 1.25f);
    trace_write_shot_snapshot(w, false, 7, 2.0f, "SHOT_EXECUTION", &sp, &sv, pos, vel, alive, pocketed, 2);
    trace_write_shot_snapshot(w, true, 7, 3.5f, "SETTLING", &sp, &sv, pos, vel, alive, pocketed, 2);
    /* no trace_close: SHOT_INTERRUPTED must already be flushed */
    char* text = read_all(path);
    TEST_ASSERT_NOT_NULL(strstr(text, "\"type\":\"POCKET\""));
    TEST_ASSERT_NOT_NULL(strstr(text, "\"piece_id\":18"));
    char* progress = strstr(text, "\"type\":\"SHOT_PROGRESS\"");
    TEST_ASSERT_NOT_NULL(progress);
    TEST_ASSERT_NOT_NULL(strstr(progress, "\"id\":3"));
    char* interrupted = strstr(text, "\"type\":\"SHOT_INTERRUPTED\"");
    TEST_ASSERT_NOT_NULL(interrupted);
    TEST_ASSERT_NOT_NULL(strstr(interrupted, "\"id\":4"));
    TEST_ASSERT_NOT_NULL(strstr(interrupted, "\"pocketed\":[0,18]"));
    /* progress snapshot omits the resting piece */
    char* nl = strchr(progress, '\n');
    TEST_ASSERT_TRUE(nl != NULL);
    *nl = '\0';
    TEST_ASSERT_NULL(strstr(progress, "\"id\":4"));
    trace_close(w);
    remove(path);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_draw_pos_never_lerps_pocketed_piece);
    RUN_TEST(test_physics_reports_pocketed_while_other_piece_moves);
    RUN_TEST(test_trace_pocket_progress_interrupted);
    return UNITY_END();
}
