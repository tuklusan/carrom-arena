#include "hud.h"
#include "common/types.h"
#include <raylib.h>
#include <stdio.h>

/* HUD sidebar fixed layout - left sidebar x=10 to x=200 */
#define HUD_X 10
#define HUD_START_Y 40
#define HUD_LINE_HEIGHT 18
#define HUD_FONT_SIZE 14

/* Team colors for HUD text */
#define COLOR_WHITE_TEAM (Color){ 255, 215, 0, 255 }   // Gold for WHITE
#define COLOR_BLACK_TEAM (Color){ 255, 255, 255, 255 } // White for BLACK
#define COLOR_HIGHLIGHT (Color){ 255, 215, 0, 255 }    // Gold highlight
#define COLOR_NORMAL (Color){ 255, 255, 255, 255 }
#define COLOR_DIM (Color){ 200, 200, 200, 255 }

void hud_draw(Viewport vp, const MatchState* match, const GameState* game, float playback_speed) {
    (void)vp;  // Fixed layout, viewport not needed for positioning
    
    float x = HUD_X;
    float y = HUD_START_Y;
    int font = HUD_FONT_SIZE;
    float lh = HUD_LINE_HEIGHT;
    
    // Determine highlight for current player
    bool white_turn = (game->active_player.team == TEAM_WHITE);
    Color white_color = white_turn ? COLOR_WHITE_TEAM : COLOR_NORMAL;
    Color black_color = !white_turn ? COLOR_WHITE_TEAM : COLOR_NORMAL;
    
    // WHITE score
    DrawText(TextFormat("WHITE  %d", match->games_won_white), (int)x, (int)y, font, white_color);
    y += lh;
    
    // BLACK score
    DrawText(TextFormat("BLACK  %d", match->games_won_black), (int)x, (int)y, font, black_color);
    y += lh;
    
    // Board score
    DrawText(TextFormat("Board: W %d - B %d", game->scores.white, game->scores.black), 
             (int)x, (int)y, font, COLOR_NORMAL);
    y += lh;
    
    // Turn indicator - HIGHLIGHT current player
    const char* seat_names[4] = { "NORTH", "EAST", "SOUTH", "WEST" };
    const char* team_names[2] = { "WHITE", "BLACK" };
    Color turn_color = white_turn ? COLOR_WHITE_TEAM : (Color){ 255, 165, 0, 255 }; // Gold for white, Orange for black
    DrawText(TextFormat("Turn: %s (%s)", seat_names[game->turn_seat], team_names[game->active_player.team]), 
             (int)x, (int)y, font, turn_color);
    y += lh;
    
    // Phase
    const char* phase_names[] = {
        "IDLE", "PLACEMENT", "AIMING", "SHOT", "SETTLING", "RESOLVING",
        "BOARD_OVER", "GAME_OVER", "MATCH_OVER"
    };
    DrawText(TextFormat("Phase: %s", phase_names[game->phase]), (int)x, (int)y, font, GREEN);
    y += lh;
    
    // Playback speed
    Color speed_color = (playback_speed <= 0.0f) ? RED : COLOR_NORMAL;
    DrawText(TextFormat("Speed: %.2fx", playback_speed), (int)x, (int)y, font, speed_color);
    y += lh;
    
    // Boards progress
    DrawText(TextFormat("Boards: %d/%d", match->boards_won_white + match->boards_won_black, match->target_boards_per_game), 
             (int)x, (int)y, font, COLOR_DIM);
    y += lh;
    
    // Games progress
    DrawText(TextFormat("Games: %d/%d", match->games_won_white + match->games_won_black, match->target_games_per_match), 
             (int)x, (int)y, font, COLOR_DIM);
    y += lh;
    
    // Queen state
    const char* queen_states[] = { "ON_BOARD", "POCKETED_NO_COVER", "COVERED", "DUE" };
    Color queen_color = (game->board.queen_state == QUEEN_STATE_COVERED) ? GOLD : COLOR_NORMAL;
    DrawText(TextFormat("Queen: %s", queen_states[game->board.queen_state]), (int)x, (int)y, font, queen_color);
    y += lh;
    
    // Dues
    DrawText(TextFormat("Dues: W=%d B=%d Q=%d", game->board.white_dues, game->board.black_dues, game->board.queen_dues), 
             (int)x, (int)y, font, COLOR_DIM);
    y += lh;
    
    // Piece counts
    DrawText(TextFormat("Pieces: W=%d B=%d Q=%s", game->board.white_on_board, game->board.black_on_board, 
             game->board.queen_on_board ? "YES" : "NO"), (int)x, (int)y, font, COLOR_DIM);
}