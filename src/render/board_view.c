#include "board_view.h"
#include "renderer.h"
#include "common/types.h"
#include "common/math.h"
#include "physics/physics.h"
#include <math.h>
#define __USE_MINGW_ANSI_STDIO 1
#include <raylib.h>

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
#define COLOR_BASELINE_MUTED (Color){ 100, 255, 100, 76 }  // 30% alpha of team color

/* Team colors for figure drawing - per spec */
#define COLOR_TEAM_WHITE_FILL (Color){ 240, 240, 220, 255 }   // Light silhouette
#define COLOR_TEAM_WHITE_OUTLINE (Color){ 60, 60, 60, 255 }   // Dark outline
#define COLOR_TEAM_BLACK_FILL (Color){ 40, 40, 40, 255 }      // Dark silhouette
#define COLOR_TEAM_BLACK_OUTLINE (Color){ 200, 200, 200, 255 } // Light outline
#define COLOR_TURN_HIGHLIGHT (Color){ 255, 215, 0, 255 }      // Gold highlight for current turn

/* Draw a stylised head-and-shoulders silhouette per spec:
 *    ○         (head: DrawCircle radius = L->board_size / 25)
 *   ┃┃         (shoulders: DrawEllipse half-width = L->board_size / 18, height = L->board_size / 45)
 *  ▄▄▄▄        (torso: filled trapezoid height = L->board_size / 12, bottom half-width = L->board_size / 22.5)
 * All shapes FILLED with team color, 2px outline accent
 */
