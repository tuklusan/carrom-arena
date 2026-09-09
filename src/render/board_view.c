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

/* Team colors for figure drawing - per spec */
#define COLOR_TEAM_WHITE_FILL (Color){ 240, 240, 220, 255 }   // Light silhouette
#define COLOR_TEAM_WHITE_OUTLINE (Color){ 60, 60, 60, 255 }   // Dark outline
#define COLOR_TEAM_BLACK_FILL (Color){ 40, 40, 40, 255 }      // Dark silhouette
#define COLOR_TEAM_BLACK_OUTLINE (Color){ 200, 200, 200, 255 } // Light outline
#define COLOR_TURN_HIGHLIGHT (Color){ 255, 215, 0, 255 }      // Gold highlight for current turn

/* Fixed world positions for player figures (mapped to screen coordinates per layout spec)
 * Viewport: board_center_px = (510, 250), world_to_screen = 360
 * Screen positions: North(510,50), South(510,450), West(280,250), East(740,250)
 * World = (screen - center) / world_to_screen (y inverted)
 */
#define FIGURE_NORTH_WORLD_Y (200.0f / 360.0f)    // 0.5555...
#define FIGURE_SOUTH_WORLD_Y (-200.0f / 360.0f)   // -0.5555...
#define FIGURE_WEST_WORLD_X (-230.0f / 360.0f)    // -0.6388...
#define FIGURE_EAST_WORLD_X (230.0f / 360.0f)     // 0.6388...

/* Draw a stylised head-and-shoulders silhouette per spec:
 *    ○         (head: DrawCircle radius 8)
 *   ┃┃         (shoulders: DrawEllipse)
 *  ▄▄▄▄        (torso: filled trapezoid via DrawTriangle * 2)
 */
static void draw_human_figure(Viewport vp, Vec2 world_pos, float angle, Team team, bool is_current_turn) {
    // Convert world position to screen
    Vec2 screen = math_world_to_screen(vp, world_pos);
    
    // Figure dimensions in screen pixels
    float head_radius = 8.0f;
    float shoulder_width = 12.0f;
    float shoulder_height = 4.0f;
    float torso_height = 24.0f;
    float torso_bottom_width = 16.0f;
    
    // Colors based on team
    Color fill_color = (team == TEAM_WHITE) ? COLOR_TEAM_WHITE_FILL : COLOR_TEAM_BLACK_FILL;
    Color outline_color = (team == TEAM_WHITE) ? COLOR_TEAM_WHITE_OUTLINE : COLOR_TEAM_BLACK_OUTLINE;
    Color highlight_color = is_current_turn ? COLOR_TURN_HIGHLIGHT : outline_color;
    
    // Calculate figure orientation (facing board center)
    // angle points from figure toward board center
    Vec2 forward = { cosf(angle), sinf(angle) };
    Vec2 right = { -sinf(angle), cosf(angle) };
    
    // Head center
    Vec2 head_center = screen;
    
    // Shoulders (ellipse centered below head)
    Vec2 shoulders_center = {
        head_center.x - forward.x * (head_radius + 2),
        head_center.y - forward.y * (head_radius + 2)
    };
    
    // Torso bottom (trapezoid base)
    Vec2 torso_bottom_center = {
        shoulders_center.x - forward.x * torso_height,
        shoulders_center.y - forward.y * torso_height
    };
    
    // Torso corners (trapezoid)
    Vec2 torso_top_left = {
        shoulders_center.x + right.x * shoulder_width,
        shoulders_center.y + right.y * shoulder_width
    };
    Vec2 torso_top_right = {
        shoulders_center.x - right.x * shoulder_width,
        shoulders_center.y - right.y * shoulder_width
    };
    Vec2 torso_bottom_left = {
        torso_bottom_center.x + right.x * torso_bottom_width,
        torso_bottom_center.y + right.y * torso_bottom_width
    };
    Vec2 torso_bottom_right = {
        torso_bottom_center.x - right.x * torso_bottom_width,
        torso_bottom_center.y - right.y * torso_bottom_width
    };
    
    // Draw torso as two triangles (filled trapezoid)
    DrawTriangle(
        (Vector2){ torso_top_left.x, torso_top_left.y },
        (Vector2){ torso_top_right.x, torso_top_right.y },
        (Vector2){ torso_bottom_left.x, torso_bottom_left.y },
        fill_color
    );
    DrawTriangle(
        (Vector2){ torso_top_right.x, torso_top_right.y },
        (Vector2){ torso_bottom_right.x, torso_bottom_right.y },
        (Vector2){ torso_bottom_left.x, torso_bottom_left.y },
        fill_color
    );
    
    // Torso outline
    DrawTriangleLines(
        (Vector2){ torso_top_left.x, torso_top_left.y },
        (Vector2){ torso_top_right.x, torso_top_right.y },
        (Vector2){ torso_bottom_left.x, torso_bottom_left.y },
        highlight_color
    );
    DrawTriangleLines(
        (Vector2){ torso_top_right.x, torso_top_right.y },
        (Vector2){ torso_bottom_right.x, torso_bottom_right.y },
        (Vector2){ torso_bottom_left.x, torso_bottom_left.y },
        highlight_color
    );
    
    // Draw shoulders ellipse
    DrawEllipse((int)shoulders_center.x, (int)shoulders_center.y, shoulder_width, shoulder_height, fill_color);
    DrawEllipseLines((int)shoulders_center.x, (int)shoulders_center.y, shoulder_width, shoulder_height, highlight_color);
    
    // Draw head circle
    DrawCircle((int)head_center.x, (int)head_center.y, head_radius, fill_color);
    DrawCircleLines((int)head_center.x, (int)head_center.y, head_radius, highlight_color);
    
    // If current turn, draw a gold halo ring around the head
    if (is_current_turn) {
        DrawRing(
            (Vector2){ head_center.x, head_center.y },
            12.0f, 14.0f, 0, 360, 32, COLOR_TURN_HIGHLIGHT
        );
    }
}

void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float alpha) {
    // Determine current turn seat from striker owner
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
    
    // Draw human figures for each seat at fixed world positions (mapped to layout spec screen coords)
    // North seat (top) - WHITE team, faces down (angle = -PI/2)
    Vec2 north_figure_pos = { 0.0f, FIGURE_NORTH_WORLD_Y };
    draw_human_figure(vp, north_figure_pos, -M_PI / 2.0f, TEAM_WHITE, current_turn_seat == SEAT_NORTH);
    
    // South seat (bottom) - WHITE team, faces up (angle = PI/2)
    Vec2 south_figure_pos = { 0.0f, FIGURE_SOUTH_WORLD_Y };
    draw_human_figure(vp, south_figure_pos, M_PI / 2.0f, TEAM_WHITE, current_turn_seat == SEAT_SOUTH);
    
    // East seat (right) - BLACK team, faces left (angle = PI)
    Vec2 east_figure_pos = { FIGURE_EAST_WORLD_X, 0.0f };
    draw_human_figure(vp, east_figure_pos, M_PI, TEAM_BLACK, current_turn_seat == SEAT_EAST);
    
    // West seat (left) - BLACK team, faces right (angle = 0)
    Vec2 west_figure_pos = { FIGURE_WEST_WORLD_X, 0.0f };
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