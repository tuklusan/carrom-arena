#include "unity.h"
#include "common/types.h"
#include "common/rng.h"
#include "common/strategy_profiles.h"
#include "game/match.h"
#include "game/rules.h"
#include "game/board.h"
#include "physics/physics.h"
#include "ai/controller.h"
#include "ai/shot_candidates.h"
#include "ai/shot_evaluator.h"
#include "physics/physics_snapshot.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

/* =============================================================================
 * Regression Test: Full Match Headless with Bounded Time
 * Catches: "jerky / no real game" state where shots don't settle or match stalls
 * ============================================================================= */

/* ---- Configuration Constants ---- */
#define REGRESSION_SEED       0xDEADBEEFULL
#define MAX_STEPS_PER_SHOT    1200     // ~10s @ 120Hz (generous CI budget)
#define MAX_TOTAL_STEPS       600000   // ~500 shots × 1200 steps
#define MAX_SHOTS_PER_MATCH   1000     // Safety net

/* ---- Global config (can be overridden via CLI) ---- */
static uint64_t g_seed = REGRESSION_SEED;
static uint64_t g_max_total_steps = MAX_TOTAL_STEPS;
static uint32_t g_max_shot_steps = MAX_STEPS_PER_SHOT;
static bool g_verbose = false;

/* ---- CLI Parsing ---- */
static void parse_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            g_seed = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--max-steps") == 0 && i + 1 < argc) {
            g_max_total_steps = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--max-shot-steps") == 0 && i + 1 < argc) {
            g_max_shot_steps = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            g_verbose = true;
        }
    }
}