static void draw_human_figure(Viewport vp, const Layout* L, Vec2 world_pos, float angle, Team team, bool is_current_turn, float halo_pulse) {
    // Convert world position to screen
    Vec2 screen = math_world_to_screen(vp, world_pos);
    
    // Figure dimensions in screen pixels (based on L->board_size per spec)
    float head_radius = (float)L->board_size / 25.0f;
    float shoulder_half_width = (float)L->board_size / 18.0f;
    float shoulder_height = (float)L->board_size / 45.0f;
    float torso_height = (float)L->board_size / 12.0f;
    float torso_bottom_half_width = (float)L->board_size / 22.5f;
    
    // Colors based on team
    Color fill_color = (team == TEAM_WHITE) ? COLOR_TEAM_WHITE_FILL : COLOR_TEAM_BLACK_FILL;
    Color outline_color = (team == TEAM_WHITE) ? COLOR_TEAM_WHITE_OUTLINE : COLOR_TEAM_BLACK_OUTLINE;
    Color highlight_color = is_current_turn ? COLOR_TURN_HIGHLIGHT : outline_color;
    
    // Calculate figure orientation (facing board center)
    Vec2 forward = { cosf(angle), sinf(angle) };
    Vec2 right = { -sinf(angle), cosf(angle) };
    
    // Head center
    Vec2 head_center = screen;
    
    // Shoulders (ellipse centered below head)
    Vec2 shoulders_center = {
        head_center.x - forward.x * (head_radius + 2.0f),
        head_center.y - forward.y * (head_radius + 2.0f)
    };
    
    // Torso bottom (trapezoid base)
    Vec2 torso_bottom_center = {
        shoulders_center.x - forward.x * torso_height,
        shoulders_center.y - forward.y * torso_height
    };
    
    // Torso corners (trapezoid)
    Vec2 torso_top_left = {
        shoulders_center.x + right.x * shoulder_half_width,
        shoulders_center.y + right.y * shoulder_half_width
    };
    Vec2 torso_top_right = {
        shoulders_center.x - right.x * shoulder_half_width,
        shoulders_center.y - right.y * shoulder_half_width
    };
    Vec2 torso_bottom_left = {
        torso_bottom_center.x + right.x * torso_bottom_half_width,
        torso_bottom_center.y + right.y * torso_bottom_half_width
    };
    Vec2 torso_bottom_right = {
        torso_bottom_center.x - right.x * torso_bottom_half_width,
        torso_bottom_center.y - right.y * torso_bottom_half_width
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
    
    // Torso outline (2px thick - draw 2 passes)
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
    // Second pass for 2px thickness - slightly offset
    DrawTriangleLines(
        (Vector2){ torso_top_left.x + 1.0f, torso_top_left.y },
        (Vector2){ torso_top_right.x + 1.0f, torso_top_right.y },
        (Vector2){ torso_bottom_left.x + 1.0f, torso_bottom_left.y },
        highlight_color
    );
    DrawTriangleLines(
        (Vector2){ torso_top_right.x + 1.0f, torso_top_right.y },
        (Vector2){ torso_bottom_right.x + 1.0f, torso_bottom_right.y },
        (Vector2){ torso_bottom_left.x + 1.0f, torso_bottom_left.y },
        highlight_color
    );
    
    // Draw shoulders ellipse (filled)
    DrawEllipse((int)shoulders_center.x, (int)shoulders_center.y, shoulder_half_width, shoulder_height, fill_color);
    // Shoulders outline (2px thick - draw 2 passes)
    DrawEllipseLines((int)shoulders_center.x, (int)shoulders_center.y, shoulder_half_width, shoulder_height, highlight_color);
    DrawEllipseLines((int)shoulders_center.x + 1, (int)shoulders_center.y, shoulder_half_width, shoulder_height, highlight_color);
    
    // Draw head circle (filled)
    DrawCircle((int)head_center.x, (int)head_center.y, head_radius, fill_color);
    // Head outline (2px thick - draw 2 passes)
    DrawCircleLines((int)head_center.x, (int)head_center.y, head_radius, highlight_color);
    DrawCircleLines((int)head_center.x, (int)head_center.y, head_radius + 1.0f, highlight_color);
    
    // If current turn, draw a pulsing gold halo ring around the head
    if (is_current_turn) {
        float halo_r_inner = L->figure_halo_base_r + halo_pulse * 5.0f * L->figure_scale;
        float halo_r_outer = halo_r_inner + 2.0f * L->figure_scale;
        DrawRing(
            (Vector2){ head_center.x, head_center.y },
            halo_r_inner, halo_r_outer, 0, 360, 32, COLOR_TURN_HIGHLIGHT
        );
    }
}

/* Draw thinking animation: striker sliding along baseline with fade pulse */
static void draw_thinking_striker(Viewport vp, const Layout* L, Seat seat, const BoardState* board, float think_time) {
    // Only draw if striker is on baseline for this seat
    if (!board->striker.on_baseline || board->striker.owner_seat != seat) return;
    
    // Slide back and forth along baseline: one full pass every 1.5s
    float slide_period = 1.5f;
    float slide_phase = fmodf(think_time, slide_period) / slide_period;  // 0 to 1
    // Map to ping-pong: 0->1->0
    float slide_t = slide_phase <= 0.5f ? slide_phase * 2.0f : (1.0f - slide_phase) * 2.0f;
    
    // Baseline limits in normalized coords
    float min_offset = BASELINE_MIN_OFFSET;
    float max_offset = BASELINE_MAX_OFFSET;
    float slide_offset = min_offset + slide_t * (max_offset - min_offset);
    
    // Alternate direction each half-period for visual variety
    if (slide_phase > 0.5f) slide_offset = max_offset - slide_t * (max_offset - min_offset);
    
    // Compute striker world position based on seat
    Vec2 striker_world = {0, 0};
    switch (seat) {
        case SEAT_NORTH:
            striker_world.x = slide_offset;
            striker_world.y = BASELINE_Y_NORTH;
            break;
        case SEAT_SOUTH:
            striker_world.x = slide_offset;
            striker_world.y = BASELINE_Y_SOUTH;
            break;
        case SEAT_EAST:
            striker_world.x = BASELINE_X_EAST;
            striker_world.y = slide_offset;
            break;
        case SEAT_WEST:
            striker_world.x = BASELINE_X_WEST;
            striker_world.y = slide_offset;
            break;
    }
    
    Vec2 screen = math_world_to_screen(vp, striker_world);
    
    // Fade pulse: 40% to 100% alpha, once per second (different from slide period)
    float fade_period = 1.0f;
    float fade_phase = fmodf(think_time, fade_period) / fade_period;
    float alpha = 0.4f + 0.6f * (0.5f + 0.5f * sinf(fade_phase * 2.0f * M_PI));
    
    Color striker_color = (Color){ 255, 215, 0, (unsigned char)(alpha * 255) };
    Color line_color = (Color){ 255, 255, 255, (unsigned char)(alpha * 100) };
    
    DrawCircle((int)screen.x, (int)screen.y, L->striker_r_px, striker_color);
    DrawCircleLines((int)screen.x, (int)screen.y, L->striker_r_px, line_color);
}

void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float alpha, const Layout* L) {
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
    
    // Draw human figures for each seat - all positioned in WORLD coordinates relative to board geometry
    
    // Compute figure margin and offset from board geometry (not screen coordinates)
    float figure_margin = L->figure_scale * 8.0f;           // replaces hardcoded 12.0f
    float figure_offset = (float)L->board_size * 0.11f;     // replaces magic 40.0f
    
    // Convert pixel offsets to world units
    float margin_world = figure_margin / vp.world_to_screen;
    float offset_world = figure_offset / vp.world_to_screen;
    
    // Current time for halo pulse animation
    float current_time = (float)GetTime();
    
    // North seat (top) - WHITE team, faces down (angle = -PI/2)
    // World Y = BASELINE_Y_NORTH - margin (above north baseline), X = 0 (centered)
    float halo_pulse_n = 0.0f;
    if (current_turn_seat == SEAT_NORTH) {
        halo_pulse_n = (sinf(current_time * 2.0f) * 0.5f + 0.5f); // 0-1 pulse
    }
    Vec2 north_world = { 0.0f, BASELINE_Y_NORTH - margin_world };
    draw_human_figure(vp, L, north_world, -M_PI / 2.0f, TEAM_WHITE, current_turn_seat == SEAT_NORTH, halo_pulse_n);
    
    // South seat (bottom) - WHITE team, faces up (angle = PI/2)
    // World Y = BASELINE_Y_SOUTH + margin (below south baseline), X = 0 (centered)
    float halo_pulse_s = 0.0f;
    if (current_turn_seat == SEAT_SOUTH) {
        halo_pulse_s = (sinf(current_time * 2.0f) * 0.5f + 0.5f);
    }
    Vec2 south_world = { 0.0f, BASELINE_Y_SOUTH + margin_world };
    draw_human_figure(vp, L, south_world, M_PI / 2.0f, TEAM_WHITE, current_turn_seat == SEAT_SOUTH, halo_pulse_s);
    
    // East seat (right) - BLACK team, faces left (angle = PI)
    // World X = BASELINE_X_EAST + offset + body_length (further right so torso extends inward)
    // Y = 0 (centered)
    float halo_pulse_e = 0.0f;
    if (current_turn_seat == SEAT_EAST) {
        halo_pulse_e = (sinf(current_time * 2.0f) * 0.5f + 0.5f);
    }
    // Compute body length in world units (same as draw_human_figure): head_radius + gap + torso_height
    float head_radius = (float)L->board_size / 25.0f;
    float torso_height = (float)L->board_size / 12.0f;
    float gap = 2.0f; // pixels
    float body_length_world = (head_radius + gap + torso_height) / vp.world_to_screen;
    Vec2 east_world = { BASELINE_X_EAST + offset_world + body_length_world, 0.0f };
    draw_human_figure(vp, L, east_world, M_PI, TEAM_BLACK, current_turn_seat == SEAT_EAST, halo_pulse_e);
    
    // West seat (left) - BLACK team, faces right (angle = 0)
    // World X = BASELINE_X_WEST - offset - body_length (further left so torso extends inward)
    // Y = 0 (centered)
    float halo_pulse_w = 0.0f;
    if (current_turn_seat == SEAT_WEST) {
        halo_pulse_w = (sinf(current_time * 2.0f) * 0.5f + 0.5f);
    }
    Vec2 west_world = { BASELINE_X_WEST - offset_world - body_length_world, 0.0f };
    draw_human_figure(vp, L, west_world, 0.0f, TEAM_BLACK, current_turn_seat == SEAT_WEST, halo_pulse_w);
    
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
    
    // Striker (interpolated) - only draw if not in thinking phase (thinking draws its own)
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