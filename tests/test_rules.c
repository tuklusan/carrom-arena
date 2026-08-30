#include "unity.h"
#include "common/types.h"
#include "game/rules.h"
#include "game/board.h"
#include "game/scoring.h"
#include "game/match.h"
#include "common/rng.h"
#include <string.h>
#include <math.h>

/* -----------------------------------------------------------------------------
 * Rules Engine Unit Tests (Article 16.1 coverage)
 * --------------------------------------------------------------------------- */

void setUp(void) {}
void tearDown(void) {}

void test_rules_basic_pocket_white(void) {
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    game.active_player.team = TEAM_WHITE;
    game.active_player.seat = SEAT_NORTH;
    board_state_init(&game.board);
    
    // White pockets one white piece
    ShotFacts facts = {0};
    facts.active_seat = SEAT_NORTH;
    facts.pocketed_ids[0] = 0;
    facts.pocketed_colors[0] = PIECE_WHITE;
    facts.pocketed_count = 1;
    facts.queen_pocketed = false;
    facts.striker_pocketed = false;
    facts.fouls = FOUL_NONE;
    facts.queen_state = QUEEN_STATE_ON_BOARD;
    facts.white_dues = 0;
    facts.black_dues = 0;
    facts.queen_dues = 0;
    
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    outcome = rules_resolve(&match, &game, &facts);
    
    // White should score 1 point
    TEST_ASSERT_EQUAL(1, outcome.score_delta.white);
    TEST_ASSERT_EQUAL(0, outcome.score_delta.black);
    
    // Turn should continue (pocketed own piece)
    TEST_ASSERT_EQUAL(TURN_CONTINUE, outcome.turn_decision);
}

void test_rules_basic_pocket_black(void) {
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_EAST;
    game.active_player.team = TEAM_BLACK;
    game.active_player.seat = SEAT_EAST;
    board_state_init(&game.board);
    
    ShotFacts facts = {0};
    facts.active_seat = SEAT_EAST;
    facts.pocketed_ids[0] = 9;
    facts.pocketed_colors[0] = PIECE_BLACK;
    facts.pocketed_count = 1;
    facts.queen_pocketed = false;
    facts.striker_pocketed = false;
    facts.fouls = FOUL_NONE;
    facts.queen_state = QUEEN_STATE_ON_BOARD;
    
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    outcome = rules_resolve(&match, &game, &facts);
    
    TEST_ASSERT_EQUAL(1, outcome.score_delta.black);
    TEST_ASSERT_EQUAL(0, outcome.score_delta.white);
    TEST_ASSERT_EQUAL(TURN_CONTINUE, outcome.turn_decision);
}

void test_rules_striker_foul(void) {
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    game.active_player.team = TEAM_WHITE;
    board_state_init(&game.board);
    
    ShotFacts facts = {0};
    facts.active_seat = SEAT_NORTH;
    facts.striker_pocketed = true;
    facts.fouls = FOUL_STRIKER_POCKETED;
    facts.queen_state = QUEEN_STATE_ON_BOARD;
    
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    outcome = rules_resolve(&match, &game, &facts);
    
    // Turn should advance (foul)
    TEST_ASSERT_EQUAL(TURN_ADVANCE, outcome.turn_decision);
    
    // Due piece for white
    TEST_ASSERT_EQUAL(1, outcome.due_actions_white);
}

