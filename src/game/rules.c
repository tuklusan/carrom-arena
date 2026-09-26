#include "rules.h"
#include "common/types.h"
#include "common/rng.h"
#include "common/vecmath.h"
#include "board.h"
#include "scoring.h"
#include <string.h>
#include <stdlib.h>


/* -----------------------------------------------------------------------------
 * Pure Rules Engine Implementation
 * Article 16.1 - All rules bullets covered
 * --------------------------------------------------------------------------- */

void rules_outcome_init(RulesOutcome* outcome) {
    memset(outcome, 0, sizeof(RulesOutcome));
    outcome->turn_decision = TURN_ADVANCE;
    outcome->event_count = 0;
}

void game_state_init(GameState* game, uint64_t seed) {
    (void)seed;
    memset(game, 0, sizeof(GameState));
    game->phase = PHASE_IDLE;
    game->turn_seat = SEAT_NORTH;
    game->active_player.seat = SEAT_NORTH;
    game->active_player.team = TEAM_WHITE;
    game->active_player.strategy_index = 0;
    game->consecutive_turns = 0;
    game->scores.white = 0;
    game->scores.black = 0;
    board_state_init(&game->board);
}

void shot_plan_init(ShotPlan* plan) {
    memset(plan, 0, sizeof(ShotPlan));
    plan->placement = (Vec2){0, 0};
    plan->aim_angle = 0.0f;
    plan->power = 0.0f;
    plan->tactic = TACTIC_FALLBACK;
    plan->rng_draw = 0;
}

void shot_result_init(ShotResult* result) {
    memset(result, 0, sizeof(ShotResult));
    result->pocketed_count = 0;
    result->queen_pocketed = false;
    result->striker_pocketed = false;
    result->fouls = FOUL_NONE;
    result->sim_time = 0.0f;
    for (int i = 0; i < 19; i++) {
        result->final_positions[i] = (Vec2){0, 0};
    }
}

/* -----------------------------------------------------------------------------
 * The ICF Laws of Carrom (reference/ICF-Carrom-Official-Rules.pdf). Rule numbers in the comments are the Laws' numbers.
 *
 * Mapping to this game: the four seats are doubles (partners sit opposite: N/S against E/W). A "player" in the Laws is a
 * seat for the turn (the turn passes N -> E -> S -> W) and a PAIR for score, coins and the board result. The breaker's pair
 * has the white coins (ICF 43); the other pair the black. Every stroke is a proper stroke: the simulation has no hands, so
 * the Laws about pushing, elbows, sitting, powder, umpires and time limits cannot occur, and a coin cannot jump the
 * board (ICF 65-66, 116), so the "improper stroke" branches (72b, 76, 77, 98b, 99b, 100b, 101b, ...) never apply.
 * --------------------------------------------------------------------------- */

static int pair_of_seat(Seat s) { return (s == SEAT_NORTH || s == SEAT_SOUTH) ? 0 : 1; }
static int pair_score(const GameState* g, int pair) { return pair == 0 ? g->scores.white : g->scores.black; }
static int queen_value_3_or_1(int score) { return score >= 22 ? 1 : 3; }   /* ICF 102-112: "3 points, if the score is 22 or more 1 point" */
static int queen_credit(int score) { return score >= 22 ? 0 : 3; }        /* ICF 52b, 54: no extra 3 points from 22 */

static uint8_t* team_on_board(BoardState* b, Team t) { return (t == TEAM_WHITE) ? &b->white_on_board : &b->black_on_board; }
static uint8_t* team_dues(BoardState* b, Team t) { return (t == TEAM_WHITE) ? &b->white_dues : &b->black_dues; }
static bool* team_had_pocketed(BoardState* b, Team t) { return (t == TEAM_WHITE) ? &b->white_had_pocketed : &b->black_had_pocketed; }
static PieceColor team_colour(Team t) { return (t == TEAM_WHITE) ? PIECE_WHITE : PIECE_BLACK; }

static void push_event(RulesOutcome* o, GameEvent e) {
    if (o->event_count < 16) o->events[o->event_count++] = e;
}

