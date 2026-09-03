#include "hud.h"
#include "common/types.h"
#include <raylib.h>
#include <stdio.h>

void hud_draw(Viewport vp, const MatchState* match, const GameState* game) {
    // Base font size scales with game surface (600px = baseline)
    float scale = vp.board_size_px / 600.0f;
    int base_font = (int)(18 * scale);
    int large_font = (int)(24 * scale);
    int small_font = (int)(14 * scale);
    float line_height = 28.0f * scale;
    
    // Game surface screen bounds
    float gs_left = vp.board_center_px.x - vp.board_size_px * 0.5f;
    float gs_top = vp.board_center_px.y - vp.board_size_px * 0.5f;
    
    // Margin from game surface
    float margin = 16.0f * scale;
    
    // HUD positioned at TOP-LEFT of game surface (outside board)
    float x = gs_left - margin;
    float y = gs_top - margin;
    
    // But if that's off-screen, anchor to window left with margin
    if (x < margin) x = margin;
    if (y < margin) y = margin;
    
    // Draw score panel - TOP LEFT
    Color white_color = WHITE;
    Color black_color = WHITE;
    
    // Highlight current player's team
    if (game->active_player.team == TEAM_WHITE) white_color = YELLOW;
    else black_color = YELLOW;
    
    DrawText(TextFormat("WHITE: %d", match->games_won_white), (int)x, (int)y, large_font, white_color);
    y += line_height;
    DrawText(TextFormat("BLACK: %d", match->games_won_black), (int)x, (int)y, large_font, black_color);
    y += line_height * 1.2f;
    
    // Board score
    DrawText(TextFormat("Board: W %d - B %d", game->scores.white, game->scores.black), 
             (int)x, (int)y, base_font, WHITE);
    y += line_height;
    
    // Turn indicator - HIGHLIGHT current player
    const char* seat_names[4] = { "NORTH", "EAST", "SOUTH", "WEST" };
    const char* team_names[2] = { "WHITE", "BLACK" };
    Color turn_color = (game->active_player.team == TEAM_WHITE) ? YELLOW : ORANGE;
    DrawText(TextFormat("Turn: %s (%s)", seat_names[game->turn_seat], team_names[game->active_player.team]), 
             (int)x, (int)y, base_font, turn_color);
    y += line_height;
    
    // Phase
    const char* phase_names[] = {
        "IDLE", "PLACEMENT", "AIMING", "SHOT", "SETTLING", "RESOLVING",
        "BOARD_OVER", "GAME_OVER", "MATCH_OVER"
    };
    DrawText(TextFormat("Phase: %s", phase_names[game->phase]), (int)x, (int)y, small_font, GREEN);
    y += line_height;
    
    // Progress
    DrawText(TextFormat("Boards: %d/%d", match->boards_won_white + match->boards_won_black, match->target_boards_per_game), 
             (int)x, (int)y, small_font, LIGHTGRAY);
    y += line_height * 0.8f;
    DrawText(TextFormat("Games: %d/%d", match->games_won_white + match->games_won_black, match->target_games_per_match), 
             (int)x, (int)y, small_font, LIGHTGRAY);
    y += line_height;
    
    // Queen state
    const char* queen_states[] = { "ON_BOARD", "POCKETED_NO_COVER", "COVERED", "DUE" };
    Color queen_color = (game->board.queen_state == QUEEN_STATE_COVERED) ? GOLD : WHITE;
    DrawText(TextFormat("Queen: %s", queen_states[game->board.queen_state]), (int)x, (int)y, small_font, queen_color);
    y += line_height * 0.8f;
    
    // Dues
    DrawText(TextFormat("Dues: W=%d B=%d Q=%d", game->board.white_dues, game->board.black_dues, game->board.queen_dues), 
             (int)x, (int)y, small_font, LIGHTGRAY);
    y += line_height * 0.8f;
    
    // Piece counts
    DrawText(TextFormat("Pieces: W=%d B=%d Q=%s", game->board.white_on_board, game->board.black_on_board, 
             game->board.queen_on_board ? "YES" : "NO"), (int)x, (int)y, small_font, LIGHTGRAY);
}