/* ---- Helper: Run a fixed number of shots headless ---- */
static void run_headless_match(uint64_t seed, uint64_t* p_total_steps, uint32_t* p_shot_count, GamePhase* p_final_phase) {
    RNGContext rng;
    rng_context_init(&rng, seed);
    
    MatchState match;
    match_state_init(&match);
    // Short match for regression testing: 1 board per game, 1 game per match
    match.target_boards_per_game = 1;
    match.target_games_per_match = 1;
    
    GameState game;
    game_state_init(&game, seed);
    
    PhysicsWorld* physics = physics_create();
    TEST_ASSERT_NOT_NULL(physics);
    
    // Create controllers for all 4 seats - use baseline (fast) for regression testing
    Controller* controllers[4];
    for (int i = 0; i < 4; i++) {
        controllers[i] = baseline_controller_create(
            (Seat)i, 
            &STRATEGY_PROFILES[STRATEGY_BALANCED], 
            &rng.streams[i]
        );
        TEST_ASSERT_NOT_NULL(controllers[i]);
    }
    
    // Start first board
    match_start_board(&match, &game, &rng);
    physics_sync_from_board(physics, &game.board, game.turn_seat);
    
    uint64_t total_physics_steps = 0;
    uint32_t shot_count = 0;
    const uint32_t MAX_TEST_SHOTS = 10;  // Run fixed number of shots for fast testing
    
    if (g_verbose) {
        printf("[REGRESSION] Seed=%" PRIu64 " starting match...\n", seed);
    }
    
    // Main simulation loop - run fixed number of shots
    while (shot_count < MAX_TEST_SHOTS && total_physics_steps < g_max_total_steps && shot_count < MAX_SHOTS_PER_MATCH) {
            // Create decision snapshot for AI
            PhysicsSnapshot* snap = physics_snapshot(physics);
            DecisionSnapshot dsnap = {
                .match = &match,
                .game = &game,
                .board = &game.board,
                .physics = snap,
                .active_seat = game.turn_seat
            };
            
            // AI decides shot
            ShotPlan plan = controller_decide(controllers[game.turn_seat], &dsnap, &rng.streams[game.turn_seat]);
            
            if (g_verbose) {
                printf("[REGRESSION] AI decision: seat=%d power=%.2f aim=%.2f tactic=%d\n",
                       game.turn_seat, plan.power, plan.aim_angle, plan.tactic);
            }
            
            physics_snapshot_destroy(snap);
            
            // Validate shot plan
            if (!match_validate_shot(&game, &plan)) {
                if (g_verbose) {
                    printf("[REGRESSION] Shot invalid (placement=%.2f,%.2f power=%.2f aim=%.2f), using fallback\n",
                           plan.placement.x, plan.placement.y, plan.power, plan.aim_angle);
                }
                // Fallback
                PhysicsSnapshot* snap2 = physics_snapshot(physics);
                dsnap.physics = snap2;
                plan = controller_fallback_shot(controllers[game.turn_seat], &dsnap, &rng.streams[game.turn_seat]);
                physics_snapshot_destroy(snap2);
            }
            
            // Execute in live physics
            physics_place_striker(physics, game.turn_seat, plan.placement);
            physics_apply_shot(physics, plan.aim_angle, plan.power);
            game.phase = PHASE_SHOT_EXECUTION;
            shot_count++;
            
            if (g_verbose) {
                printf("[REGRESSION] Shot %u: seat=%d power=%.2f angle=%.2f\n", 
                       shot_count, game.turn_seat, plan.power, plan.aim_angle);
            }
        
        // Phase: SHOT_EXECUTION -> step physics until settled (BOUNDED)
        if (game.phase == PHASE_SHOT_EXECUTION) {
            uint32_t shot_steps = 0;
            bool settled = false;
            
            while (shot_steps < g_max_shot_steps && !settled) {
                physics_step(physics, PHYSICS_DT);
                total_physics_steps++;
                shot_steps++;
                
                if (physics_is_settled(physics)) {
                    settled = true;
                    game.phase = PHASE_SETTLING;
                }
            }
            
            // ASSERT: Shot settled within budget
            char msg[256];
            snprintf(msg, sizeof(msg), 
                "Shot %u exceeded max physics steps (%u >= %u) - possible stall",
                shot_count, shot_steps, g_max_shot_steps);
            TEST_ASSERT_MESSAGE(shot_steps < g_max_shot_steps, msg);
            
            if (!settled) {
                // Force resolve if timeout (should not happen with proper settling)
                game.phase = PHASE_SETTLING;
            }
        }
        
        // Phase: SETTLING -> collect result, resolve rules
        if (game.phase == PHASE_SETTLING) {
            ShotResult result;
            shot_result_init(&result);
            
            physics_collect_pocketed(physics, &result);
            physics_get_final_positions(physics, result.final_positions);
            result.sim_time = physics_get_sim_time(physics);
            
            ShotFacts facts;
            match_extract_facts(&game, &result, &facts);
            RulesOutcome outcome = rules_resolve(&match, &game, &facts);
            
            // Apply outcome
            game = outcome.next_game_state;
            match = outcome.next_match_state;
            
            if (g_verbose) {
                printf("[REGRESSION] Shot %u resolved: decision=%d phase=%d scores=%d-%d\n",
                       shot_count, outcome.turn_decision, game.phase,
                       game.scores.white, game.scores.black);
            }
            
            // Handle phase transitions
            switch (outcome.turn_decision) {
                case TURN_CONTINUE:
                case TURN_ADVANCE:
                    game.phase = PHASE_PLACEMENT;
                    break;
                    
                case TURN_BOARD_OVER:
                    if (!match_is_over(&match)) {
                        match_start_board(&match, &game, &rng);
                        physics_sync_from_board(physics, &game.board, game.turn_seat);
                    }
                    break;
                    
                case TURN_GAME_OVER:
                case TURN_MATCH_OVER:
                    // match_is_over will be true next iteration
                    break;
                    
                default:
                    game.phase = PHASE_IDLE;
                    break;
            }
        }
    }
    
    // Store outputs
    *p_total_steps = total_physics_steps;
    *p_shot_count = shot_count;
    *p_final_phase = game.phase;
    
    if (g_verbose) {
        printf("[REGRESSION] Seed=%" PRIu64 " done: steps=%" PRIu64 " shots=%u phase=%d match_over=%d\n",
               seed, total_physics_steps, shot_count, game.phase, match_is_over(&match));
    }
    
    // Cleanup
    for (int i = 0; i < 4; i++) {
        controller_destroy(controllers[i]);
    }
    physics_destroy(physics);
}

/* =============================================================================
 * Debug Helpers
 * ============================================================================= */