static void advance_turn(GameState* game, RulesOutcome* o) {
    game->consecutive_turns = 0;
    game->turn_seat = (Seat)((game->turn_seat + 1) % 4);
    game->active_player.seat = game->turn_seat;
    game->active_player.team = board_team_of_seat(&game->board, game->turn_seat);
    o->turn_decision = TURN_ADVANCE;
    push_event(o, (GameEvent){ .type = EVENT_TURN_CHANGE, .tick = game->consecutive_turns, .seat = game->turn_seat,
                               .team = game->active_player.team, .turn_decision = TURN_ADVANCE });
}

/* which pocket a coin fell into (the animation slides it back from there); nearest pocket to `to` when unknown */
static uint8_t coin_pocket(const BoardState* b, int id, Vec2 to) {
    uint8_t pk = b->pieces[id].pocket_index;
    if (pk <= 3) return pk;
    int best = 0;
    float bd = 1e9f;
    for (int p = 0; p < 4; p++) {
        float dx = POCKET_CENTERS[p].x - to.x, dy = POCKET_CENTERS[p].y - to.y;
        if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = p; }
    }
    return (uint8_t)best;
}

/* put a pocketed coin (or the queen) back on the board at `pos`, and record it for the animation */
static void put_back(GameState* game, RulesOutcome* o, int id, Vec2 pos) {
    BoardState* b = &game->board;
    uint8_t from = coin_pocket(b, id, pos);
    PieceState* pc = &b->pieces[id];
    pc->pocketed = false;
    pc->on_board = true;
    pc->position = pos;
    pc->velocity = (Vec2){ 0.0f, 0.0f };
    board_remove_from_stash(b, id);
    if (o->returned_count < MAX_PIECES) {
        o->returned[o->returned_count++] = (ReturnedCoin){ (uint8_t)id, from, pos };
    }
}

/* A finished board: the winner's pair gets `points` (ICF 52-55: at most 12 a board, dues and penalties written off). */
static void finish_board(const MatchState* prior_match, MatchState* match, GameState* game, RulesOutcome* o, int winner_pair, int points) {
    (void)prior_match;
    if (points > 12) points = 12;
    if (points < 0) points = 0;
    if (winner_pair == 0) { game->scores.white += points; o->score_delta.white += points; match->boards_won_white++; }
    else                  { game->scores.black += points; o->score_delta.black += points; match->boards_won_black++; }
    o->turn_decision = TURN_BOARD_OVER;
    game->phase = PHASE_BOARD_OVER;
    *team_dues(&game->board, TEAM_WHITE) = 0;   /* ICF 55 */
    *team_dues(&game->board, TEAM_BLACK) = 0;
    push_event(o, (GameEvent){ .type = EVENT_BOARD_END, .tick = game->consecutive_turns,
                               .team = (winner_pair == 0) ? TEAM_WHITE : TEAM_BLACK,
                               .score_delta_white = o->score_delta.white, .score_delta_black = o->score_delta.black,
                               .turn_decision = TURN_BOARD_OVER });

    /* ICF 56, 57: a game is 25 points or eight boards; a tie after eight boards is settled by an extra board; best of three games */
    int boards_played = match->boards_won_white + match->boards_won_black;
    int w = game->scores.white, k = game->scores.black;
    bool game_done = false;
    int game_winner = 0;
    if (w >= 25 || k >= 25) { game_done = true; game_winner = (w >= k) ? 0 : 1; }
    else if (boards_played >= match->target_boards_per_game && w != k) { game_done = true; game_winner = (w > k) ? 0 : 1; }
    if (game_done) {
        if (game_winner == 0) match->games_won_white++; else match->games_won_black++;
        o->turn_decision = TURN_GAME_OVER;
        game->phase = PHASE_GAME_OVER;
        push_event(o, (GameEvent){ .type = EVENT_GAME_END, .tick = game->consecutive_turns,
                                   .team = (game_winner == 0) ? TEAM_WHITE : TEAM_BLACK, .turn_decision = TURN_GAME_OVER });
        if (match->games_won_white >= match->target_games_per_match || match->games_won_black >= match->target_games_per_match) {
            o->turn_decision = TURN_MATCH_OVER;
            game->phase = PHASE_MATCH_OVER;
            push_event(o, (GameEvent){ .type = EVENT_MATCH_END, .tick = game->consecutive_turns,
                                       .team = (match->games_won_white >= match->target_games_per_match) ? TEAM_WHITE : TEAM_BLACK,
                                       .turn_decision = TURN_MATCH_OVER });
        }
    }
}

