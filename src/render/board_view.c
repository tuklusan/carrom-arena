#include "board_view.h"
#include "common/types.h"
#include "common/math.h"
#include "physics/physics.h"
#include <raylib.h>
#include <math.h>

/* Colors */
#define COLOR_BOARD (Color){ 139, 105, 70, 255 }      // Wood brown
#define COLOR_CUSHION (Color){ 100, 70, 40, 255 }     // Darker brown
#define COLOR_POCKET (Color){ 0, 0, 0, 255 }          // Black
#define COLOR_WHITE_PIECE (Color){ 240, 240, 240, 255 }
#define COLOR_BLACK_PIECE (Color){ 30, 30, 30, 255 }
#define COLOR_QUEEN (Color){ 220, 30, 30, 255 }       // Red
#define COLOR_STRIKER (Color){ 255, 215, 0, 255 }     // Gold
#define COLOR_LINE (Color){ 255, 255, 255, 100 }      // White translucent
#define COLOR_BASELINE (Color){ 100, 255, 100, 150 }  // Green translucent (muted for baseline)
#define COLOR_BASELINE_MUTED (Color){ 100, 255, 100, 76 }  // 30% alpha of team color (150 * 0.3 ≈ 45, but using 76 for visibility)

/* Team colors for figure drawing */
#define COLOR_TEAM_WHITE_FILL (Color){ 230, 230, 230, 255 }   // Light silhouette
#define COLOR_TEAM_WHITE_OUTLINE (Color){ 40, 40, 40, 255 }   // Dark outline
#define COLOR_TEAM_BLACK_FILL (Color){ 40, 40, 40, 255 }      // Dark silhouette
#define COLOR_TEAM_BLACK_OUTLINE (Color){ 230, 230, 230, 255 } // Light outline
#define COLOR_TURN_HIGHLIGHT (Color){ 255, 215, 0, 255 }      // Gold highlight for current turn

/* Draw a stylized human figure (head + shoulders + torso) using raylib primitives */
static void draw_human_figure(Viewport vp, Vec2 world_pos, float angle, Team team, bool is_current_turn) {
    // Convert world position to screen
    Vec2 screen = math_world_to_screen(vp, world_pos);
    
    // Figure dimensions (in screen pixels)
    float head_radius = math_world_to_screen_dist(vp, 0.025f);
    float shoulder_width = math_world_to_screen_dist(vp, 0.045f);
    float torso_height = math_world_to_screen_dist(vp, 0.055f);
    
    // Colors based on team
    Color fill_color = (team == TEAM_WHITE) ? COLOR_TEAM_WHITE_FILL : COLOR_TEAM_BLACK_FILL;
    Color outline_color = (team == TEAM_WHITE) ? COLOR_TEAM_WHITE_OUTLINE : COLOR_TEAM_BLACK_OUTLINE;
    Color highlight_color = is_current_turn ? COLOR_TURN_HIGHLIGHT : outline_color;
    
    // Calculate figure points (head at top, torso extending down)
    // The figure faces the board center, so angle points toward center
    Vec2 forward = { cosf(angle), sinf(angle) };
    Vec2 right = { -sinf(angle), cosf(angle) };
    
    // Head center
    Vec2 head_center = screen;
    
    // Shoulder line (perpendicular to facing direction)
    Vec2 shoulder_left = {
        head_center.x + right.x * shoulder_width,
        head_center.y + right.y * shoulder_width
    };
    Vec2 shoulder_right = {
        head_center.x - right.x * shoulder_width,
        head_center.y - right.y * shoulder_width
    };
    
    // Torso bottom (extending opposite to facing direction - away from board)
    Vec2 torso_bottom = {
        head_center.x - forward.x * torso_height,
        head_center.y - forward.y * torso_height
    };
    
    // Draw torso as a triangle (shoulders to bottom)
    DrawTriangle(
        (Vector2){ shoulder_left.x, shoulder_left.y },
        (Vector2){ shoulder_right.x, shoulder_right.y },
        (Vector2){ torso_bottom.x, torso_bottom.y },
        fill_color
    );
    DrawTriangleLines(
        (Vector2){ shoulder_left.x, shoulder_left.y },
        (Vector2){ shoulder_right.x, shoulder_right.y },
        (Vector2){ torso_bottom.x, torso_bottom.y },
        highlight_color
    );
    
    // Draw head circle
    DrawCircle((int)head_center.x, (int)head_center.y, head_radius, fill_color);
    DrawCircleLines((int)head_center.x, (int)head_center.y, head_radius, highlight_color);
    
    // If current turn, draw a subtle pulse ring around the figure
    if (is_current_turn) {
        float pulse = 1.0f + 0.15f * sinf((float)GetTime() * 4.0f);
        float pulse_radius = head_radius * pulse + shoulder_width * 0.5f;
        DrawCircleLines((int)head_center.x, (int)head_center.y, pulse_radius, COLOR_TURN_HIGHLIGHT);
    }
}

