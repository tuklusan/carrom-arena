/* The ICF Laws of Carrom (reference/ICF-Carrom-Official-Rules.pdf), rule by rule. North breaks and plays the white coins
 * (ids 0-8); the black coins are 9-17; the queen is 18. Every stroke is proper. Scores are per PAIR: .white = north/south. */
#include "unity.h"
#include "common/types.h"
#include "common/rng.h"
#include "game/match.h"
#include "game/rules.h"
#include "game/board.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

static void fresh(MatchState* m, GameState* g) {
    RNGContext rng;
    rng_context_init(&rng, 3);
    match_state_init(m);
    game_state_init(g, 3);
    match_start_board(m, g, &rng);     /* North breaks and plays white */
    g->board.break_made = true;        /* the break is behind us unless a test is about the break */
}

/* the coin is in a pocket (and stays there) */
static void pocketed(GameState* g, int id) {
    PieceState* p = &g->board.pieces[id];
    if (!p->on_board) return;
    p->pocketed = true;
    p->on_board = false;
    p->pocket_index = 1;
    if (id == QUEEN_ID) { g->board.queen_on_board = false; return; }
    if (p->color == PIECE_WHITE) { g->board.white_on_board--; g->board.white_had_pocketed = true; }
    else g->board.black_on_board--;
}
/* leave exactly `own` white and `opp` black coins on the board */
static void leave(GameState* g, int own, int opp) {
    for (int i = 8; i >= own; i--) pocketed(g, i);
    for (int i = 17; i >= 9 + opp; i--) pocketed(g, i);
}
static void queen_covered_by(GameState* g, int team /* 1 white, 2 black */) {
    pocketed(g, QUEEN_ID);
    g->board.queen_state = QUEEN_STATE_COVERED;
    g->board.queen_covered_team = (uint8_t)team;
}
static void queen_pending(GameState* g, int strokes) {
    pocketed(g, QUEEN_ID);
    g->board.queen_state = QUEEN_STATE_POCKETED_NO_COVER;
    g->board.queen_pending = (uint8_t)strokes;
}

/* One stroke by the seat on the move: n_own white coins, n_opp black coins, the queen and/or the striker pocketed */
static RulesOutcome stroke(MatchState* m, GameState* g, int n_own, int n_opp, bool queen, bool striker) {
    ShotResult r;
    shot_result_init(&r);
    r.striker_touched_coin = true;
    r.striker_pocketed = striker;
    r.queen_pocketed = queen;
    int k = 0;
    Team me = board_team_of_seat(&g->board, g->turn_seat);
    int own_lo = (me == TEAM_WHITE) ? 0 : 9, opp_lo = (me == TEAM_WHITE) ? 9 : 0;
    for (int i = own_lo, c = 0; i < own_lo + 9 && c < n_own; i++)
        if (g->board.pieces[i].on_board) { r.pocketed_ids[k] = (uint8_t)i; r.pocketed_colors[k] = (uint8_t)g->board.pieces[i].color; k++; c++; }
    for (int i = opp_lo, c = 0; i < opp_lo + 9 && c < n_opp; i++)
        if (g->board.pieces[i].on_board) { r.pocketed_ids[k] = (uint8_t)i; r.pocketed_colors[k] = (uint8_t)g->board.pieces[i].color; k++; c++; }
    if (queen) { r.pocketed_ids[k] = QUEEN_ID; r.pocketed_colors[k] = PIECE_QUEEN; k++; }
    r.pocketed_count = (uint8_t)k;
    ShotFacts f;
    match_extract_facts(g, &r, &f);
    RulesOutcome o = rules_resolve(m, g, &f);
    *g = o.next_game_state;
    *m = o.next_match_state;
    return o;
}