/* What the stroke did to the queen, and the end-of-board table (ICF 102-112) need these facts */
typedef struct {
    int n, m;                 /* own / opponent coins pocketed by this stroke */
    bool S, Q;                /* striker / queen pocketed by this stroke */
    bool own_last, opp_last;  /* the stroke pocketed the last coin of that colour */
    bool q_pending;           /* the queen was pocketed by the previous stroke and awaits its cover */
    int q_covered_by;         /* 0 nobody, 1 white, 2 black (before this stroke) */
    bool q_covered_by_me_now; /* this stroke covers her */
    int a, b;                 /* coins left on the board: own, opponent */
} StrokeFacts;

/* Returns true when the stroke ends the board; *winner_pair and *points say how. ICF 52, 53, 102-112. */
static bool board_result(const StrokeFacts* f, Team A, int pairA, const GameState* game, int* winner_pair, int* points) {
    (void)A;
    const int pairB = 1 - pairA;
    const int scoreA = pair_score(game, pairA), scoreB = pair_score(game, pairB);
    const int meCol = (A == TEAM_WHITE) ? 1 : 2;
    const bool qcovA = (f->q_covered_by == meCol) || f->q_covered_by_me_now;
    const bool qcovB = (f->q_covered_by != 0 && f->q_covered_by != meCol);
    const bool q_on_board = !f->Q && !f->q_pending && f->q_covered_by == 0;   /* untouched, still in play */
    const int extra = 1;   /* the pocketed striker's additional point, which the beneficiary demands (ICF 87b) */

    if (!f->own_last && !f->opp_last) return false;

    if (f->S) {
        if (f->own_last && f->opp_last) {
            *winner_pair = pairB;
            if (f->Q)        *points = queen_value_3_or_1(scoreB) + extra;   /* 109a */
            else if (qcovA)  *points = 1 + extra;                            /* 110a */
            else             *points = queen_value_3_or_1(scoreB) + extra;   /* 112a (queen covered by the opponent) and, likewise, queen on board */
            return true;
        }
        if (f->own_last) {
            if (q_on_board) { *winner_pair = pairB; *points = queen_value_3_or_1(scoreB) + extra; return true; }   /* 108a */
            return false;   /* queen covered or pocketed: ICF 73/98/101 apply, the coin goes back */
        }
        /* opponent's last coin with the striker */
        *winner_pair = pairB;
        if (q_on_board) *points = f->a + (scoreB >= 22 ? 0 : 3) + extra;                       /* 111a */
        else            *points = f->a + (qcovB && scoreB < 22 ? 3 : 0) + extra;               /* 52a with the striker's point */
        return true;
    }

    if (f->own_last && f->opp_last) {
        if (f->q_pending || f->Q)            { *winner_pair = pairA; *points = queen_value_3_or_1(scoreA); return true; }   /* 102a, 104a */
        if (q_on_board)                      { *winner_pair = pairB; *points = queen_value_3_or_1(scoreB); return true; }   /* 105a */
        if (qcovA)                           { *winner_pair = pairA; *points = queen_value_3_or_1(scoreA); return true; }   /* like 104a: the queen is his */
        *winner_pair = pairA; *points = 1; return true;                                                                     /* queen covered by the opponent: the mover finished */
    }
    if (f->own_last) {
        if (f->Q || f->q_pending)            { *winner_pair = pairA; *points = f->b + queen_credit(scoreA); return true; }   /* 97: covered by this stroke; 52 */
        if (qcovA)                           { *winner_pair = pairA; *points = f->b + queen_credit(scoreA); return true; }   /* 52, 53b */
        if (qcovB)                           { *winner_pair = pairA; *points = f->b; return true; }                          /* 53c */
        *winner_pair = pairB; *points = queen_value_3_or_1(scoreB); return true;                                            /* 107a: last coin, queen on board */
    }
    /* the opponent's last coin, by the mover */
    *winner_pair = pairB;
    if (f->q_pending || q_on_board || f->Q) *points = f->a + (scoreB >= 22 ? 0 : 3);   /* 103a, 106a */
    else if (qcovB)                         *points = f->a + queen_credit(scoreB);      /* 52, 53b: B covered the queen and wins */
    else                                    *points = f->a;                             /* the mover covered her: B wins by the coins */
    return true;
}

