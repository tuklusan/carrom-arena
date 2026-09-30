/*
 * Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs.
 * Licensed under the SANYALnet Labs Non-Commercial License; see LICENSE.
 */

#include "unity.h"
#include "common/types.h"
#include "common/rng.h"
#include "common/strategy_profiles.h"
#include "ai/controller.h"
#include "ai/shot_candidates.h"
#include "ai/shot_evaluator.h"
#include "game/rules.h"
#include "game/board.h"
#include "physics/physics.h"
#include "physics/physics_snapshot.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_baseline_controller_creates(void) {
    RNGContext rng;
    rng_context_init(&rng, 12345);
    
    Controller* ctrl = baseline_controller_create(SEAT_NORTH, &STRATEGY_PROFILES[STRATEGY_BALANCED], &rng.streams[SEAT_NORTH]);
    TEST_ASSERT_NOT_NULL(ctrl);
    
    controller_destroy(ctrl);
}

void test_arena_controller_creates(void) {
    RNGContext rng;
    rng_context_init(&rng, 12345);
    
    Controller* ctrl = arena_controller_create(SEAT_NORTH, &STRATEGY_PROFILES[STRATEGY_AGGRESSIVE], &rng.streams[SEAT_NORTH]);
    TEST_ASSERT_NOT_NULL(ctrl);
    
    controller_destroy(ctrl);
}

void test_shot_candidates_placements(void) {
    Vec2 placements[8];
    int count = shot_candidates_placements(SEAT_NORTH, placements, 8);
    
    TEST_ASSERT_TRUE(count > 0);
    TEST_ASSERT_TRUE(count <= 8);
    
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_TRUE(board_is_legal_placement(SEAT_NORTH, placements[i]));
    }
}

void test_shot_candidates_generate(void) {
    RNGContext rng;
    rng_context_init(&rng, 12345);
    
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, &rng);
    
    PhysicsWorld* pw = physics_create();
    PhysicsSnapshot* snap = physics_snapshot_create(pw);
    
    DecisionSnapshot dsnap = {
        .match = &match,
        .game = &game,
        .board = &game.board,
        .physics = snap,
        .active_seat = SEAT_NORTH
    };
    
    ShotCandidate candidates[320];
    int count = shot_candidates_generate(&dsnap, candidates, 320, &rng.streams[SEAT_NORTH]);
    
    TEST_ASSERT_TRUE(count > 0);
    TEST_ASSERT_TRUE(count <= 320);
    
    // All candidates should have valid plans
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_TRUE(candidates[i].plan.power >= 0.0f && candidates[i].plan.power <= 1.0f);
        TEST_ASSERT_TRUE(candidates[i].plan.aim_angle >= -M_PI && candidates[i].plan.aim_angle <= M_PI);
        TEST_ASSERT_TRUE(board_is_legal_placement(SEAT_NORTH, candidates[i].plan.placement));
    }
    
    physics_snapshot_destroy(snap);
    physics_destroy(pw);
}

void test_controller_fallback_shot(void) {
    RNGContext rng;
    rng_context_init(&rng, 12345);
    
    Controller* ctrl = baseline_controller_create(SEAT_NORTH, &STRATEGY_PROFILES[STRATEGY_BALANCED], &rng.streams[SEAT_NORTH]);
    
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    board_state_init(&game.board);
    
    PhysicsWorld* pw = physics_create();
    PhysicsSnapshot* snap = physics_snapshot_create(pw);
    
    DecisionSnapshot dsnap = {
        .match = &match,
        .game = &game,
        .board = &game.board,
        .physics = snap,
        .active_seat = SEAT_NORTH
    };
    
    ShotPlan plan = controller_fallback_shot(ctrl, &dsnap, &rng.streams[SEAT_NORTH]);
    
    TEST_ASSERT_TRUE(board_is_legal_placement(SEAT_NORTH, plan.placement));
    TEST_ASSERT_TRUE(plan.power >= 0.0f && plan.power <= 1.0f);
    
    controller_destroy(ctrl);
    physics_snapshot_destroy(snap);
    physics_destroy(pw);
}

