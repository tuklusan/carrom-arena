#include "types.h"
#include "math.h"
#include <string.h>
#include <stdio.h>

/* -----------------------------------------------------------------------------
 * Type Utility Functions
 * --------------------------------------------------------------------------- */

void board_state_init(BoardState* board) {
    memset(board, 0, sizeof(BoardState));
    
    // Initialize 9 white pieces (IDs 0-8)
    for (uint8_t i = 0; i < 9; i++) {
        board->pieces[i].id = i;
        board->pieces[i].color = PIECE_WHITE;
        board->pieces[i].on_board = true;
    }
    
    // Initialize 9 black pieces (IDs 9-17)
    for (uint8_t i = 9; i < 18; i++) {
        board->pieces[i].id = i;
        board->pieces[i].color = PIECE_BLACK;
        board->pieces[i].on_board = true;
    }
    
    // Initialize queen (ID 18)
    board->pieces[QUEEN_ID].id = QUEEN_ID;
    board->pieces[QUEEN_ID].color = PIECE_QUEEN;
    board->pieces[QUEEN_ID].on_board = true;
    
    board->white_on_board = 9;
    board->black_on_board = 9;
    board->queen_on_board = true;
    board->queen_state = QUEEN_STATE_ON_BOARD;
    board->white_dues = 0;
    board->black_dues = 0;
    board->queen_dues = 0;
    
    // Striker initialized separately per seat
    board->striker.on_baseline = true;
}

void striker_state_init(StrikerState* striker, Seat seat) {
    memset(striker, 0, sizeof(StrikerState));
    striker->owner_seat = seat;
    striker->on_baseline = true;
    striker->pocketed = false;
}

void game_state_init(GameState* game, uint64_t seed) {
    memset(game, 0, sizeof(GameState));
    game->phase = PHASE_IDLE;
    game->turn_seat = SEAT_NORTH;
    game->consecutive_turns = 0;
    board_state_init(&game->board);
}

void match_state_init(MatchState* match) {
    memset(match, 0, sizeof(MatchState));
    match->target_boards_per_game = 8;   // First to 8 boards wins game
    match->target_games_per_match = 3;   // Best of 3 games wins match
}

void shot_plan_init(ShotPlan* plan) {
    memset(plan, 0, sizeof(ShotPlan));
    plan->power = 0.5f;
}

void shot_result_init(ShotResult* result) {
    memset(result, 0, sizeof(ShotResult));
}

void rules_outcome_init(RulesOutcome* outcome) {
    memset(outcome, 0, sizeof(RulesOutcome));
}

/* -----------------------------------------------------------------------------
 * Debug Printing
 * --------------------------------------------------------------------------- */

const char* piece_color_name(PieceColor c) {
    switch (c) {
        case PIECE_WHITE: return "WHITE";
        case PIECE_BLACK: return "BLACK";
        case PIECE_QUEEN: return "QUEEN";
        case PIECE_STRIKER: return "STRIKER";
        default: return "UNKNOWN";
    }
}

const char* seat_name(Seat s) {
    switch (s) {
        case SEAT_NORTH: return "NORTH";
        case SEAT_EAST:  return "EAST";
        case SEAT_SOUTH: return "SOUTH";
        case SEAT_WEST:  return "WEST";
        default: return "UNKNOWN";
    }
}

const char* team_name(Team t) {
    return t == TEAM_WHITE ? "WHITE" : "BLACK";
}

const char* phase_name(GamePhase p) {
    switch (p) {
        case PHASE_IDLE: return "IDLE";
        case PHASE_PLACEMENT: return "PLACEMENT";
        case PHASE_AIMING: return "AIMING";
        case PHASE_SHOT_EXECUTION: return "SHOT_EXECUTION";
        case PHASE_SETTLING: return "SETTLING";
        case PHASE_RESOLVING: return "RESOLVING";
        case PHASE_BOARD_OVER: return "BOARD_OVER";
        case PHASE_GAME_OVER: return "GAME_OVER";
        case PHASE_MATCH_OVER: return "MATCH_OVER";
        default: return "UNKNOWN";
    }
}

const char* tactic_name(TacticType t) {
    switch (t) {
        case TACTIC_BREAK: return "BREAK";
        case TACTIC_DIRECT: return "DIRECT";
        case TACTIC_CUT: return "CUT";
        case TACTIC_BANK: return "BANK";
        case TACTIC_QUEEN: return "QUEEN";
        case TACTIC_COVER: return "COVER";
        case TACTIC_DEFENSIVE: return "DEFENSIVE";
        case TACTIC_FALLBACK: return "FALLBACK";
        default: return "UNKNOWN";
    }
}

const char* event_name(GameEventType e) {
    switch (e) {
        case EVENT_POCKET: return "POCKET";
        case EVENT_FOUL: return "FOUL";
        case EVENT_QUEEN_POCKETED: return "QUEEN_POCKETED";
        case EVENT_QUEEN_COVERED: return "QUEEN_COVERED";
        case EVENT_TURN_CHANGE: return "TURN_CHANGE";
        case EVENT_BOARD_START: return "BOARD_START";
        case EVENT_BOARD_END: return "BOARD_END";
        case EVENT_GAME_START: return "GAME_START";
        case EVENT_GAME_END: return "GAME_END";
        case EVENT_MATCH_START: return "MATCH_START";
        case EVENT_MATCH_END: return "MATCH_END";
        default: return "UNKNOWN";
    }
}

const char* turn_decision_name(TurnDecision d) {
    switch (d) {
        case TURN_CONTINUE: return "CONTINUE";
        case TURN_ADVANCE: return "ADVANCE";
        case TURN_BOARD_OVER: return "BOARD_OVER";
        case TURN_GAME_OVER: return "GAME_OVER";
        case TURN_MATCH_OVER: return "MATCH_OVER";
        default: return "UNKNOWN";
    }
}

void shot_plan_print(const ShotPlan* plan) {
    printf("ShotPlan: placement=(%.3f,%.3f) aim=%.3f rad power=%.3f tactic=%s rng_draw=%u\n",
           (double)plan->placement.x, (double)plan->placement.y,
           (double)plan->aim_angle, (double)plan->power,
           tactic_name(plan->tactic), plan->rng_draw);
}

void shot_result_print(const ShotResult* result) {
    printf("ShotResult: pockets=%d queen=%s striker=%s fouls=0x%X sim_time=%.3f\n",
           result->pocketed_count,
           result->queen_pocketed ? "YES" : "NO",
           result->striker_pocketed ? "YES" : "NO",
           result->fouls, (double)result->sim_time);
    for (int i = 0; i < result->pocketed_count; i++) {
        printf("  [%d] ID=%d Color=%s\n", i, result->pocketed_ids[i], 
               piece_color_name(result->pocketed_colors[i]));
    }
}

void game_event_print(const GameEvent* evt) {
    printf("Event: type=%s tick=%llu seat=%s team=%s piece=%d color=%s pocket=%d score=(%d,%d) turn=%s\n",
           event_name(evt->type), (unsigned long long)evt->tick,
           seat_name(evt->seat), team_name(evt->team),
           evt->piece_id, piece_color_name(evt->piece_color),
           evt->pocket_index,
           evt->score_delta_white, evt->score_delta_black,
           turn_decision_name(evt->turn_decision));
}