RulesOutcome rules_resolve(const MatchState* prior_match, const GameState* prior_game, const ShotFacts* facts) {
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    outcome.next_game_state = *prior_game;
    outcome.next_match_state = *prior_match;
    GameState* game = &outcome.next_game_state;
    MatchState* match = &outcome.next_match_state;
    BoardState* b = &game->board;
    game->shots_played++;
    outcome.turn_decision = TURN_CONTINUE;

    const Seat seat = facts->active_seat;
    const Team A = board_team_of_seat(b, seat);
    const Team B = (A == TEAM_WHITE) ? TEAM_BLACK : TEAM_WHITE;
    const PieceColor ownC = team_colour(A), oppC = team_colour(B);
    const int pairA = pair_of_seat(seat);

    /* what the stroke pocketed */
    int n = 0, m = 0;
    bool Q = facts->queen_pocketed;
    const bool S = facts->striker_pocketed;
    for (int i = 0; i < facts->pocketed_count && i < MAX_PIECES; i++) {
        uint8_t id = facts->pocketed_ids[i];
        if (id >= MAX_PIECES) continue;
        PieceState* pc = &b->pieces[id];
        pc->pocketed = true;
        pc->on_board = false;
        pc->pocket_index = facts->pocketed_pocket_indices[i];
        if (id == QUEEN_ID) Q = true;
        else if (facts->pocketed_colors[i] == ownC) n++;
        else if (facts->pocketed_colors[i] == oppC) m++;
        push_event(&outcome, (GameEvent){ .type = EVENT_POCKET, .tick = game->consecutive_turns, .seat = seat,
                                          .team = (facts->pocketed_colors[i] == PIECE_WHITE) ? TEAM_WHITE : TEAM_BLACK,
                                          .piece_id = id, .piece_color = facts->pocketed_colors[i],
                                          .pocket_index = (int8_t)facts->pocketed_pocket_indices[i], .turn_decision = TURN_CONTINUE });
    }
    if (Q) { b->queen_on_board = false; b->pieces[QUEEN_ID].pocketed = true; b->pieces[QUEEN_ID].on_board = false; }
    if (S) { b->striker.pocketed = true; b->striker.on_baseline = false; }

    /* ICF 44, 45: the break is made once the striker touches a coin; otherwise two more chances, then the turn passes */
    if (!b->break_made) {
        if (!facts->striker_touched_coin) {
            if (S) {
                outcome.foul = true;                     /* 45c: the turn is lost, no due, no penalty */
                b->break_attempts = 0;
                advance_turn(game, &outcome);
            } else if (++b->break_attempts >= 3) {
                b->break_attempts = 0;
                advance_turn(game, &outcome);            /* 45b: the right to break is lost */
            } else {
                game->consecutive_turns++;               /* 45a: another chance */
            }
            return outcome;
        }
        b->break_made = true;
        b->break_attempts = 0;
    }

    uint8_t* own_on = team_on_board(b, A);
    uint8_t* opp_on = team_on_board(b, B);
    const int own_before = *own_on, opp_before = *opp_on;
    *own_on = (uint8_t)((n <= own_before) ? own_before - n : 0);
    *opp_on = (uint8_t)((m <= opp_before) ? opp_before - m : 0);
    const bool all_nine = (own_before == 9);
    bool* had = team_had_pocketed(b, A);
    const bool right_to_queen = *had;                           /* 92, 95a, 95c */
    const bool due_out = (*team_dues(b, A) > 0);                /* 95b */
    const bool q_pending = (b->queen_state == QUEEN_STATE_POCKETED_NO_COVER);
    const int qcov_before = b->queen_covered_team;

    /* --- the queen and the turn (ICF 48, 92-101) --- */
    enum { QA_NONE, QA_PEND, QA_COVER, QA_RETURN } qa = QA_NONE;
    bool cont = false;
    int new_due = 0;
    if (S) {
        outcome.foul = true;
        new_due = 1;                                            /* 72a */
        if (q_pending) {
            if (n > 0 && b->queen_pending < 2) { cont = true; qa = QA_PEND; b->queen_pending = 2; }   /* 101a: one more stroke to cover */
            else { qa = QA_RETURN; cont = (n > 0); }                                                    /* 100a; 101a second time */
        } else if (Q) {
            qa = QA_RETURN;
            cont = !all_nine && (right_to_queen || n > 0) && !due_out;                                  /* 95a, 95b, 95d; 98a, 99a continue */
        } else {
            cont = (n > 0);                                                                             /* 73, 75 continue; 72a, 74 lose the turn */
        }
    } else if (q_pending) {
        if (n > 0) { qa = QA_COVER; cont = true; }              /* 15, 96: covered by a coin of his own */
        else       { qa = QA_RETURN; cont = false; }            /* 96: not covered */
    } else if (Q) {
        if (due_out)                       { qa = QA_RETURN; cont = false; }                  /* 95b */
        else if (m > 0 && n == 0)          { qa = QA_RETURN; cont = false; }                  /* 125: an opponent's coin stops him; she is not covered */
        else if (n == 0)                   { qa = right_to_queen ? QA_PEND : QA_RETURN; cont = right_to_queen; }   /* 92, 95a */
        else if (all_nine && n == 1)       { qa = QA_PEND; cont = true; }                     /* 97b: has to be covered */
        else                               { qa = QA_COVER; cont = true; }                    /* 97a, 97b */
    } else {
        cont = (n > 0);                                                                       /* 48 */
    }
    if (!S && n > 0) *had = true;                               /* he has pocketed a coin of his own: the right to the queen (92) */

    /* --- end of the board? (ICF 52, 53, 102-112) --- */
    StrokeFacts sf = {
        .n = n, .m = m, .S = S, .Q = Q,
        .own_last = (n > 0 && *own_on == 0), .opp_last = (m > 0 && *opp_on == 0),
        .q_pending = q_pending, .q_covered_by = qcov_before, .q_covered_by_me_now = (qa == QA_COVER),
        .a = *own_on, .b = *opp_on
    };
    int winner_pair = 0, points = 0;
    /* a coin can also have finished the board earlier by returning: nothing to check when both colours still have coins */
    if (board_result(&sf, A, pairA, game, &winner_pair, &points)) {
        if (Q && !q_pending && qa != QA_COVER) b->queen_state = QUEEN_STATE_POCKETED_NO_COVER;
        finish_board(prior_match, match, game, &outcome, winner_pair, points);
        return outcome;
    }

    /* --- the queen's fate --- */
    if (qa == QA_PEND) {
        b->queen_state = QUEEN_STATE_POCKETED_NO_COVER;
        if (b->queen_pending == 0) b->queen_pending = 1;
        b->queen_on_board = false;
        push_event(&outcome, (GameEvent){ .type = EVENT_QUEEN_POCKETED, .tick = game->consecutive_turns, .seat = seat, .team = A,
                                          .turn_decision = TURN_CONTINUE });
    } else if (qa == QA_COVER) {
        b->queen_state = QUEEN_STATE_COVERED;
        b->queen_covered_team = (uint8_t)((A == TEAM_WHITE) ? 1 : 2);
        b->queen_pending = 0;
        b->queen_on_board = false;
        push_event(&outcome, (GameEvent){ .type = EVENT_QUEEN_COVERED, .tick = game->consecutive_turns, .seat = seat, .team = A,
                                          .turn_decision = TURN_CONTINUE });
    }

    /* --- coins to put back: the ones a pocketed striker takes out (73, 75), and the dues (72, 95b, 98a, 99a) --- */
    const int owe_total = (int)*team_dues(b, A) + new_due;
    const int forced = S ? n : 0;                               /* his own coins pocketed with the striker are taken out again */
    int pocketed_own = 9 - *own_on;                             /* his coins lying in the pockets now */
    int give = forced + owe_total;
    if (give > pocketed_own) give = pocketed_own;
    if (qa == QA_RETURN) {
        Vec2 spot = board_find_free_spot(b, (Vec2){ 0.0f, 0.0f });   /* ICF 93, 94: the queen goes to the centre circle */
        put_back(game, &outcome, QUEEN_ID, spot);
        b->queen_state = QUEEN_STATE_ON_BOARD;
        b->queen_on_board = true;
        b->queen_pending = 0;
        outcome.queen_returned = true;
    }
    int given = 0;
    /* the coins go back in the order they fell: this stroke's first (last in, first out) */
    for (int q = b->pocketed_count - 1; q >= 0 && given < give; q--) {
        int id = b->pocketed_pieces[q].id;
        if (id >= MAX_PIECES || id == QUEEN_ID) continue;
        if (b->pieces[id].color != ownC || !b->pieces[id].pocketed) continue;
        Vec2 spot;
        if (!board_find_due_spot(b, &spot)) break;              /* no room: the due stays outstanding (79) */
        put_back(game, &outcome, id, spot);
        (*own_on)++;
        given++;
    }
    /* coins pocketed by this stroke that were not yet registered in the stash (tests, headless) */
    if (given < give) {
        for (int id = MAX_PIECES - 2; id >= 0 && given < give; id--) {
            PieceState* pc = &b->pieces[id];
            if (pc->color != ownC || !pc->pocketed || pc->on_board) continue;
            Vec2 spot;
            if (!board_find_due_spot(b, &spot)) break;
            put_back(game, &outcome, id, spot);
            (*own_on)++;
            given++;
        }
    }
    {
        int settled = given - forced;                           /* forced coins are not dues */
        if (settled < 0) settled = 0;
        int left = owe_total - settled;
        *team_dues(b, A) = (uint8_t)((left > 0) ? left : 0);
    }

    if (S) push_event(&outcome, (GameEvent){ .type = EVENT_FOUL, .tick = game->consecutive_turns, .seat = seat, .team = A, .turn_decision = TURN_ADVANCE });

    /* --- the turn --- */
    if (cont) {
        outcome.turn_decision = TURN_CONTINUE;
        game->consecutive_turns++;
    } else {
        advance_turn(game, &outcome);
    }
    return outcome;
}

