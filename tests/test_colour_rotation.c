/*
 * Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs.
 * Licensed under the SANYALnet Labs Non-Commercial License; see LICENSE.
 */

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

/* Real carrom decides the first breaker with a toss (RESUME.md previously noted this game always started
   north deterministically instead). match_randomize_first_breaker() is opt-in: match_state_init() alone must
   leave the old deterministic behaviour untouched (break_offset 0, north breaks board 1) - the test above
   already covers that implicitly by never calling it - and a drawn offset must correctly shift board 1's
   breaker by that many seats round the table. */
void test_arena_kickoff_coin_toss_can_pick_any_seat_to_break_first(void) {
    MatchState match;
    match_state_init(&match);
    TEST_ASSERT_EQUAL_INT(0, match.break_offset);   /* untouched: still the old deterministic default */

    const Seat expected_breaker[4] = { SEAT_NORTH, SEAT_EAST, SEAT_SOUTH, SEAT_WEST };
    const bool expected_swapped[4] = { false, true, false, true };
    bool saw_offset[4] = { false, false, false, false };

    /* Try enough distinct seeds that a genuinely-drawing toss should land on every one of the 4 seats at least
       once; a toss that were secretly hardcoded to one outcome would fail the coverage check below. */
    for (uint64_t seed = 1; seed <= 40; seed++) {
        RNGContext rng;
        rng_context_init(&rng, seed);
        MatchState m;
        match_state_init(&m);
        match_randomize_first_breaker(&m, &rng);
        TEST_ASSERT_TRUE(m.break_offset <= 3);
        saw_offset[m.break_offset] = true;

        GameState game;
        game_state_init(&game, seed);
        match_start_board(&m, &game, &rng);
        TEST_ASSERT_EQUAL_INT(expected_breaker[m.break_offset], game.turn_seat);
        TEST_ASSERT_EQUAL_INT(expected_swapped[m.break_offset], game.board.seats_swapped ? 1 : 0);
        TEST_ASSERT_EQUAL_INT(TEAM_WHITE, board_team_of_seat(&game.board, game.turn_seat));
    }
    for (int i = 0; i < 4; i++) TEST_ASSERT_TRUE(saw_offset[i]);

    /* The toss must not perturb board_setup_initial_formation's own draw from the SAME RNGContext's global
       stream - a fresh, unrandomized match and a randomized one (same seed) must lay out board 1 identically
       except for who breaks it, or every existing formation-rotation test would start failing under real play. */
    RNGContext rng_plain, rng_tossed;
    rng_context_init(&rng_plain, 7);
    rng_context_init(&rng_tossed, 7);
    MatchState m_plain, m_tossed;
    match_state_init(&m_plain);
    match_state_init(&m_tossed);
    match_randomize_first_breaker(&m_tossed, &rng_tossed);
    GameState g_plain, g_tossed;
    game_state_init(&g_plain, 7);
    game_state_init(&g_tossed, 7);
    match_start_board(&m_plain, &g_plain, &rng_plain);
    match_start_board(&m_tossed, &g_tossed, &rng_tossed);
    for (int i = 0; i < MAX_PIECES; i++) {
        TEST_ASSERT_EQUAL_FLOAT(g_plain.board.pieces[i].position.x, g_tossed.board.pieces[i].position.x);
        TEST_ASSERT_EQUAL_FLOAT(g_plain.board.pieces[i].position.y, g_tossed.board.pieces[i].position.y);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_breaker_always_plays_white_and_colours_alternate);
    RUN_TEST(test_arena_kickoff_coin_toss_can_pick_any_seat_to_break_first);
    return UNITY_END();
}