/* ---- break: ICF 44, 45 ---- */
void test_icf_45_break_chances(void) {
    MatchState m; GameState g; fresh(&m, &g);
    g.board.break_made = false;
    ShotResult r;
    shot_result_init(&r);                      /* the striker touched nothing */
    ShotFacts f;
    for (int i = 0; i < 2; i++) {              /* 45a: two more chances after a first miss */
        match_extract_facts(&g, &r, &f);
        RulesOutcome o = rules_resolve(&m, &g, &f);
        TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision);
        TEST_ASSERT_EQUAL_INT(SEAT_NORTH, o.next_game_state.turn_seat);
        g = o.next_game_state;
    }
    match_extract_facts(&g, &r, &f);
    RulesOutcome o = rules_resolve(&m, &g, &f);   /* 45b: the right to break is lost, the turn passes */
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);
    TEST_ASSERT_EQUAL_INT(SEAT_EAST, o.next_game_state.turn_seat);
    TEST_ASSERT_FALSE(o.next_game_state.board.break_made);
    TEST_ASSERT_TRUE(o.next_game_state.board.seats_swapped == g.board.seats_swapped);   /* colours unchanged: the breaker keeps white */
}
void test_icf_44_break_made_by_touching(void) {
    MatchState m; GameState g; fresh(&m, &g);
    g.board.break_made = false;
    ShotResult r;
    shot_result_init(&r);
    r.striker_touched_coin = true;
    ShotFacts f;
    match_extract_facts(&g, &r, &f);
    RulesOutcome o = rules_resolve(&m, &g, &f);
    TEST_ASSERT_TRUE(o.next_game_state.board.break_made);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);   /* nothing pocketed: the turn passes */
}
void test_icf_45c_striker_pocketed_without_touching(void) {
    MatchState m; GameState g; fresh(&m, &g);
    g.board.break_made = false;
    ShotResult r;
    shot_result_init(&r);
    r.striker_pocketed = true;
    ShotFacts f;
    match_extract_facts(&g, &r, &f);
    RulesOutcome o = rules_resolve(&m, &g, &f);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);   /* lose the turn ... */
    TEST_ASSERT_EQUAL_INT(0, o.returned_count);             /* ... with no due and no penalty */
    TEST_ASSERT_EQUAL_INT(0, o.next_game_state.board.white_dues);
}

/* ---- turn of play: ICF 48, 74, 125 ---- */
void test_icf_48_turn_continues_on_own_coin(void) {
    MatchState m; GameState g; fresh(&m, &g);
    RulesOutcome o = stroke(&m, &g, 1, 0, false, false);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision);
    TEST_ASSERT_EQUAL_INT(8, g.board.white_on_board);
    TEST_ASSERT_EQUAL_INT(0, o.score_delta.white);   /* points are counted when the board ends (ICF 53) */
}
void test_icf_48_turn_passes_without_a_pocket(void) {
    MatchState m; GameState g; fresh(&m, &g);
    RulesOutcome o = stroke(&m, &g, 0, 0, false, false);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);
    TEST_ASSERT_EQUAL_INT(SEAT_EAST, g.turn_seat);
}
void test_icf_125_opponents_coin_alone_loses_the_turn(void) {
    MatchState m; GameState g; fresh(&m, &g);
    RulesOutcome o = stroke(&m, &g, 0, 1, false, false);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);
    TEST_ASSERT_EQUAL_INT(8, g.board.black_on_board);   /* the coin stays pocketed */
}