void test_ai_rng_isolation(void) {
    RNGContext rng;
    rng_context_init(&rng, 42);
    
    // Save initial state
    RNGSnapshot initial = rng_snapshot(&rng.streams[SEAT_NORTH]);
    
    // Create controller and generate candidates
    Controller* ctrl = arena_controller_create(SEAT_NORTH, &STRATEGY_PROFILES[STRATEGY_AGGRESSIVE], &rng.streams[SEAT_NORTH]);
    
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, &rng);
    
    PhysicsWorld* pw = physics_create();
    PhysicsSnapshot* snap = physics_snapshot_create(pw);
    
    DecisionSnapshot dsnap = {
        .match = &match,
        .game = &game,
        .board = &game.board,
        .physics = snap,
        .active_seat = SEAT_NORTH
    };
    
    // Call decide (this should restore RNG after planning)
    (void)ctrl->decide(ctrl, &dsnap, &rng.streams[SEAT_NORTH]);
    
    // RNG should be restored to pre-planning state
    TEST_ASSERT_EQUAL_UINT64(initial.state, rng.streams[SEAT_NORTH].state);
    TEST_ASSERT_EQUAL_UINT64(initial.inc, rng.streams[SEAT_NORTH].inc);
    
    controller_destroy(ctrl);
    physics_snapshot_destroy(snap);
    physics_destroy(pw);
}

/* The break must not be the same stroke every game: with the same formation, different seeds give different opening strokes */
void test_ai_break_stroke_varies_with_seed(void) {
    float aims[8];
    for (int k = 0; k < 8; k++) {
        RNGContext rng;
        rng_context_init(&rng, 500 + (uint64_t)k);
        Controller* ctrl = arena_controller_create(SEAT_NORTH, &STRATEGY_PROFILES[STRATEGY_AGGRESSIVE], &rng.streams[SEAT_NORTH]);
        MatchState match;
        match_state_init(&match);
        GameState game;
        game_state_init(&game, 12345);
        game.turn_seat = SEAT_NORTH;
        board_state_init(&game.board);
        board_setup_initial_formation(&game.board, NULL);   /* the same formation for every seed */
        TEST_ASSERT_FALSE(game.board.break_made);
        PhysicsWorld* pw = physics_create();
        PhysicsSnapshot* snap = physics_snapshot_create(pw);
        DecisionSnapshot dsnap = { .match = &match, .game = &game, .board = &game.board, .physics = snap, .active_seat = SEAT_NORTH };
        ShotPlan plan = ctrl->decide(ctrl, &dsnap, &rng.streams[SEAT_NORTH]);
        aims[k] = plan.aim_angle;
        controller_destroy(ctrl);
        physics_snapshot_destroy(snap);
        physics_destroy(pw);
    }
    int distinct = 0;
    for (int k = 1; k < 8; k++) if (fabsf(aims[k] - aims[0]) > 1e-3f) distinct++;
    TEST_ASSERT_TRUE_MESSAGE(distinct >= 5, "the break stroke must differ from seed to seed");
}

/* An uncovered queen must not be scored as a fraction of a plain coin pocket: pocketing anything
 * (queen included) earns another shot (ICF 49), so "cover it next turn" is the normal, low-risk
 * continuation of taking the queen now, not a separate harder plan a one-shot-ahead evaluator can't
 * see. Regression test for a real reported symptom: the robots would leave an available queen on the
 * board turn after turn (or cover an available cover piece before an equally available queen) because
 * a myopic per-shot score was comparing the old reduced queen-alone value against a plain coin pocket
 * and always preferring the coin. */
