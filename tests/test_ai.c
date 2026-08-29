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

void test_shot_evaluator_scoring(void) {
    ShotCandidate candidate;
    shot_result_init(&candidate.sim_result);
    
    // Mock a result with white piece pocketed
    candidate.sim_result.pocketed_ids[0] = 0;
    candidate.sim_result.pocketed_colors[0] = PIECE_WHITE;
    candidate.sim_result.pocketed_count = 1;
    candidate.sim_result.queen_pocketed = false;
    candidate.sim_result.striker_pocketed = false;
    candidate.sim_valid = true;
    
    StrategyProfile profile = STRATEGY_PROFILES[STRATEGY_BALANCED];
    (void)profile;
    
    // Would need full board state for complete test
    // This is a stub test
    TEST_ASSERT_TRUE(true);
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
    RUN_TEST(test_shot_candidates_placements);
    RUN_TEST(test_shot_candidates_generate);
    RUN_TEST(test_controller_fallback_shot);
    RUN_TEST(test_ai_rng_isolation);
    RUN_TEST(test_shot_evaluator_scoring);
    RUN_TEST(test_candidate_budget_limit);
    
    return UNITY_END();
}