/* ---- striker pocketed, dues: ICF 72-75 ---- */
void test_icf_72a_striker_alone_pays_a_coin_back(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);                                    /* he had pocketed one earlier */
    RulesOutcome o = stroke(&m, &g, 0, 0, false, true);
    TEST_ASSERT_TRUE(o.foul);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);
    TEST_ASSERT_EQUAL_INT(1, o.returned_count);
    TEST_ASSERT_EQUAL_INT(9, g.board.white_on_board);
    TEST_ASSERT_EQUAL_INT(0, g.board.white_dues);
}
void test_icf_72c_due_stays_outstanding_then_is_paid(void) {
    MatchState m; GameState g; fresh(&m, &g);
    RulesOutcome o = stroke(&m, &g, 0, 0, false, true);    /* nothing of his in the pockets yet */
    TEST_ASSERT_EQUAL_INT(0, o.returned_count);
    TEST_ASSERT_EQUAL_INT(1, g.board.white_dues);
    g.turn_seat = SEAT_NORTH;                              /* back to him (four seats later, in a real game) */
    o = stroke(&m, &g, 1, 0, false, false);                /* he pockets a coin: it pays the due at once */
    TEST_ASSERT_EQUAL_INT(1, o.returned_count);
    TEST_ASSERT_EQUAL_INT(0, g.board.white_dues);
    TEST_ASSERT_EQUAL_INT(9, g.board.white_on_board);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision); /* 48: he pocketed his own coin */
}
void test_icf_73_striker_with_own_coin(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);
    RulesOutcome o = stroke(&m, &g, 1, 0, false, true);
    TEST_ASSERT_EQUAL_INT(2, o.returned_count);            /* the coin pocketed with the striker, and the due */
    TEST_ASSERT_EQUAL_INT(9, g.board.white_on_board);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision); /* he continues */
}
void test_icf_74_striker_with_opponents_coin(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);
    RulesOutcome o = stroke(&m, &g, 0, 1, false, true);
    TEST_ASSERT_EQUAL_INT(8, g.board.black_on_board);      /* the opponent's coin stays pocketed */
    TEST_ASSERT_EQUAL_INT(1, o.returned_count);            /* the due */
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);
}
void test_icf_75_striker_with_both_colours(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);
    RulesOutcome o = stroke(&m, &g, 1, 1, false, true);
    TEST_ASSERT_EQUAL_INT(2, o.returned_count);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision);
    TEST_ASSERT_EQUAL_INT(8, g.board.black_on_board);
}
void test_icf_84_89_placement_of_returned_coins(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0); pocketed(&g, 1); pocketed(&g, 2);
    RulesOutcome o = stroke(&m, &g, 0, 0, false, true);
    TEST_ASSERT_EQUAL_INT(1, o.returned_count);
    Vec2 p = o.returned[0].pos;
    float d = sqrtf(p.x * p.x + p.y * p.y);
    TEST_ASSERT_TRUE_MESSAGE(d >= CENTRE_CIRCLE_RADIUS + PIECE_RADIUS_NORM, "must not cover the centre circle (89b)");
    TEST_ASSERT_TRUE_MESSAGE(d <= OUTER_CIRCLE_RADIUS, "inside the outer circle (84a)");
    for (int i = 0; i < MAX_PIECES; i++) {
        if (i == o.returned[0].id || !g.board.pieces[i].on_board) continue;
        float dx = g.board.pieces[i].position.x - p.x, dy = g.board.pieces[i].position.y - p.y;
        TEST_ASSERT_TRUE_MESSAGE(sqrtf(dx * dx + dy * dy) >= 2.0f * PIECE_RADIUS_NORM - 0.001f, "must not disturb another coin (86)");
    }
}

