#include "match.h"
#include "common/types.h"
#include "common/rng.h"
#include "board.h"

void match_state_init(MatchState* match) {
    match->boards_won_white = 0;
    match->boards_won_black = 0;
    match->games_won_white = 0;
    match->games_won_black = 0;
    match->target_boards_per_game = 8;
    match->target_games_per_match = 3;
}

bool match_is_over(const MatchState* match) {
    return match->games_won_white >= match->target_games_per_match || 
           match->games_won_black >= match->target_games_per_match;
}

void match_start_board(MatchState* match, GameState* game, RNGContext* rng) {
    board_state_init(&game->board);
    board_setup_initial_formation(&game->board, rng);
    game->phase = PHASE_PLACEMENT;
    
    // Determine who breaks (alternate or based on previous board winner)
    int total_boards = match->boards_won_white + match->boards_won_black;
    game->turn_seat = (Seat)(total_boards % 4);
    game->active_player.seat = game->turn_seat;
    game->active_player.team = (game->turn_seat == SEAT_NORTH || game->turn_seat == SEAT_SOUTH) ? TEAM_WHITE : TEAM_BLACK;
    game->consecutive_turns = 0;
    
    // Place striker
    striker_state_init(&game->board.striker, game->turn_seat);
    board_place_striker_on_baseline(&game->board.striker, game->turn_seat);
}

void match_advance_turn(GameState* game) {
    game->turn_seat = (game->turn_seat + 1) % 4;
    game->active_player.seat = game->turn_seat;
    game->active_player.team = (game->turn_seat == SEAT_NORTH || game->turn_seat == SEAT_SOUTH) ? TEAM_WHITE : TEAM_BLACK;
    game->consecutive_turns = 0;
}