void test_shot_evaluator_scoring(void) {
    const StrategyProfile* profile = &STRATEGY_PROFILES[STRATEGY_BALANCED];

    BoardState board;
    board_state_init(&board);
    board.white_had_pocketed = true;   // already has the right to the queen (ICF 92, 95a/b)
    board.white_dues = 0;
    board.queen_state = QUEEN_STATE_ON_BOARD;

    // Queen pocketed alone (no own coin in the same shot): due, but must not be worth a fraction of
    // weight_queen - only the separate cover bonus is conditional on covering, not the queen's own value.
    ShotResult uncovered;
    shot_result_init(&uncovered);
    uncovered.pocketed_ids[0] = 0;
    uncovered.pocketed_colors[0] = PIECE_QUEEN;
    uncovered.pocketed_count = 1;
    uncovered.queen_pocketed = true;
    TEST_ASSERT_EQUAL_FLOAT(profile->weight_queen, score_queen_value(&uncovered, &board, TEAM_WHITE, profile));

    // Queen pocketed AND covered in the same shot: still the best outcome, full queen value plus the
    // cover bonus on top - strictly better than taking the queen alone, as it should be.
    ShotResult covered;
    shot_result_init(&covered);
    covered.pocketed_ids[0] = 0;
    covered.pocketed_colors[0] = PIECE_QUEEN;
    covered.pocketed_ids[1] = 1;
    covered.pocketed_colors[1] = PIECE_WHITE;
    covered.pocketed_count = 2;
    covered.queen_pocketed = true;
    float covered_score = score_queen_value(&covered, &board, TEAM_WHITE, profile);
    TEST_ASSERT_EQUAL_FLOAT(profile->weight_queen + profile->weight_cover, covered_score);
    TEST_ASSERT_TRUE(covered_score > profile->weight_queen);

    // No right to the queen yet (never pocketed an own coin) and none pocketed in this shot either:
    // ICF 92/95a/b says it goes straight back and the turn is lost - must stay a hard penalty.
    BoardState no_right = board;
    no_right.white_had_pocketed = false;
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, score_queen_value(&uncovered, &no_right, TEAM_WHITE, profile));

    // An uncovered queen-alone shot must never score below a plain single-coin pocket - that
    // comparison is exactly what made the robots avoid an available queen.
    ShotResult plain_coin;
    shot_result_init(&plain_coin);
    plain_coin.pocketed_ids[0] = 0;
    plain_coin.pocketed_colors[0] = PIECE_WHITE;
    plain_coin.pocketed_count = 1;
    float queen_alone_total = score_pocket_value(&uncovered, TEAM_WHITE, profile)
        + score_queen_value(&uncovered, &board, TEAM_WHITE, profile);
    float plain_coin_total = score_pocket_value(&plain_coin, TEAM_WHITE, profile)
        + score_queen_value(&plain_coin, &board, TEAM_WHITE, profile);
    TEST_ASSERT_TRUE(queen_alone_total >= plain_coin_total);
}

void test_candidate_budget_limit(void) {
    RNGContext rng;
    rng_context_init(&rng, 12345);
    
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, &rng);
    
    PhysicsWorld* pw = physics_create();
    PhysicsSnapshot* snap = physics_snapshot_create(pw);
    
    DecisionSnapshot dsnap = {
        .match = &match,
        .game = &game,
        .board = &game.board,
        .physics = snap,
        .active_seat = SEAT_NORTH
    };
    
    ShotCandidate candidates[640];  // Double budget
    int count = shot_candidates_generate(&dsnap, candidates, 320, &rng.streams[SEAT_NORTH]);
    
    // Should respect MAX_CANDIDATES limit
    TEST_ASSERT_LESS_OR_EQUAL(320, count);
    
    physics_snapshot_destroy(snap);
    physics_destroy(pw);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_baseline_controller_creates);
    RUN_TEST(test_arena_controller_creates);
    RUN_TEST(test_ai_break_stroke_varies_with_seed);
    RUN_TEST(test_shot_candidates_placements);
    RUN_TEST(test_shot_candidates_generate);
    RUN_TEST(test_controller_fallback_shot);
    RUN_TEST(test_ai_rng_isolation);
    RUN_TEST(test_shot_evaluator_scoring);
    RUN_TEST(test_candidate_budget_limit);
    
    return UNITY_END();
}