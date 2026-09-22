#include "unity.h"
#include "common/types.h"
#include "game/rules.h"
#include "game/board.h"
#include "game/match.h"
#include "game/scoring.h"
#include "physics/physics.h"
#include "common/vecmath.h"
#include <stdlib.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

/* 
 * Integration test for the pocketing system flow.
 * Since we cannot easily run the full app.c (which depends on raylib) in this environment 
 * without a full X server and GPU, we simulate the app-level loop.
 */

void test_pocketing_integration_flow(void) {
    // 1. Initialize state
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, TEAM_WHITE);
    game.turn_seat = SEAT_NORTH;
    
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &game.board, game.turn_seat);
    
    // 2. Setup a scenario: Piece 0 is at pocket 0
    game.board.pieces[0].position = (Vec2){ POCKET_CENTERS[0].x, POCKET_CENTERS[0].y };
    game.board.pieces[0].on_board = true;
    game.board.pieces[0].color = PIECE_WHITE;
    physics_sync_from_board(pw, &game.board, game.turn_seat);
    
    // 3. Simulate physics step -> Pocketing
    physics_step(pw, PHYSICS_DT);
    
    // 4. Collect results (as the app would)
    ShotResult result;
    shot_result_init(&result);
    physics_collect_pocketed(pw, &result);
    
    // 5. Resolve rules (app-level HUD/Score update)
    ShotFacts facts = {0};
    facts.active_seat = game.turn_seat;
    facts.pocketed_count = result.pocketed_count;
    for(int i=0; i<result.pocketed_count; i++) {
        facts.pocketed_ids[i] = result.pocketed_ids[i];
        facts.pocketed_colors[i] = result.pocketed_colors[i];
    }
    facts.striker_pocketed = result.striker_pocketed;
    facts.queen_pocketed = result.queen_pocketed;
    facts.fouls = FOUL_NONE;
    
    RulesOutcome outcome = rules_resolve(&match, &game, &facts);
    
    // Extract values for assertions and destroy physics world
    int result_count = result.pocketed_count;
    int first_piece_id = (result.pocketed_count > 0) ? result.pocketed_ids[0] : -1;
    int first_piece_color = (result.pocketed_count > 0) ? result.pocketed_colors[0] : -1;
    int score_white = outcome.score_delta.white;
    int decision = outcome.turn_decision;
    bool piece0_pocketed = outcome.next_game_state.board.pieces[0].pocketed;
    bool piece0_on_board = outcome.next_game_state.board.pieces[0].on_board;

    physics_destroy(pw);

    // Assertions
    TEST_ASSERT_EQUAL_INT(1, result_count);
    TEST_ASSERT_EQUAL_INT(0, first_piece_id);
    TEST_ASSERT_EQUAL_INT(PIECE_WHITE, first_piece_color);
    TEST_ASSERT_EQUAL_INT(1, score_white);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, decision);
    TEST_ASSERT_TRUE(piece0_pocketed);
    TEST_ASSERT_FALSE(piece0_on_board);
}





void test_striker_pocket_integration(void) {
    MatchState match;
    match_state_init(&match);
    GameState game;
    game_state_init(&game, 12345);
    board_state_init(&game.board);
    game.turn_seat = SEAT_NORTH;
    
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &game.board, game.turn_seat);
    
    // Place striker in pocket 0
    Vec2 pocket_pos = POCKET_CENTERS[0];
    physics_place_striker(pw, SEAT_NORTH, pocket_pos);
    
    physics_step(pw, PHYSICS_DT);
    
    ShotResult result;
    shot_result_init(&result);
    physics_collect_pocketed(pw, &result);
    
    TEST_ASSERT_TRUE(result.striker_pocketed);
    
    ShotFacts facts = {0};
    facts.active_seat = game.turn_seat;
    facts.striker_pocketed = result.striker_pocketed;
    
    RulesOutcome outcome = rules_resolve(&match, &game, &facts);
    
    // Striker pocketed is a foul -> turn advances
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, outcome.turn_decision);
    
    physics_destroy(pw);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pocketing_integration_flow);
    RUN_TEST(test_striker_pocket_integration);
    return UNITY_END();
}