void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float alpha) {
    // We need the turn_seat to highlight the current player's figure
    // The board state has turn_seat in game->turn_seat, but we only have board here.
    // For now, we'll determine from the striker's owner_seat if on baseline, or default to NORTH
    Seat current_turn_seat = board->striker.owner_seat;
    if (board->striker.on_baseline) {
        current_turn_seat = board->striker.owner_seat;
    }
    // Clamp alpha to [0, 1]
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    
    // Get current and previous physics positions for interpolation
    Vec2 curr_positions[MAX_PIECES];
    Vec2 prev_positions[MAX_PIECES];
    Vec2 curr_striker_pos;
    Vec2 prev_striker_pos;
    bool use_physics = false;
    
    if (physics) {
        physics_get_positions(physics, curr_positions);
        physics_get_prev_positions(physics, prev_positions);
        physics_get_striker_position(physics, &curr_striker_pos);
        physics_get_prev_striker_position(physics, &prev_striker_pos);
        
        // Check if physics has valid positions (not all zero)
        for (int i = 0; i < MAX_PIECES; i++) {
            if (curr_positions[i].x != 0 || curr_positions[i].y != 0) {
                use_physics = true;
                break;
            }
        }
    }
    
    // Board surface
    float board_half = BOARD_SIDE_NORM * 0.5f;
    Vec2 bl = math_world_to_screen(vp, (Vec2){ -board_half, -board_half });
    Vec2 tr = math_world_to_screen(vp, (Vec2){ board_half, board_half });
    float board_w = tr.x - bl.x;
    float board_h = tr.y - bl.y;
    DrawRectangle((int)bl.x, (int)bl.y, (int)board_w, (int)board_h, COLOR_BOARD);
    
    // Cushions
    float cushion_t = math_world_to_screen_dist(vp, CUSHION_THICKNESS);
    
    // Top cushion
    DrawRectangle((int)bl.x, (int)bl.y, (int)board_w, (int)cushion_t, COLOR_CUSHION);
    // Bottom cushion
    DrawRectangle((int)bl.x, (int)(tr.y - cushion_t), (int)board_w, (int)cushion_t, COLOR_CUSHION);
    // Left cushion
    DrawRectangle((int)bl.x, (int)bl.y, (int)cushion_t, (int)board_h, COLOR_CUSHION);
    // Right cushion
    DrawRectangle((int)(tr.x - cushion_t), (int)bl.y, (int)cushion_t, (int)board_h, COLOR_CUSHION);
    
    // Center circle
    Vec2 center = math_world_to_screen(vp, (Vec2){0, 0});
    float circle_r = math_world_to_screen_dist(vp, 0.08f);
    DrawCircleLines((int)center.x, (int)center.y, circle_r, COLOR_LINE);
    
    // Baselines
    float baseline_y_n = math_world_to_screen(vp, (Vec2){0, BASELINE_Y_NORTH}).y;
    float baseline_y_s = math_world_to_screen(vp, (Vec2){0, BASELINE_Y_SOUTH}).y;
    float baseline_x_e = math_world_to_screen(vp, (Vec2){BASELINE_X_EAST, 0}).x;
    float baseline_x_w = math_world_to_screen(vp, (Vec2){BASELINE_X_WEST, 0}).x;
    
    float baseline_len = math_world_to_screen_dist(vp, BASELINE_MAX_OFFSET * 2);
    float baseline_x_start = center.x - baseline_len * 0.5f;
    float baseline_y_start = center.y - baseline_len * 0.5f;
    
    // Draw muted baseline lines (30% alpha)
    DrawLine((int)baseline_x_start, (int)baseline_y_n, (int)(baseline_x_start + baseline_len), (int)baseline_y_n, COLOR_BASELINE_MUTED);
    DrawLine((int)baseline_x_start, (int)baseline_y_s, (int)(baseline_x_start + baseline_len), (int)baseline_y_s, COLOR_BASELINE_MUTED);
    DrawLine((int)baseline_x_e, (int)baseline_y_start, (int)baseline_x_e, (int)(baseline_y_start + baseline_len), COLOR_BASELINE_MUTED);
    DrawLine((int)baseline_x_w, (int)baseline_y_start, (int)baseline_x_w, (int)(baseline_y_start + baseline_len), COLOR_BASELINE_MUTED);
    
    // Draw human figures for each seat (outside the board, facing center)
    // North seat (top) - WHITE team, faces down (angle = -PI/2)
    Vec2 north_figure_pos = { 0, BASELINE_Y_NORTH + 0.05f };  // 0.05 outside the baseline
    draw_human_figure(vp, north_figure_pos, -M_PI / 2.0f, TEAM_WHITE, current_turn_seat == SEAT_NORTH);
    
    // South seat (bottom) - WHITE team, faces up (angle = PI/2)
    Vec2 south_figure_pos = { 0, BASELINE_Y_SOUTH - 0.05f };
    draw_human_figure(vp, south_figure_pos, M_PI / 2.0f, TEAM_WHITE, current_turn_seat == SEAT_SOUTH);
    
    // East seat (right) - BLACK team, faces left (angle = PI)
    Vec2 east_figure_pos = { BASELINE_X_EAST + 0.05f, 0 };
    draw_human_figure(vp, east_figure_pos, M_PI, TEAM_BLACK, current_turn_seat == SEAT_EAST);
    
    // West seat (left) - BLACK team, faces right (angle = 0)
    Vec2 west_figure_pos = { BASELINE_X_WEST - 0.05f, 0 };
    draw_human_figure(vp, west_figure_pos, 0.0f, TEAM_BLACK, current_turn_seat == SEAT_WEST);
    
    // Pockets
    float pocket_r = math_world_to_screen_dist(vp, POCKET_RADIUS_NORM);
    for (int i = 0; i < 4; i++) {
        Vec2 p = math_world_to_screen(vp, POCKET_CENTERS[i]);
        DrawCircle((int)p.x, (int)p.y, pocket_r, COLOR_POCKET);
    }
    
    // Pieces (interpolated from physics for smooth animation, fall back to board state)
    for (int i = 0; i < MAX_PIECES; i++) {
        if (!board->pieces[i].on_board) continue;
        
        Vec2 pos;
        if (use_physics) {
            // Interpolate between previous and current physics positions
            Vec2 interp = vec2_lerp(prev_positions[i], curr_positions[i], alpha);
            pos = interp;
        } else {
            pos = board->pieces[i].position;
        }
        
        Vec2 screen = math_world_to_screen(vp, pos);
        float piece_r = math_world_to_screen_dist(vp, PIECE_RADIUS_NORM);
        
        Color c;
        if (board->pieces[i].color == PIECE_WHITE) c = COLOR_WHITE_PIECE;
        else if (board->pieces[i].color == PIECE_BLACK) c = COLOR_BLACK_PIECE;
        else c = COLOR_QUEEN;
        
        DrawCircle((int)screen.x, (int)screen.y, piece_r, c);
        DrawCircleLines((int)screen.x, (int)screen.y, piece_r, COLOR_LINE);
    }
    
    // Striker (interpolated)
    if (board->striker.on_baseline && !board->striker.pocketed) {
        Vec2 pos;
        if (use_physics) {
            Vec2 interp = vec2_lerp(prev_striker_pos, curr_striker_pos, alpha);
            pos = interp;
        } else {
            pos = board->striker.position;
        }
        
        Vec2 screen = math_world_to_screen(vp, pos);
        float striker_r = math_world_to_screen_dist(vp, STRIKER_RADIUS_NORM);
        DrawCircle((int)screen.x, (int)screen.y, striker_r, COLOR_STRIKER);
        DrawCircleLines((int)screen.x, (int)screen.y, striker_r, COLOR_LINE);
    }
}