void test_rules_queen_covered(void) {
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    game.active_player.team = TEAM_WHITE;
    board_state_init(&game.board);
    game.board.queen_state = QUEEN_STATE_ON_BOARD;
    
    ShotFacts facts = {0};
    facts.active_seat = SEAT_NORTH;
    facts.pocketed_ids[0] = QUEEN_ID;
    facts.pocketed_colors[0] = PIECE_QUEEN;
    facts.pocketed_ids[1] = 0;
    facts.pocketed_colors[1] = PIECE_WHITE;
    facts.pocketed_count = 2;
    facts.queen_pocketed = true;
    facts.striker_pocketed = false;
    facts.fouls = FOUL_NONE;
    facts.queen_state = QUEEN_STATE_ON_BOARD;
    
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    outcome = rules_resolve(&match, &game, &facts);
    
    // Queen covered = 3 points for white
    TEST_ASSERT_EQUAL(4, outcome.score_delta.white);  // 1 for piece + 3 for queen
    TEST_ASSERT_EQUAL(0, outcome.score_delta.black);
    TEST_ASSERT_EQUAL(QUEEN_STATE_COVERED, outcome.next_game_state.board.queen_state);
}

void test_rules_queen_not_covered(void) {
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    game.active_player.team = TEAM_WHITE;
    board_state_init(&game.board);
    
    ShotFacts facts = {0};
    facts.active_seat = SEAT_NORTH;
    facts.pocketed_ids[0] = QUEEN_ID;
    facts.pocketed_colors[0] = PIECE_QUEEN;
    facts.pocketed_count = 1;
    facts.queen_pocketed = true;
    facts.striker_pocketed = false;
    facts.fouls = FOUL_NONE;
    facts.queen_state = QUEEN_STATE_ON_BOARD;
    
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    outcome = rules_resolve(&match, &game, &facts);
    
    // Queen pocketed but not covered - goes to due
    TEST_ASSERT_EQUAL(0, outcome.score_delta.white);  // No points for uncovered queen
    TEST_ASSERT_EQUAL(QUEEN_STATE_POCKETED_NO_COVER, outcome.next_game_state.board.queen_state);
    TEST_ASSERT_EQUAL(1, outcome.next_game_state.board.queen_dues);
}

void test_rules_turn_advance_no_pocket(void) {
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    game.active_player.team = TEAM_WHITE;
    board_state_init(&game.board);
    
    ShotFacts facts = {0};
    facts.active_seat = SEAT_NORTH;
    facts.pocketed_count = 0;
    facts.queen_pocketed = false;
    facts.striker_pocketed = false;
    facts.fouls = FOUL_NONE;
    facts.queen_state = QUEEN_STATE_ON_BOARD;
    
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    outcome = rules_resolve(&match, &game, &facts);
    
    TEST_ASSERT_EQUAL(TURN_ADVANCE, outcome.turn_decision);
    TEST_ASSERT_EQUAL(SEAT_EAST, outcome.next_game_state.turn_seat);
}

void test_rules_board_over(void) {
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    game.active_player.team = TEAM_WHITE;
    board_state_init(&game.board);
    
    // All white pieces pocketed - White wins board
    game.board.white_on_board = 0;
    game.board.black_on_board = 5;
    
    ShotFacts facts = {0};
    facts.active_seat = SEAT_NORTH;
    facts.pocketed_count = 0;
    facts.queen_pocketed = false;
    facts.striker_pocketed = false;
    facts.fouls = FOUL_NONE;
    facts.queen_state = QUEEN_STATE_ON_BOARD;
    
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    outcome = rules_resolve(&match, &game, &facts);
    
    TEST_ASSERT_EQUAL(TURN_BOARD_OVER, outcome.turn_decision);
    TEST_ASSERT_EQUAL(1, outcome.next_match_state.boards_won_white);
}

void test_rules_game_over(void) {
    MatchState match;
    match_state_init(&match);
    match.boards_won_white = 7;
    match.boards_won_black = 3;
    match.target_boards_per_game = 8;
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, TEAM_WHITE);  // Properly set up board
    game.board.white_on_board = 0;  // White has pocketed all pieces
    
    ShotFacts facts = {0};
    facts.active_seat = SEAT_NORTH;
    facts.pocketed_count = 0;
    facts.queen_pocketed = false;
    facts.striker_pocketed = false;
    facts.fouls = FOUL_NONE;
    facts.queen_state = QUEEN_STATE_ON_BOARD;
    
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    outcome = rules_resolve(&match, &game, &facts);
    
    TEST_ASSERT_EQUAL(TURN_GAME_OVER, outcome.turn_decision);
    TEST_ASSERT_EQUAL(1, outcome.next_match_state.games_won_white);
}