/* ---- the queen: ICF 92-101 ---- */
void test_icf_95a_queen_before_any_own_coin(void) {
    MatchState m; GameState g; fresh(&m, &g);
    RulesOutcome o = stroke(&m, &g, 0, 0, true, false);
    TEST_ASSERT_TRUE(o.queen_returned);
    TEST_ASSERT_TRUE(g.board.pieces[QUEEN_ID].on_board);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);
}
void test_icf_95b_queen_with_a_due_outstanding(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);
    g.board.white_dues = 1;
    RulesOutcome o = stroke(&m, &g, 0, 0, true, false);
    TEST_ASSERT_TRUE(o.queen_returned);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);
}
void test_icf_92_96_queen_alone_then_cover_or_return(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);
    RulesOutcome o = stroke(&m, &g, 0, 0, true, false);      /* he has the right: she waits for her cover */
    TEST_ASSERT_EQUAL_INT(QUEEN_STATE_POCKETED_NO_COVER, g.board.queen_state);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision);
    MatchState m2 = m; GameState g2 = g;
    o = stroke(&m, &g, 1, 0, false, false);                  /* 15, 96: covered by an own coin on the next stroke */
    TEST_ASSERT_EQUAL_INT(QUEEN_STATE_COVERED, g.board.queen_state);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision);
    o = stroke(&m2, &g2, 0, 0, false, false);                /* 96: not covered: she goes back */
    TEST_ASSERT_TRUE(o.queen_returned);
    TEST_ASSERT_EQUAL_INT(QUEEN_STATE_ON_BOARD, g2.board.queen_state);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);
}
void test_icf_97_queen_with_own_coins(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);
    stroke(&m, &g, 1, 0, true, false);                       /* 97a: queen and a coin together: covered */
    TEST_ASSERT_EQUAL_INT(QUEEN_STATE_COVERED, g.board.queen_state);
    fresh(&m, &g);                                           /* 97b: all nine on the board */
    stroke(&m, &g, 1, 0, true, false);                       /*   queen and ONE coin: still to be covered */
    TEST_ASSERT_EQUAL_INT(QUEEN_STATE_POCKETED_NO_COVER, g.board.queen_state);
    fresh(&m, &g);
    stroke(&m, &g, 2, 0, true, false);                       /*   queen and more than one coin: covered */
    TEST_ASSERT_EQUAL_INT(QUEEN_STATE_COVERED, g.board.queen_state);
}
void test_icf_98a_queen_coin_and_striker(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);
    RulesOutcome o = stroke(&m, &g, 1, 0, true, true);
    TEST_ASSERT_TRUE(o.queen_returned);
    TEST_ASSERT_EQUAL_INT(9, g.board.white_on_board);        /* the coin, and a due coin, are taken out */
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision);
}
void test_icf_99a_and_95d_queen_and_striker(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);
    RulesOutcome o = stroke(&m, &g, 0, 0, true, true);       /* 99a: continues */
    TEST_ASSERT_TRUE(o.queen_returned);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision);
    fresh(&m, &g);                                           /* 95d: all nine on the board: loses the turn */
    o = stroke(&m, &g, 0, 0, true, true);
    TEST_ASSERT_TRUE(o.queen_returned);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);
}
void test_icf_100a_striker_alone_while_covering(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);
    queen_pending(&g, 1);
    RulesOutcome o = stroke(&m, &g, 0, 0, false, true);
    TEST_ASSERT_TRUE(o.queen_returned);
    TEST_ASSERT_EQUAL_INT(TURN_ADVANCE, o.turn_decision);
    TEST_ASSERT_EQUAL_INT(9, g.board.white_on_board);        /* the due coin went back */
}
void test_icf_101a_striker_and_coin_while_covering(void) {
    MatchState m; GameState g; fresh(&m, &g);
    pocketed(&g, 0);
    queen_pending(&g, 1);
    RulesOutcome o = stroke(&m, &g, 1, 0, false, true);
    TEST_ASSERT_EQUAL_INT(TURN_CONTINUE, o.turn_decision);   /* he continues ... */
    TEST_ASSERT_EQUAL_INT(QUEEN_STATE_POCKETED_NO_COVER, g.board.queen_state);
    o = stroke(&m, &g, 0, 0, false, false);                  /* ... and if that stroke pockets nothing of his, she goes back */
    TEST_ASSERT_TRUE(o.queen_returned);
}

/* ---- scoring and the end of a board: ICF 52-55, 102-112 ---- */
static int pts(const GameState* g, int pair) { return pair == 0 ? g->scores.white : g->scores.black; }

