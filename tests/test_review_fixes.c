/* Regression tests for the defects found in the adversarial code review. */
#include "unity.h"
#include "common/types.h"
#include "common/rng.h"
#include "common/strategy_profiles.h"
#include "game/board.h"
#include "game/rules.h"
#include "game/match.h"
#include "physics/physics.h"
#include "physics/physics_snapshot.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* A world made from a snapshot must contain only the coins the snapshot has on the board. It used to keep a body at the
 * origin for every pocketed coin: phantom coins in the middle of the board that every AI scratch simulation then hit. */
void test_scratch_world_has_no_phantom_coins(void) {
    PhysicsWorld* live = physics_create();
    BoardState board;
    board_state_init(&board);
    board.pieces[0].on_board = true;          /* one coin far off the line of fire; every other coin is off the board */
    board.pieces[0].position = (Vec2){ 0.30f, -0.10f };
    physics_sync_from_board(live, &board, SEAT_NORTH);
    PhysicsSnapshot* snap = physics_snapshot(live);
    TEST_ASSERT_NOT_NULL(snap);
    PhysicsWorld* scratch = physics_world_from_snapshot(snap);
    TEST_ASSERT_NOT_NULL(scratch);

    physics_place_striker(scratch, SEAT_NORTH, (Vec2){ 0.0f, BASELINE_Y_NORTH });
    physics_apply_shot(scratch, -(float)M_PI / 2.0f, 0.6f);   /* straight down through the middle of the board */
    float min_y = 1.0f;
    for (int i = 0; i < 120 * 3; i++) {
        physics_step(scratch, PHYSICS_DT);
        Vec2 s;
        physics_get_striker_position(scratch, &s);
        if (s.y < min_y) min_y = s.y;
    }
    Vec2 pos[MAX_PIECES];
    physics_get_final_positions(scratch, pos);
    physics_destroy(scratch);
    physics_snapshot_destroy(snap);
    physics_destroy(live);
    TEST_ASSERT_TRUE_MESSAGE(min_y < -0.40f, "the striker was stopped by something in the middle of the board");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.30f, pos[0].x);        /* and the lone coin was never touched */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.10f, pos[0].y);
}

/* A scratch simulation runs on its own clock, whatever time the live world has reached */
void test_scratch_world_clock_starts_at_zero(void) {
    PhysicsWorld* live = physics_create();
    BoardState board;
    board_state_init(&board);
    physics_sync_from_board(live, &board, SEAT_NORTH);
    for (int i = 0; i < 100; i++) physics_step(live, PHYSICS_DT);
    TEST_ASSERT_TRUE(physics_get_sim_time(live) > 0.5f);
    PhysicsSnapshot* snap = physics_snapshot(live);
    PhysicsWorld* scratch = physics_world_from_snapshot(snap);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, physics_get_sim_time(scratch));
    physics_destroy(scratch);
    physics_snapshot_destroy(snap);
    physics_destroy(live);
}

