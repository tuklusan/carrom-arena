#include "hud.h"
#include "common/types.h"
#include <raylib.h>
#include <stdio.h>

void hud_draw(Viewport vp, const MatchState* match, const GameState* game) {
    (void)vp;
    
    // Score panel
    DrawText(TextFormat("WHITE: %d", match->games_won_white), 20, 20, 24, WHITE);
    DrawText(TextFormat("BLACK: %d", match->games_won_black), 20, 50, 24, WHITE);
    
    // Board score
    DrawText(TextFormat("Board: W %d - B %d", game->scores.white, game->scores.black), 20, 90, 20, WHITE);
    
    // Turn indicator
    const char* seat_names[4] = { "NORTH", "EAST", "SOUTH", "WEST" };
    const char* team_names[2] = { "WHITE", "BLACK" };
    DrawText(TextFormat("Turn: %s (%s)", seat_names[game->turn_seat], team_names[game->active_player.team]), 20, 120, 20, YELLOW);
    
    // Phase
    const char* phase_names[] = {
        "IDLE", "PLACEMENT", "AIMING", "SHOT", "SETTLING", "RESOLVING",
        "BOARD_OVER", "GAME_OVER", "MATCH_OVER"
    };
    DrawText(TextFormat("Phase: %s", phase_names[game->phase]), 20, 150, 20, GREEN);
    
    // Board/Game/Match progress
    DrawText(TextFormat("Boards: %d/%d", match->boards_won_white + match->boards_won_black, match->target_boards_per_game), 20, 180, 18, LIGHTGRAY);
    DrawText(TextFormat("Games: %d/%d", match->games_won_white + match->games_won_black, match->target_games_per_match), 20, 200, 18, LIGHTGRAY);
    
    // Queen state
    const char* queen_states[] = { "ON_BOARD", "POCKETED_NO_COVER", "COVERED", "DUE" };
    DrawText(TextFormat("Queen: %s", queen_states[game->board.queen_state]), 20, 230, 18, game->board.queen_state == QUEEN_STATE_COVERED ? GOLD : WHITE);
    
    // Dues
    DrawText(TextFormat("Dues: W=%d B=%d Q=%d", game->board.white_dues, game->board.black_dues, game->board.queen_dues), 20, 255, 18, LIGHTGRAY);
    
    // Piece counts
    DrawText(TextFormat("Pieces: W=%d B=%d Q=%s", game->board.white_on_board, game->board.black_on_board, game->board.queen_on_board ? "YES" : "NO"), 20, 280, 18, LIGHTGRAY);
}