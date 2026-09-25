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


void test_aim_line_length_is_proportional_to_power(void) {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, aim_line_length(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, aim_line_length(1.0f), 2.0f * aim_line_length(0.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, aim_line_length(0.6f), 3.0f * aim_line_length(0.2f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, aim_line_length(1.0f), aim_line_length(1.7f));  /* clamped */
}

void test_pocket_records_where_piece_and_striker_fell_in(void) {
    GameState game;
    game_state_init(&game, 99);
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, NULL);
    game.board.pieces[0].position = POCKET_CENTERS[3];
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &game.board, SEAT_NORTH);
    physics_place_striker(pw, SEAT_NORTH, POCKET_CENTERS[2]);
    physics_step(pw, PHYSICS_DT);

    ShotResult r;
    shot_result_init(&r);
    physics_collect_pocketed(pw, &r);
    TEST_ASSERT_TRUE(r.pocketed_count >= 1);
    Vec2 pos, vel;
    physics_get_pocketed_last(pw, 0, &pos, &vel);
    TEST_ASSERT_TRUE(hypotf(pos.x - POCKET_CENTERS[r.pocketed_pocket_indices[0]].x, pos.y - POCKET_CENTERS[r.pocketed_pocket_indices[0]].y) < 0.06f);

    Vec2 sp, sv;
    int spocket = -1;
    TEST_ASSERT_TRUE(physics_get_striker_pocket_info(pw, &sp, &sv, &spocket));
    TEST_ASSERT_EQUAL_INT(2, spocket);
    physics_destroy(pw);
}

/* A new board must put ALL nineteen coins into the physics world, including the ones pocketed on the previous board */
void test_sync_recreates_coins_pocketed_on_the_previous_board(void) {
    GameState game;
    game_state_init(&game, 5);
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, NULL);
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &game.board, SEAT_NORTH);
    /* first board: everything but coins 2 and 4 gets pocketed (their bodies are destroyed) */
    for (int i = 0; i < MAX_PIECES; i++) {
        if (i == 2 || i == 4) continue;
        game.board.pieces[i].on_board = false;
        game.board.pieces[i].pocketed = true;
    }
    physics_sync_from_board(pw, &game.board, SEAT_NORTH);
    Vec2 v;
    int alive = 0;
    for (int i = 0; i < MAX_PIECES; i++) if (physics_get_piece_velocity(pw, i, &v)) alive++;
    TEST_ASSERT_EQUAL_INT(2, alive);
    /* second board: a fresh rack */
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, NULL);
    physics_sync_from_board(pw, &game.board, SEAT_EAST);
    alive = 0;
    for (int i = 0; i < MAX_PIECES; i++) {
        if (physics_get_piece_velocity(pw, i, &v)) alive++;
        TEST_ASSERT_FALSE(physics_is_piece_pocketed(pw, i));
    }
    TEST_ASSERT_EQUAL_INT(MAX_PIECES, alive);
    Vec2 pos[MAX_PIECES];
    physics_get_positions(pw, pos);
    for (int i = 0; i < MAX_PIECES; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, game.board.pieces[i].position.x, pos[i].x);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, game.board.pieces[i].position.y, pos[i].y);
    }
    /* and the queen returned to the centre after an uncovered pocket gets her body back too */
    game.board.pieces[QUEEN_ID].on_board = false; game.board.pieces[QUEEN_ID].pocketed = true;
    physics_sync_from_board(pw, &game.board, SEAT_EAST);
    TEST_ASSERT_FALSE(physics_get_piece_velocity(pw, QUEEN_ID, &v));
    game.board.pieces[QUEEN_ID].on_board = true; game.board.pieces[QUEEN_ID].pocketed = false;
    game.board.pieces[QUEEN_ID].position = (Vec2){ 0.0f, 0.0f };
    physics_sync_from_board(pw, &game.board, SEAT_EAST);
    TEST_ASSERT_TRUE(physics_get_piece_velocity(pw, QUEEN_ID, &v));
    physics_destroy(pw);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_draw_pos_never_lerps_pocketed_piece);
    RUN_TEST(test_physics_reports_pocketed_while_other_piece_moves);
    RUN_TEST(test_trace_pocket_progress_interrupted);
    RUN_TEST(test_aim_line_length_is_proportional_to_power);
    RUN_TEST(test_sync_recreates_coins_pocketed_on_the_previous_board);
    RUN_TEST(test_pocket_records_where_piece_and_striker_fell_in);
    return UNITY_END();
}