void test_icf_52_53_last_coin_queen_covered_by_the_winner(void) {
    MatchState m; GameState g; fresh(&m, &g);
    leave(&g, 1, 4);
    queen_covered_by(&g, 1);
    RulesOutcome o = stroke(&m, &g, 1, 0, false, false);
    TEST_ASSERT_EQUAL_INT(TURN_BOARD_OVER, o.turn_decision);
    TEST_ASSERT_EQUAL_INT(4 + 3, pts(&g, 0));                /* the opponent's coins on the board, and the queen */
    TEST_ASSERT_EQUAL_INT(1, m.boards_won_white);
}
void test_icf_53c_queen_covered_by_the_loser_scores_nothing(void) {
    MatchState m; GameState g; fresh(&m, &g);
    leave(&g, 1, 4);
    queen_covered_by(&g, 2);
    stroke(&m, &g, 1, 0, false, false);
    TEST_ASSERT_EQUAL_INT(4, pts(&g, 0));
}
void test_icf_54_no_queen_credit_from_22_points(void) {
    MatchState m; GameState g; fresh(&m, &g);
    g.scores.white = 22;
    leave(&g, 1, 4);
    queen_covered_by(&g, 1);
    stroke(&m, &g, 1, 0, false, false);
    TEST_ASSERT_EQUAL_INT(22 + 4, pts(&g, 0));
}
void test_icf_55_at_most_12_points_a_board(void) {
    MatchState m; GameState g; fresh(&m, &g);
    leave(&g, 1, 9);
    queen_covered_by(&g, 1);
    stroke(&m, &g, 1, 0, false, false);
    TEST_ASSERT_TRUE(pts(&g, 0) <= 12);
    TEST_ASSERT_EQUAL_INT(12, pts(&g, 0));
}
void test_icf_107a_last_coin_with_the_queen_on_the_board(void) {
    MatchState m; GameState g; fresh(&m, &g);
    leave(&g, 1, 5);
    stroke(&m, &g, 1, 0, false, false);
    TEST_ASSERT_EQUAL_INT(3, pts(&g, 1));                    /* he loses the board by 3 points */
    TEST_ASSERT_EQUAL_INT(0, pts(&g, 0));
    fresh(&m, &g);
    g.scores.black = 22;
    leave(&g, 1, 5);
    stroke(&m, &g, 1, 0, false, false);
    TEST_ASSERT_EQUAL_INT(22 + 1, pts(&g, 1));               /* ... by one point from 22 */
}
void test_icf_106a_opponents_last_coin_with_the_queen_on_the_board(void) {
    MatchState m; GameState g; fresh(&m, &g);
    leave(&g, 3, 1);
    stroke(&m, &g, 0, 1, false, false);
    TEST_ASSERT_EQUAL_INT(3 + 3, pts(&g, 1));                /* his coins on the board and the queen's points */
}
void test_icf_103a_opponents_last_coin_while_covering(void) {
    MatchState m; GameState g; fresh(&m, &g);
    leave(&g, 3, 1);
    queen_pending(&g, 1);
    stroke(&m, &g, 0, 1, false, false);
    TEST_ASSERT_EQUAL_INT(3 + 3, pts(&g, 1));
}
void test_icf_102a_104a_105a_both_last_coins(void) {
    MatchState m; GameState g; fresh(&m, &g);
    leave(&g, 1, 1);
    queen_pending(&g, 1);
    stroke(&m, &g, 1, 1, false, false);                      /* 102a: covering, last coins of both */
    TEST_ASSERT_EQUAL_INT(3, pts(&g, 0));
    fresh(&m, &g);
    leave(&g, 1, 1);
    stroke(&m, &g, 1, 1, true, false);                       /* 104a: with the queen */
    TEST_ASSERT_EQUAL_INT(3, pts(&g, 0));
    fresh(&m, &g);
    leave(&g, 1, 1);
    stroke(&m, &g, 1, 1, false, false);                      /* 105a: queen still on the board: the opponent gets 3 */
    TEST_ASSERT_EQUAL_INT(3, pts(&g, 1));
}
void test_icf_108_to_112_striker_with_the_last_coins(void) {
    MatchState m; GameState g; fresh(&m, &g);
    leave(&g, 1, 4);
    stroke(&m, &g, 1, 0, false, true);                       /* 108a: last own coin and the striker, queen on the board */
    TEST_ASSERT_EQUAL_INT(3 + 1, pts(&g, 1));                /* 3 points and the striker's additional point */
    fresh(&m, &g);
    leave(&g, 1, 1);
    queen_covered_by(&g, 1);
    stroke(&m, &g, 1, 1, false, true);                       /* 110a: both last coins with the striker, queen covered by him */
    TEST_ASSERT_EQUAL_INT(1 + 1, pts(&g, 1));
    fresh(&m, &g);
    leave(&g, 1, 1);
    queen_covered_by(&g, 2);
    stroke(&m, &g, 1, 1, false, true);                       /* 112a: queen covered by the opponent */
    TEST_ASSERT_EQUAL_INT(3 + 1, pts(&g, 1));
    fresh(&m, &g);
    leave(&g, 3, 1);
    stroke(&m, &g, 0, 1, false, true);                       /* 111a: the opponent's last coin and the striker, queen on the board */
    TEST_ASSERT_EQUAL_INT(3 + 3 + 1, pts(&g, 1));
}