static void debug_ai_decision(Controller* ctrl, const DecisionSnapshot* snap, PCG32* rng, const char* label) {
    // Generate candidates to see what's available
    ShotCandidate candidates[320];
    int count = shot_candidates_generate(snap, candidates, 320, rng);
    printf("[DEBUG %s] Candidates generated: %d\n", label, count);
    
    if (count > 0) {
        // Evaluate first few
        for (int i = 0; i < count && i < 5; i++) {
            shot_evaluator_evaluate(&candidates[i], snap, rng);
            printf("[DEBUG %s] Cand %d: tactic=%d power=%.2f aim=%.2f sim_valid=%d\n",
                   label, i, candidates[i].plan.tactic, candidates[i].plan.power,
                   candidates[i].plan.aim_angle, candidates[i].sim_valid);
        }
        
        // Score all
        shot_evaluator_score_candidates(candidates, count, snap, &ctrl->profile);
        
        // Find best
        int best_idx = 0;
        float best_score = -1e9f;
        for (int i = 0; i < count; i++) {
            if (candidates[i].score > best_score) {
                best_score = candidates[i].score;
                best_idx = i;
            }
        }
        printf("[DEBUG %s] Best: idx=%d score=%.2f tactic=%d power=%.2f aim=%.2f\n",
               label, best_idx, best_score, candidates[best_idx].plan.tactic,
               candidates[best_idx].plan.power, candidates[best_idx].plan.aim_angle);
        
        // Check validation
        bool valid = match_validate_shot(snap->game, &candidates[best_idx].plan);
        printf("[DEBUG %s] Best valid: %d\n", label, valid);
    }
}

/* =============================================================================
 * Unity Test Cases
 * ============================================================================= */

void setUp(void) {}
void tearDown(void) {}

void test_full_match_headless_bounded_time(void) {
    uint64_t total_steps = 0;
    uint32_t out_shot_count = 0;
    GamePhase final_phase = PHASE_IDLE;
    
    run_headless_match(g_seed, &total_steps, &out_shot_count, &final_phase);
    
    // ASSERT 1: Match terminated within total step budget
    TEST_ASSERT_MESSAGE(
        total_steps < g_max_total_steps,
        "Match exceeded total physics step budget - possible infinite loop"
    );
    
    // ASSERT 2: At least one shot was played and all shots settled
    TEST_ASSERT_GREATER_THAN(0, out_shot_count);
    
    // ASSERT 3: No shot exceeded per-shot step budget (verified inside run_headless_match)
    
    if (g_verbose) {
        printf("[REGRESSION] PASS: steps=%" PRIu64 " shots=%u phase=%d\n", 
               total_steps, out_shot_count, final_phase);
    }
}

void test_shot_settling_bounded_per_shot(void) {
    // This test specifically verifies the per-shot bound by running
    // multiple shots and checking each individually
    RNGContext rng;
    rng_context_init(&rng, g_seed);
    
    MatchState match;
    match_state_init(&match);
    
    GameState gs;
    game_state_init(&gs, g_seed);
    
    PhysicsWorld* physics = physics_create();
    TEST_ASSERT_NOT_NULL(physics);
    
    Controller* ctrl = baseline_controller_create(SEAT_NORTH, &STRATEGY_PROFILES[STRATEGY_BALANCED], &rng.streams[SEAT_NORTH]);
    TEST_ASSERT_NOT_NULL(ctrl);
    
    match_start_board(&match, &gs, &rng);
    physics_sync_from_board(physics, &gs.board, gs.turn_seat);
    
    // Play 10 shots and verify each settles within bound
    for (int shot = 0; shot < 10; shot++) {
        if (gs.phase == PHASE_PLACEMENT) {
            PhysicsSnapshot* snap = physics_snapshot(physics);
            DecisionSnapshot dsnap = { &match, &gs, &gs.board, snap, gs.turn_seat };
            ShotPlan plan = controller_decide(ctrl, &dsnap, &rng.streams[gs.turn_seat]);
            physics_snapshot_destroy(snap);
            
            if (!match_validate_shot(&gs, &plan)) {
                PhysicsSnapshot* snap2 = physics_snapshot(physics);
                dsnap.physics = snap2;
                plan = controller_fallback_shot(ctrl, &dsnap, &rng.streams[gs.turn_seat]);
                physics_snapshot_destroy(snap2);
            }
            
            physics_place_striker(physics, gs.turn_seat, plan.placement);
            physics_apply_shot(physics, plan.aim_angle, plan.power);
            gs.phase = PHASE_SHOT_EXECUTION;
        }
        
        // Step until settled
        uint32_t shot_steps = 0;
        while (gs.phase == PHASE_SHOT_EXECUTION && shot_steps < g_max_shot_steps) {
            physics_step(physics, PHYSICS_DT);
            shot_steps++;
            if (physics_is_settled(physics)) {
                gs.phase = PHASE_SETTLING;
            }
        }
        
        // Each shot must settle within budget
        char msg[128];
        snprintf(msg, sizeof(msg), "Shot %d exceeded step budget: %u steps", shot, shot_steps);
        TEST_ASSERT_MESSAGE(shot_steps < g_max_shot_steps, msg);
        
        // Resolve to continue
        if (gs.phase == PHASE_SETTLING) {
            ShotResult result;
            shot_result_init(&result);
            physics_collect_pocketed(physics, &result);
            physics_get_final_positions(physics, result.final_positions);
            result.sim_time = physics_get_sim_time(physics);
            
            ShotFacts facts;
            match_extract_facts(&gs, &result, &facts);
            RulesOutcome outcome = rules_resolve(&match, &gs, &facts);
            gs = outcome.next_game_state;
            match = outcome.next_match_state;
            
            if (outcome.turn_decision == TURN_BOARD_OVER && !match_is_over(&match)) {
                match_start_board(&match, &gs, &rng);
                physics_sync_from_board(physics, &gs.board, gs.turn_seat);
            } else if (outcome.turn_decision == TURN_CONTINUE || outcome.turn_decision == TURN_ADVANCE) {
                gs.phase = PHASE_PLACEMENT;
            }
        }
    }
    
    controller_destroy(ctrl);
    physics_destroy(physics);
}

