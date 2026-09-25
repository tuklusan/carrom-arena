#include "unity.h"
#include "common/types.h"
#include "common/rng.h"
#include "game/match.h"
#include "game/board.h"

void setUp(void) {}
void tearDown(void) {}

/* ICF 43/49: whoever breaks a board plays the white coins on it; the break passes round the table */
void test_breaker_always_plays_white_and_colours_alternate(void) {
    const Seat breaker[4] = { SEAT_NORTH, SEAT_EAST, SEAT_SOUTH, SEAT_WEST };
    const bool swapped[4] = { false, true, false, true };
    for (int boards = 0; boards < 4; boards++) {
        MatchState match;
        match_state_init(&match);
        match.boards_won_white = (uint8_t)boards;   /* total boards played so far */
        GameState game;
        game_state_init(&game, 5);
        RNGContext rng;
        rng_context_init(&rng, 5);
        match_start_board(&match, &game, &rng);
        TEST_ASSERT_EQUAL_INT(breaker[boards], game.turn_seat);
        TEST_ASSERT_EQUAL_INT(swapped[boards], game.board.seats_swapped ? 1 : 0);
        TEST_ASSERT_EQUAL_INT(TEAM_WHITE, game.active_player.team);   /* the breaker holds white */
        TEST_ASSERT_EQUAL_INT(TEAM_WHITE, board_team_of_seat(&game.board, breaker[boards]));
        /* the partner shares the colour, the opponents have the other */
        Seat partner = (Seat)((breaker[boards] + 2) % 4), opp = (Seat)((breaker[boards] + 1) % 4);
        TEST_ASSERT_EQUAL_INT(TEAM_WHITE, board_team_of_seat(&game.board, partner));
        TEST_ASSERT_EQUAL_INT(TEAM_BLACK, board_team_of_seat(&game.board, opp));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_breaker_always_plays_white_and_colours_alternate);
    return UNITY_END();
}
