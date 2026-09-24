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

void test_pocketing_integration_flow(void) {
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, NULL);
    game.turn_seat = SEAT_NORTH;
    
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &game.board, game.turn_seat);
    
    game.board.pieces[0].position = (Vec2){ POCKET_CENTERS[0].x, POCKET_CENTERS[0].y };
    game.board.pieces[0].on_board = true;
    game.board.pieces[0].color = PIECE_WHITE;
    physics_sync_from_board(pw, &game.board, game.turn_seat);
    
    physics_step(pw, PHYSICS_DT);
    
    ShotResult result;
    shot_result_init(&result);
    physics_collect_pocketed(pw, &result);
    
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
    
    int result_count = result.pocketed_count;
    int first_piece_id = (result.pocketed_count > 0) ? result.pocketed_ids[0] : -1;
    int first_piece_color = (result.pocketed_count > 0) ? result.pocketed_colors[0] : -1;
    int score_white = outcome.score_delta.white;
    int decision = (int)outcome.turn_decision;
    bool piece0_pocketed = outcome.next_game_state.board.pieces[0].pocketed;
    bool piece0_on_board = outcome.next_game_state.board.pieces[0].on_board;

    physics_destroy(pw);

    printf("Debug: result_count=%d, first_id=%d, score=%d, decision=%d, p0_pocketed=%d, p0_on_board=%d\n",
           result_count, first_piece_id, score_white, decision, piece0_pocketed, piece0_on_board);

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
    
    Vec2 pocket_pos = POCKET_CENTERS[0];
    physics_place_striker(pw, SEAT_NORTH, pocket_pos);
    
    physics_step(pw, PHYSICS_DT);
    
    ShotResult result;
    shot_result_init(&result);
    physics_collect_pocketed(pw, &result);
    
    bool striker_pocketed = result.striker_pocketed;
    physics_destroy(pw);
    
    TEST_ASSERT_TRUE(striker_pocketed);
    
    ShotFacts facts = {0};
    facts.active_seat = game.turn_seat;
    facts.striker_pocketed = striker_pocketed;
    
    RulesOutcome outcome_final = rules_resolve(&match, &game, &facts);
    int final_decision = (int)outcome_final.turn_decision;

    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, final_decision);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pocketing_integration_flow);
    RUN_TEST(test_striker_pocket_integration);
    return UNITY_END();
}