/* ---- games and matches: ICF 56, 57 ---- */
void test_icf_56_game_is_25_points_or_eight_boards(void) {
    MatchState m; GameState g; fresh(&m, &g);
    g.scores.white = 20;
    leave(&g, 1, 4);
    queen_covered_by(&g, 1);
    RulesOutcome o = stroke(&m, &g, 1, 0, false, false);     /* 7 points: 27 */
    TEST_ASSERT_EQUAL_INT(TURN_GAME_OVER, o.turn_decision);
    TEST_ASSERT_EQUAL_INT(1, m.games_won_white);
    /* eight boards, higher score wins; a tie needs an extra board */
    fresh(&m, &g);
    m.boards_won_white = 4; m.boards_won_black = 3;          /* seven played */
    g.scores.white = 10; g.scores.black = 10;
    leave(&g, 1, 2);
    queen_covered_by(&g, 1);
    o = stroke(&m, &g, 1, 0, false, false);                  /* white wins the eighth board: 10 + 5 > 10 */
    TEST_ASSERT_EQUAL_INT(TURN_GAME_OVER, o.turn_decision);
    fresh(&m, &g);
    m.boards_won_white = 4; m.boards_won_black = 3;
    g.scores.white = 10; g.scores.black = 15;
    leave(&g, 1, 2);
    queen_covered_by(&g, 1);
    o = stroke(&m, &g, 1, 0, false, false);                  /* 15 : 15 after eight boards: an extra board */
    TEST_ASSERT_EQUAL_INT(TURN_BOARD_OVER, o.turn_decision);
}
void test_icf_57_best_of_three_games(void) {
    MatchState m; GameState g; fresh(&m, &g);
    m.games_won_white = 1;
    g.scores.white = 24;
    leave(&g, 1, 4);
    queen_covered_by(&g, 1);
    RulesOutcome o = stroke(&m, &g, 1, 0, false, false);
    TEST_ASSERT_EQUAL_INT(TURN_MATCH_OVER, o.turn_decision);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_icf_45_break_chances);
    RUN_TEST(test_icf_44_break_made_by_touching);
    RUN_TEST(test_icf_45c_striker_pocketed_without_touching);
    RUN_TEST(test_icf_48_turn_continues_on_own_coin);
    RUN_TEST(test_icf_48_turn_passes_without_a_pocket);
    RUN_TEST(test_icf_125_opponents_coin_alone_loses_the_turn);
    RUN_TEST(test_icf_72a_striker_alone_pays_a_coin_back);
    RUN_TEST(test_icf_72c_due_stays_outstanding_then_is_paid);
    RUN_TEST(test_icf_73_striker_with_own_coin);
    RUN_TEST(test_icf_74_striker_with_opponents_coin);
    RUN_TEST(test_icf_75_striker_with_both_colours);
    RUN_TEST(test_icf_84_89_placement_of_returned_coins);
    RUN_TEST(test_icf_95a_queen_before_any_own_coin);
    RUN_TEST(test_icf_95b_queen_with_a_due_outstanding);
    RUN_TEST(test_icf_92_96_queen_alone_then_cover_or_return);
    RUN_TEST(test_icf_97_queen_with_own_coins);
    RUN_TEST(test_icf_98a_queen_coin_and_striker);
    RUN_TEST(test_icf_99a_and_95d_queen_and_striker);
    RUN_TEST(test_icf_100a_striker_alone_while_covering);
    RUN_TEST(test_icf_101a_striker_and_coin_while_covering);
    RUN_TEST(test_icf_52_53_last_coin_queen_covered_by_the_winner);
    RUN_TEST(test_icf_53c_queen_covered_by_the_loser_scores_nothing);
    RUN_TEST(test_icf_54_no_queen_credit_from_22_points);
    RUN_TEST(test_icf_55_at_most_12_points_a_board);
    RUN_TEST(test_icf_107a_last_coin_with_the_queen_on_the_board);
    RUN_TEST(test_icf_106a_opponents_last_coin_with_the_queen_on_the_board);
    RUN_TEST(test_icf_103a_opponents_last_coin_while_covering);
    RUN_TEST(test_icf_102a_104a_105a_both_last_coins);
    RUN_TEST(test_icf_108_to_112_striker_with_the_last_coins);
    RUN_TEST(test_icf_56_game_is_25_points_or_eight_boards);
    RUN_TEST(test_icf_57_best_of_three_games);
    return UNITY_END();
}