void test_rules_match_over(void) {
    MatchState match;
    match_state_init(&match);
    match.boards_won_white = 7;  // One board away from game win (8 boards per game)
    match.games_won_white = 2;
    match.games_won_black = 1;
    match.target_boards_per_game = 8;
    match.target_games_per_match = 3;
    
    GameState game;
    game_state_init(&game, 12345);
    game.turn_seat = SEAT_NORTH;
    board_state_init(&game.board);
    board_setup_initial_formation(&game.board, TEAM_WHITE);  // Properly set up board
    game.board.white_on_board = 0;  // White has pocketed all pieces
    
    ShotFacts facts = {0};
    facts.active_seat = SEAT_NORTH;
    facts.pocketed_count = 0;
    facts.queen_pocketed = false;
    facts.striker_pocketed = false;
    facts.fouls = FOUL_NONE;
    facts.queen_state = QUEEN_STATE_ON_BOARD;
    
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    outcome = rules_resolve(&match, &game, &facts);
    
    TEST_ASSERT_EQUAL(TURN_MATCH_OVER, outcome.turn_decision);
    TEST_ASSERT_EQUAL(3, outcome.next_match_state.games_won_white);
}

void test_scoring_queen_points(void) {
    TEST_ASSERT_EQUAL(3, scoring_queen_points(true));
    TEST_ASSERT_EQUAL(0, scoring_queen_points(false));
}

void test_board_legal_placements(void) {
    Vec2 placements[8];
    int count = board_get_legal_placements(SEAT_NORTH, placements, 8);
    TEST_ASSERT_TRUE(count > 0);
    TEST_ASSERT_TRUE(count <= 8);
    
    // All placements should be on baseline
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_TRUE(board_is_legal_placement(SEAT_NORTH, placements[i]));
    }
}

void test_rng_deterministic(void) {
    PCG32 rng1, rng2;
    pcg32_init(&rng1, 12345, 1);
    pcg32_init(&rng2, 12345, 1);
    
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUAL_UINT32(pcg32_random(&rng1), pcg32_random(&rng2));
    }
}

void test_rng_stream_isolation(void) {
    RNGContext ctx1, ctx2;
    rng_context_init(&ctx1, 42);
    rng_context_init(&ctx2, 42);
    
    // All streams should match
    for (int s = 0; s < 4; s++) {
        for (int i = 0; i < 10; i++) {
            TEST_ASSERT_EQUAL_UINT64(ctx1.streams[s].state, ctx2.streams[s].state);
            TEST_ASSERT_EQUAL_UINT64(ctx1.streams[s].inc, ctx2.streams[s].inc);
            pcg32_random(&ctx1.streams[s]);
            pcg32_random(&ctx2.streams[s]);
        }
    }
    
    // Different seeds should produce different streams
    RNGContext ctx3;
    rng_context_init(&ctx3, 43);
    TEST_ASSERT_NOT_EQUAL(ctx1.streams[0].state, ctx3.streams[0].state);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_rules_basic_pocket_white);
    RUN_TEST(test_rules_basic_pocket_black);
    RUN_TEST(test_rules_striker_foul);
    RUN_TEST(test_rules_queen_covered);
    RUN_TEST(test_rules_queen_not_covered);
    RUN_TEST(test_rules_turn_advance_no_pocket);
    RUN_TEST(test_rules_board_over);
    RUN_TEST(test_rules_game_over);
    RUN_TEST(test_rules_match_over);
    RUN_TEST(test_scoring_queen_points);
    RUN_TEST(test_board_legal_placements);
    RUN_TEST(test_rng_deterministic);
    RUN_TEST(test_rng_stream_isolation);
    
    return UNITY_END();
}