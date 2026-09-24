#include "types.h"
#include "vecmath.h"
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