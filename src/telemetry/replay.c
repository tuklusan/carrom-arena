#include "trace.h"
#include "common/types.h"
#include "common/rng.h"
#include "common/strategy_profiles.h"
#include "platform/platform.h"
#include "game/match.h"
#include "game/board.h"
#include "game/rules.h"
#include "physics/physics.h"
#include "ai/controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* -----------------------------------------------------------------------------
 * Trace Replay & Deterministic Validation
 * Re-runs simulation from a trace file and verifies identical results
 * --------------------------------------------------------------------------- */

static int trace_replay_shot(const char* trace_file, uint64_t shot_number, 
                             MatchState* match, GameState* game, 
                             PhysicsWorld* physics, RNGContext* rng,
                             TraceWriter* verify_trace) {
    // This is a simplified replay - in a full implementation, we would:
    // 1. Parse the trace file for the specific shot
    // 2. Set up the exact same state
    // 3. Execute the shot
    // 4. Compare results
    
    // For now, just run the normal simulation step
    return 0;
}

int trace_replay_and_verify(const char* trace_file, uint64_t seed) {
    printf("Replaying trace: %s (seed=%" PRIu64 ")\n", trace_file, seed);
    
    // Initialize fresh simulation with same seed
    RNGContext rng;
    rng_context_init(&rng, seed);
    
    MatchState match;
    GameState game;
    match_state_init(&match);
    game_state_init(&game, seed);
    
    PhysicsWorld* physics = physics_create();
    if (!physics) {
        fprintf(stderr, "Failed to create physics world\n");
        return -1;
    }
    
    // Start first board
    match_start_board(&match, &game, &rng);
    
    // Create verification trace
    TraceWriter* verify_trace = NULL;
    char verify_path[512];
    snprintf(verify_path, sizeof(verify_path), "%s.verify.jsonl", trace_file);
    verify_trace = trace_open(verify_path, ".", true, seed);
    
    // Run simulation for a few shots to verify determinism
    int max_shots = 10;
    int shots_run = 0;
    
    while (shots_run < max_shots && game.phase != PHASE_MATCH_OVER) {
        // Execute shot using current AI
        Seat seat = game.turn_seat;
        
        // Create decision snapshot
        DecisionSnapshot snap = {
            .match = &match,
            .game = &game,
            .board = &game.board,
            .physics = physics_snapshot(physics),
            .active_seat = seat
        };
        
        // Use arena controller for replay
        const StrategyProfile* profile = strategy_by_index(seat_to_strategy(seat));
        Controller* controller = arena_controller_create(seat, profile, &rng.streams[seat]);
        
        ShotPlan plan = controller_decide(controller, &snap, &rng.streams[seat]);
        controller_destroy(controller);
        
        // Validate and execute
        if (!match_validate_shot(&game, &plan)) {
            plan = controller_fallback_shot(controller, &snap, &rng.streams[seat]);
        }
        
        physics_place_striker(physics, seat, plan.placement);
        physics_apply_shot(physics, plan.aim_angle, plan.power);
        game.phase = PHASE_SHOT_EXECUTION;
        
        // Simulate until settled
        int settle_steps = 0;
        while (!physics_is_settled(physics) && settle_steps < 10000) {
            physics_step(physics, PHYSICS_DT);
            settle_steps++;
        }
        
        // Collect result
        ShotResult result;
        shot_result_init(&result);
        physics_collect_pocketed(physics, &result);
        physics_get_final_positions(physics, result.final_positions);
        result.sim_time = physics_get_sim_time(physics);
        
        // Resolve
        ShotFacts facts;
        match_extract_facts(&game, &result, &facts);
        RulesOutcome outcome = rules_resolve(&match, &game, &facts);
        
        game = outcome.next_game_state;
        match = outcome.next_match_state;
        
        // Write to verification trace
        if (verify_trace) {
            trace_write_shot_start(verify_trace, &match, &game, (uint64_t)shots_run, seat, &plan);
            trace_write_shot_end(verify_trace, &result, &outcome);
        }
        
        shots_run++;
        
        if (outcome.turn_decision == TURN_BOARD_OVER) {
            if (!match_is_over(&match)) {
                match_start_board(&match, &game, &rng);
            }
        }
    }
    
    if (verify_trace) {
        trace_close(verify_trace);
    }
    
    physics_destroy(physics);
    
    // Compare traces
    bool identical = trace_validate_determinism(trace_file, verify_path);
    
    if (identical) {
        printf("VERIFICATION PASSED: Trace matches original\n");
        return 0;
    } else {
        printf("VERIFICATION FAILED: Trace differs from original\n");
        return 1;
    }
}