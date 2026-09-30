/*
 * Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs.
 * Licensed under the SANYALnet Labs Non-Commercial License; see LICENSE.
 */

#include <stdbool.h>
#include "unity.h"
#include "common/types.h"
#include "game/board.h"
#include "game/scoring.h"

void setUp(void) {}
void tearDown(void) {}

static BoardState fresh(void) {
    BoardState b;
    board_state_init(&b);
    board_setup_initial_formation(&b, NULL);
    return b;
}

void test_start_of_board_is_zero(void) {
    BoardState b = fresh();
    TEST_ASSERT_EQUAL_INT(0, scoring_live_board_points(&b, 0));
    TEST_ASSERT_EQUAL_INT(0, scoring_live_board_points(&b, 1));
}

void test_a_pocketed_coin_counts_for_its_colour_at_once_and_stops_when_it_returns(void) {
    BoardState b = fresh();
    b.pieces[0].on_board = false;                   /* a white coin in a pocket: the red pair's colour */
    TEST_ASSERT_EQUAL_INT(1, scoring_live_board_points(&b, 0));
    TEST_ASSERT_EQUAL_INT(0, scoring_live_board_points(&b, 1));
    b.pieces[9].on_board = false;                   /* a black coin: the blue pair's */
    b.pieces[10].on_board = false;
    TEST_ASSERT_EQUAL_INT(2, scoring_live_board_points(&b, 1));
    b.pieces[0].on_board = true;                    /* the rules put the white coin back (a foul): the point goes */
    TEST_ASSERT_EQUAL_INT(0, scoring_live_board_points(&b, 0));
}

void test_swapped_board_gives_the_white_coins_to_blue(void) {
    BoardState b = fresh();
    b.seats_swapped = true;                         /* E/W broke, so they play white */
    b.pieces[0].on_board = false;
    TEST_ASSERT_EQUAL_INT(0, scoring_live_board_points(&b, 0));
    TEST_ASSERT_EQUAL_INT(1, scoring_live_board_points(&b, 1));
}

void test_covered_queen_is_three_points_for_the_covering_colour(void) {
    BoardState b = fresh();
    b.queen_state = QUEEN_STATE_COVERED;
    b.queen_covered_team = 1;                       /* covered with a white coin */
    TEST_ASSERT_EQUAL_INT(3, scoring_live_board_points(&b, 0));
    TEST_ASSERT_EQUAL_INT(0, scoring_live_board_points(&b, 1));
    b.queen_state = QUEEN_STATE_POCKETED_NO_COVER;  /* pocketed but not covered yet: nothing */
    TEST_ASSERT_EQUAL_INT(0, scoring_live_board_points(&b, 0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_start_of_board_is_zero);
    RUN_TEST(test_a_pocketed_coin_counts_for_its_colour_at_once_and_stops_when_it_returns);
    RUN_TEST(test_swapped_board_gives_the_white_coins_to_blue);
    RUN_TEST(test_covered_queen_is_three_points_for_the_covering_colour);
    return UNITY_END();
}