void match_extract_facts(const GameState* game, const ShotResult* result, ShotFacts* facts) {
    memset(facts, 0, sizeof(ShotFacts));
    
    facts->active_seat = game->turn_seat;
    facts->queen_state = game->board.queen_state;
    facts->white_dues = game->board.white_dues;
    facts->black_dues = game->board.black_dues;
    facts->queen_dues = game->board.queen_dues;
    
    facts->pocketed_count = result->pocketed_count;
    facts->queen_pocketed = result->queen_pocketed;
    facts->striker_pocketed = result->striker_pocketed;
    facts->striker_touched_coin = result->striker_touched_coin;
    facts->fouls = result->fouls | (result->striker_pocketed ? FOUL_STRIKER_POCKETED : FOUL_NONE);
    
    for (int i = 0; i < result->pocketed_count; i++) {
        facts->pocketed_ids[i] = result->pocketed_ids[i];
        facts->pocketed_colors[i] = result->pocketed_colors[i];
        facts->pocketed_pocket_indices[i] = result->pocketed_pocket_indices[i];
    }
}

bool match_validate_shot(const GameState* game, const ShotPlan* plan) {
    // Validate placement is on correct baseline for active seat
    Seat seat = game->turn_seat;
    Vec2 p = plan->placement;
    
    // Check baseline bounds
    switch (seat) {
        case SEAT_NORTH:
            if (!(math_fabsf(p.y - BASELINE_Y_NORTH) <= 0.01f)) return false;
            if (!(p.x >= -BASELINE_MAX_OFFSET && p.x <= BASELINE_MAX_OFFSET)) return false;
            break;
        case SEAT_SOUTH:
            if (!(math_fabsf(p.y - BASELINE_Y_SOUTH) <= 0.01f)) return false;
            if (!(p.x >= -BASELINE_MAX_OFFSET && p.x <= BASELINE_MAX_OFFSET)) return false;
            break;
        case SEAT_EAST:
            if (!(math_fabsf(p.x - BASELINE_X_EAST) <= 0.01f)) return false;
            if (!(p.y >= -BASELINE_MAX_OFFSET && p.y <= BASELINE_MAX_OFFSET)) return false;
            break;
        case SEAT_WEST:
            if (!(math_fabsf(p.x - BASELINE_X_WEST) <= 0.01f)) return false;
            if (!(p.y >= -BASELINE_MAX_OFFSET && p.y <= BASELINE_MAX_OFFSET)) return false;
            break;
    }
    
    // Validate power range
    if (!(plan->power >= 0.0f && plan->power <= 1.0f)) return false;   /* written so that NaN fails */
    
    // Validate aim angle
    if (!(plan->aim_angle >= -M_PI && plan->aim_angle <= M_PI)) return false;
    
    return true;
}