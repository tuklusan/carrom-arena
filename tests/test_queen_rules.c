#include "unity.h"
#include "common/types.h"
#include "common/rng.h"
#include "game/match.h"
#include "game/rules.h"
#include "game/board.h"

void setUp(void) {}
void tearDown(void) {}

static void fresh(MatchState* m, GameState* g) {
    RNGContext rng;
    rng_context_init(&rng, 3);
    match_state_init(m);
    game_state_init(g, 3);
    match_start_board(m, g, &rng);   /* North breaks, plays white */
}

static RulesOutcome shot(MatchState* m, GameState* g, const int* ids, const PieceColor* colors, int n, bool queen) {
    ShotResult r;
    shot_result_init(&r);
    r.pocketed_count = (uint8_t)n;
    for (int i = 0; i < n; i++) { r.pocketed_ids[i] = (uint8_t)ids[i]; r.pocketed_colors[i] = (uint8_t)colors[i]; }
    r.queen_pocketed = queen;
    ShotFacts f;
    match_extract_facts(g, &r, &f);
    return rules_resolve(m, g, &f);
}

/* queen pocketed alone: the same player gets one more stroke to cover her */
void test_queen_alone_gives_a_cover_stroke(void) {
    MatchState m; GameState g; fresh(&m, &g);
    int ids[1] = { QUEEN_ID }; PieceColor cols[1] = { PIECE_QUEEN };
    RulesOutcome o = shot(&m, &g, ids, cols, 1, true);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision);
    TEST_ASSERT_EQUAL_INT(QUEEN_STATE_POCKETED_NO_COVER, o.next_game_state.board.queen_state);
    TEST_ASSERT_FALSE(o.next_game_state.board.pieces[QUEEN_ID].on_board);
}

/* ...and if that stroke does not pocket one of his own coins, the queen goes back to the centre */
void test_queen_returns_to_centre_when_not_covered(void) {
    MatchState m; GameState g; fresh(&m, &g);
    int qid[1] = { QUEEN_ID }; PieceColor qcol[1] = { PIECE_QUEEN };
    RulesOutcome o1 = shot(&m, &g, qid, qcol, 1, true);
    g = o1.next_game_state; m = o1.next_match_state;
    RulesOutcome o2 = shot(&m, &g, NULL, NULL, 0, false);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o2.turn_decision);
    const BoardState* b = &o2.next_game_state.board;
    TEST_ASSERT_EQUAL_INT(QUEEN_STATE_ON_BOARD, b->queen_state);
    TEST_ASSERT_TRUE(b->pieces[QUEEN_ID].on_board);
    TEST_ASSERT_TRUE(b->queen_on_board);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, b->pieces[QUEEN_ID].position.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, b->pieces[QUEEN_ID].position.y);
}

void test_queen_covered_by_own_coin_on_the_next_stroke(void) {
    MatchState m; GameState g; fresh(&m, &g);
    int qid[1] = { QUEEN_ID }; PieceColor qcol[1] = { PIECE_QUEEN };
    RulesOutcome o1 = shot(&m, &g, qid, qcol, 1, true);
    g = o1.next_game_state; m = o1.next_match_state;
    int cid[1] = { 0 }; PieceColor ccol[1] = { PIECE_WHITE };
    RulesOutcome o2 = shot(&m, &g, cid, ccol, 1, false);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o2.turn_decision);
    TEST_ASSERT_EQUAL_INT(QUEEN_STATE_COVERED, o2.next_game_state.board.queen_state);
    TEST_ASSERT_TRUE(o2.score_delta.white >= 4);   /* the coin and the 3-point queen */
}

/* the board ends when a player clears his coins, even if the queen was never covered (this used to loop forever) */
void test_board_ends_when_a_player_clears_his_coins_even_with_queen_off(void) {
    MatchState m; GameState g; fresh(&m, &g);
    g.board.queen_state = QUEEN_STATE_POCKETED_NO_COVER;
    g.board.queen_on_board = false;
    g.board.pieces[QUEEN_ID].on_board = false;
    g.board.white_on_board = 1;
    int cid[1] = { 0 }; PieceColor ccol[1] = { PIECE_WHITE };
    RulesOutcome o = shot(&m, &g, cid, ccol, 1, false);
    TEST_ASSERT_EQUAL_INT(TURN_BOARD_OVER, o.turn_decision);
}

void test_returning_the_queen_removes_her_from_the_stash(void) {
    BoardState b;
    board_state_init(&b);
    /* two coins and the queen stashed in pocket 0 */
    int ids[3] = { 1, QUEEN_ID, 10 };
    for (int i = 0; i < 3; i++) {
        b.pieces[ids[i]].pocketed = true; b.pieces[ids[i]].on_board = false; b.pieces[ids[i]].pocket_index = 0;
        b.pocketed_pieces[i] = b.pieces[ids[i]];
        b.pocketed_pieces[i].pocketed_position = board_stash_position(0, i);
    }
    b.pocketed_count = 3;
    board_remove_from_stash(&b, QUEEN_ID);
    TEST_ASSERT_EQUAL_INT(2, b.pocketed_count);
    TEST_ASSERT_EQUAL_INT(1, b.pocketed_pieces[0].id);
    TEST_ASSERT_EQUAL_INT(10, b.pocketed_pieces[1].id);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, board_stash_position(0, 1).x, b.pocketed_pieces[1].pocketed_position.x);   /* gap closed */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_queen_alone_gives_a_cover_stroke);
    RUN_TEST(test_queen_returns_to_centre_when_not_covered);
    RUN_TEST(test_queen_covered_by_own_coin_on_the_next_stroke);
    RUN_TEST(test_board_ends_when_a_player_clears_his_coins_even_with_queen_off);
    RUN_TEST(test_returning_the_queen_removes_her_from_the_stash);
    return UNITY_END();
}
