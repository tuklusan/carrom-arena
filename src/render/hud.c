#include "hud.h"
#include "common/types.h"
#include <raylib.h>
#include <stdio.h>

void hud_draw(Viewport vp, const MatchState* match, const GameState* game, float playback_speed, const Layout* L, int candidates_evaluated) {
    (void)vp;
    
    float x = 10.0f;
    float y = (float)L->hud_start_y;
    int font = L->font_size_hud;
    float lh = (float)L->hud_line_height;
    
    bool white_turn = (game->active_player.team == TEAM_WHITE);
    Color white_color = white_turn ? (Color){ 255, 215, 0, 255 } : WHITE;
    Color black_color = !white_turn ? (Color){ 255, 215, 0, 255 } : WHITE;
    
    DrawText(TextFormat("WHITE  %d", match->games_won_white), (int)x, (int)y, font, white_color);
    y += lh;
    
    DrawText(TextFormat("BLACK  %d", match->games_won_black), (int)x, (int)y, font, black_color);
    y += lh;
    
    DrawText(TextFormat("Board: W %d - B %d", game->scores.white, game->scores.black), (int)x, (int)y, font, WHITE);
    y += lh;
    
    const char* seat_names[4] = { "NORTH", "EAST", "SOUTH", "WEST" };
    const char* team_names[2] = { "WHITE", "BLACK" };
    Color turn_color = white_turn ? (Color){ 255, 215, 0, 255 } : (Color){ 255, 165, 0, 255 };
    DrawText(TextFormat("Turn: %s (%s)", seat_names[game->turn_seat], team_names[game->active_player.team]), (int)x, (int)y, font, turn_color);
    y += lh;
    
    const char* phase_names[] = {
        "IDLE", "THINKING", "PLACEMENT", "AIMING", "SHOT", "SETTLING", "RESOLVING",
        "BOARD_OVER", "GAME_OVER", "MATCH_OVER"
    };
    
    if (game->phase == PHASE_THINKING && candidates_evaluated >= 0) {
        DrawText(TextFormat("Phase: THINKING (evaluating %d shots...)", candidates_evaluated), (int)x, (int)y, font, YELLOW);
    } else {
        DrawText(TextFormat("Phase: %s", phase_names[game->phase]), (int)x, (int)y, font, GREEN);
    }
    y += lh;
    
    Color speed_color = (playback_speed <= 0.0f) ? RED : WHITE;
    DrawText(TextFormat("Speed: %.2fx", playback_speed), (int)x, (int)y, font, speed_color);
    y += lh;
    
    DrawText(TextFormat("Boards: %d/%d", match->boards_won_white + match->boards_won_black, match->target_boards_per_game), (int)x, (int)y, font, LIGHTGRAY);
    y += lh;
    
    DrawText(TextFormat("Games: %d/%d", match->games_won_white + match->games_won_black, match->target_games_per_match), (int)x, (int)y, font, LIGHTGRAY);
    y += lh;
    
    const char* queen_states[] = { "ON_BOARD", "POCKETED_NO_COVER", "COVERED", "DUE" };
    Color queen_color = (game->board.queen_state == QUEEN_STATE_COVERED) ? GOLD : WHITE;
    DrawText(TextFormat("Queen: %s", queen_states[game->board.queen_state]), (int)x, (int)y, font, queen_color);
    y += lh;
    
    DrawText(TextFormat("Dues: W=%d B=%d Q=%d", game->board.white_dues, game->board.black_dues, game->board.queen_dues), (int)x, (int)y, font, LIGHTGRAY);
    y += lh;
    
    DrawText(TextFormat("Pieces: W=%d B=%d Q=%s", game->board.white_on_board, game->board.black_on_board, 
             game->board.queen_on_board ? "YES" : "NO"), (int)x, (int)y, font, LIGHTGRAY);
}