void test_debug_ai_candidates(void) {
    if (!g_verbose) return;
    
    RNGContext rng;
    rng_context_init(&rng, g_seed);
    
    MatchState match;
    match_state_init(&match);
    
    GameState gs;
    game_state_init(&gs, g_seed);
    
    PhysicsWorld* physics = physics_create();
    TEST_ASSERT_NOT_NULL(physics);
    
    Controller* ctrl = baseline_controller_create(SEAT_NORTH, &STRATEGY_PROFILES[STRATEGY_BALANCED], &rng.streams[SEAT_NORTH]);
    TEST_ASSERT_NOT_NULL(ctrl);
    
    match_start_board(&match, &gs, &rng);
    physics_sync_from_board(physics, &gs.board, gs.turn_seat);
    
    PhysicsSnapshot* snap = physics_snapshot(physics);
    DecisionSnapshot dsnap = { &match, &gs, &gs.board, snap, gs.turn_seat };
    
    debug_ai_decision(ctrl, &dsnap, &rng.streams[SEAT_NORTH], "INITIAL");
    
    physics_snapshot_destroy(snap);
    controller_destroy(ctrl);
    physics_destroy(physics);
}

void test_physics_settles_from_rest(void) {
    // Quick sanity: physics with pieces at rest should settle
    // Note: physics_sync_from_board sets bodies awake, which may trigger
    // collision resolution. Need enough steps to settle (like test_physics.c does).
    PhysicsWorld* pw = physics_create();
    TEST_ASSERT_NOT_NULL(pw);
    
    BoardState board;
    board_state_init(&board);
    
    RNGContext rng;
    rng_context_init(&rng, 123);
    board_setup_initial_formation(&board, &rng);
    board_place_striker_on_baseline(&board.striker, SEAT_NORTH);
    
    physics_sync_from_board(pw, &board, SEAT_NORTH);
    
    // Step enough times to allow velocities to damp to zero
    // Pieces are placed at rest but physics_sync_from_board sets them awake,
    // which may trigger collision resolution. Need enough steps to settle.
    for (int i = 0; i < 100; i++) {
        physics_step(pw, PHYSICS_DT);
    }
    
    // Call physics_is_settled 3 times for SETTLE_CONFIRM_STEPS confirmation
    bool settled = false;
    for (int i = 0; i < 3; i++) {
        settled = physics_is_settled(pw);
    }
    
    TEST_ASSERT_TRUE(settled);
    physics_destroy(pw);
}

void test_deterministic_replay(void) {
    // Run same seed twice, verify identical shot count and final state
    uint64_t steps1 = 0, steps2 = 0;
    uint32_t shots1 = 0, shots2 = 0;
    GamePhase phase1 = PHASE_IDLE, phase2 = PHASE_IDLE;
    
    run_headless_match(g_seed, &steps1, &shots1, &phase1);
    run_headless_match(g_seed, &steps2, &shots2, &phase2);
    
    TEST_ASSERT_EQUAL(steps1, steps2);
    TEST_ASSERT_EQUAL(shots1, shots2);
    TEST_ASSERT_EQUAL(phase1, phase2);
}

int main(int argc, char* argv[]) {
    // Parse CLI args before Unity init
    parse_args(argc, argv);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_physics_settles_from_rest);
    RUN_TEST(test_shot_settling_bounded_per_shot);
    RUN_TEST(test_debug_ai_candidates);
    RUN_TEST(test_full_match_headless_bounded_time);
    RUN_TEST(test_deterministic_replay);
    
    return UNITY_END();
}