/* Restoring a snapshot into a world whose scratch shot pocketed a coin must bring the coin back */
void test_restore_recreates_a_pocketed_coin(void) {
    PhysicsWorld* pw = physics_create();
    BoardState board;
    board_state_init(&board);
    board.pieces[0].on_board = true;
    board.pieces[0].position = (Vec2){ 0.44f, 0.35f };
    physics_sync_from_board(pw, &board, SEAT_NORTH);
    PhysicsSnapshot* snap = physics_snapshot(pw);
    /* knock the coin into the top-right pocket: the striker comes up the same line */
    physics_place_striker(pw, SEAT_NORTH, (Vec2){ 0.44f, 0.15f });
    physics_apply_shot(pw, (float)M_PI / 2.0f, 0.6f);
    for (int i = 0; i < 120 * 6 && !physics_is_piece_pocketed(pw, 0); i++) physics_step(pw, PHYSICS_DT);
    TEST_ASSERT_TRUE_MESSAGE(physics_is_piece_pocketed(pw, 0), "test set-up: the coin should have been pocketed");
    physics_restore_snapshot(pw, snap);
    Vec2 pos[MAX_PIECES];
    physics_get_positions(pw, pos);
    TEST_ASSERT_FALSE(physics_is_piece_pocketed(pw, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.35f, pos[0].y);
    Vec2 v;
    TEST_ASSERT_TRUE(physics_get_piece_velocity(pw, 0, &v));   /* it has a body again */
    physics_snapshot_destroy(snap);
    physics_destroy(pw);
}

/* Shot validation must fail closed on NaN */
void test_validate_rejects_nan(void) {
    GameState game;
    game_state_init(&game, 1);
    game.turn_seat = SEAT_NORTH;
    ShotPlan plan;
    shot_plan_init(&plan);
    plan.placement = (Vec2){ 0.0f, BASELINE_Y_NORTH };
    plan.aim_angle = 1.0f;
    plan.power = 0.5f;
    TEST_ASSERT_TRUE(match_validate_shot(&game, &plan));
    plan.power = NAN;
    TEST_ASSERT_FALSE(match_validate_shot(&game, &plan));
    plan.power = 0.5f;
    plan.aim_angle = NAN;
    TEST_ASSERT_FALSE(match_validate_shot(&game, &plan));
    plan.aim_angle = 1.0f;
    plan.placement.x = NAN;
    TEST_ASSERT_FALSE(match_validate_shot(&game, &plan));
}

/* Asking for fewer than two placements must not divide by zero */
void test_legal_placements_small_request(void) {
    Vec2 out[8];
    TEST_ASSERT_EQUAL_INT(0, board_get_legal_placements(SEAT_NORTH, out, 1));
    TEST_ASSERT_EQUAL_INT(0, board_get_legal_placements(SEAT_NORTH, out, 0));
    TEST_ASSERT_TRUE(board_get_legal_placements(SEAT_NORTH, out, 8) > 4);
}

/* pcg32_random_float is in [0, 1): it used to round up to exactly 1.0 for the largest draws */
void test_random_float_below_one(void) {
    PCG32 rng;
    pcg32_init(&rng, 12345, 7);
    for (int i = 0; i < 2000000; i++) {
        float f = pcg32_random_float(&rng);
        TEST_ASSERT_TRUE(f >= 0.0f && f < 1.0f);
    }
}

/* All four players have the same skill (expert to championship level): identical accuracy for every seat */
void test_all_seats_have_equal_accuracy(void) {
    const StrategyProfile* north = strategy_for_seat(SEAT_NORTH);
    for (int s = 0; s < 4; s++) {
        const StrategyProfile* p = strategy_for_seat((Seat)s);
        TEST_ASSERT_EQUAL_FLOAT(north->aim_noise_std, p->aim_noise_std);
        TEST_ASSERT_EQUAL_FLOAT(north->power_noise_std, p->power_noise_std);
    }
    TEST_ASSERT_TRUE(north->aim_noise_std <= 0.01f);   /* half a degree of aim error at most */
    TEST_ASSERT_TRUE(north->power_noise_std <= 0.03f);
}

/* A pocketed striker is a foul: the turn passes and the player pays one coin back to the centre, where it must not overlap
 * a coin already there (the queen starts on the centre spot). */
void test_striker_pocketed_returns_a_coin(void) {
    RNGContext rng;
    rng_context_init(&rng, 5);
    MatchState match;
    match_state_init(&match);
    GameState game;
    game_state_init(&game, 5);
    match_start_board(&match, &game, &rng);
    TEST_ASSERT_EQUAL_INT(SEAT_NORTH, game.turn_seat);
    ShotResult res;
    shot_result_init(&res);
    res.pocketed_count = 1;
    res.pocketed_ids[0] = 0;   /* a white coin: north plays white */
    res.pocketed_colors[0] = PIECE_WHITE;
    res.striker_pocketed = true;
    for (int i = 0; i < MAX_PIECES; i++) res.final_positions[i] = game.board.pieces[i].position;
    board_apply_shot_positions(&game.board, &res);
    ShotFacts facts;
    match_extract_facts(&game, &res, &facts);
    TEST_ASSERT_TRUE(facts.fouls & FOUL_STRIKER_POCKETED);
    RulesOutcome out = rules_resolve(&match, &game, &facts);
    const BoardState* b = &out.next_game_state.board;
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, out.turn_decision);
    TEST_ASSERT_EQUAL_INT(SEAT_EAST, out.next_game_state.turn_seat);
    TEST_ASSERT_TRUE(b->pieces[0].on_board);
    TEST_ASSERT_FALSE(b->pieces[0].pocketed);
    TEST_ASSERT_EQUAL_INT(9, b->white_on_board);
    TEST_ASSERT_EQUAL_INT(0, out.next_game_state.scores.white);
    for (int i = 1; i < MAX_PIECES; i++) {
        if (!b->pieces[i].on_board) continue;
        float d = hypotf(b->pieces[i].position.x - b->pieces[0].position.x, b->pieces[i].position.y - b->pieces[0].position.y);
        TEST_ASSERT_TRUE_MESSAGE(d >= 2.0f * PIECE_RADIUS_NORM, "the coin paid back overlaps another coin");
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scratch_world_has_no_phantom_coins);
    RUN_TEST(test_scratch_world_clock_starts_at_zero);
    RUN_TEST(test_restore_recreates_a_pocketed_coin);
    RUN_TEST(test_validate_rejects_nan);
    RUN_TEST(test_legal_placements_small_request);
    RUN_TEST(test_random_float_below_one);
    RUN_TEST(test_all_seats_have_equal_accuracy);
    RUN_TEST(test_striker_pocketed_returns_a_coin);
    return UNITY_END();
}
