#include <math.h>
#include "board_view.h"
#include "renderer.h"
#include "common/types.h"
#include "common/math.h"
#include "physics/physics.h"
#define __USE_MINGW_ANSI_STDIO 1
#include <raylib.h>

/* Inline math functions to avoid implicit declaration issues */
static inline float my_fmodf(float x, float y) {
    return x - y * (float)((int)(x / y));
}
static inline float my_fmaxf(float a, float b) {
    return (a > b) ? a : b;
}
static inline float my_fminf(float a, float b) {
    return (a < b) ? a : b;
}

/* Shared flash alpha computation for syncing figure and striker */
static inline float compute_flash_alpha(double wall_time) {
    // alpha = 0.4 + 0.6 * (0.5 + 0.5 * sin(2π * t)) where t = wall time in seconds, ~1Hz
    float t = (float)wall_time;
    return 0.4f + 0.6f * (0.5f + 0.5f * sinf(t * 2.0f * M_PI));
}

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

/* Aim preview line colors */
#define COLOR_AIM_PREVIEW_LINE (Color){ 255, 255, 0, 255 }    // Yellow
#define COLOR_AIM_PREVIEW_OUTLINE (Color){ 0, 0, 0, 255 }     // Black outline
#define COLOR_AIM_PREVIEW_ARROW (Color){ 255, 255, 0, 255 }   // Yellow arrowhead

/* Draw a stylised head-and-shoulders silhouette per spec:
 *    ○         (head: DrawCircle radius = L->board_size / 25)
 *   ┃┃         (shoulders: DrawEllipse half-width = L->board_size / 18, height = L->board_size / 45)
 *  ▄▄▄▄        (torso: filled trapezoid height = L->board_size / 12, bottom half-width = L->board_size / 22.5)
 * All shapes FILLED with team color, 2px outline accent
 */
static void draw_human_figure(Viewport vp, const Layout* L, Vec2 world_pos, float angle, Team team, bool is_current_turn, float halo_pulse, float alpha) {
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
    
    // Apply alpha to colors
    fill_color.a = (unsigned char)(fill_color.a * alpha);
    outline_color.a = (unsigned char)(outline_color.a * alpha);
    highlight_color.a = (unsigned char)(highlight_color.a * alpha);
    
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

/* Compute thinking striker baseline coordinate for a given seat and time.
 * Returns the oscillating baseline coordinate (x for N/S, y for E/W).
 * Mirrors the logic in effects.c:draw_thinking_striker()
 */
static float compute_thinking_striker_baseline_coord(Seat seat, double wall_time) {
    // Slide back and forth along baseline: one full pass every 1.5s
    float slide_period = 1.5f;
    float slide_phase = my_fmodf((float)wall_time, slide_period) / slide_period;  // 0 to 1
    // Map to ping-pong: 0->1->0
    float slide_t = slide_phase <= 0.5f ? slide_phase * 2.0f : (1.0f - slide_phase) * 2.0f;
    
    // Baseline limits in normalized coords
    float min_offset = BASELINE_MIN_OFFSET;
    float max_offset = BASELINE_MAX_OFFSET;
    float slide_offset = min_offset + slide_t * (max_offset - min_offset);
    
    // Alternate direction each half-period for visual variety
    if (slide_phase > 0.5f) slide_offset = max_offset - slide_t * (max_offset - min_offset);
    
    return slide_offset;
}

/* Draw aim preview line from striker position in aim direction.
 * Called INSIDE BeginMode2D() camera, AFTER striker draw.
 * Origin = striker's current rendered position (interpolated physics position if use_physics, else game->board.striker.position).
 * Direction = game->computed_shot_plan.aim_angle.
 * Natural length = game->computed_shot_plan.power * 0.5f * sqrtf(2.0f) (power * half-diagonal).
 * Clamped length = min(natural_len, distance_to_board_boundary(striker_pos, aim_angle) - 0.01f).
 * Draw solid line >=3px thick with visible arrowhead at far end.
 * Color: high contrast (YELLOW with dark outline).
 */
static void draw_aim_preview_line(Viewport vp, const GameState* game, const Layout* L, float alpha, bool use_physics, Vec2 curr_striker_pos, Vec2 prev_striker_pos) {
    if (!game || !game->computed_shot_valid) return;
    
    // Get striker's current rendered position
    Vec2 striker_pos;
    if (use_physics) {
        striker_pos = vec2_lerp(prev_striker_pos, curr_striker_pos, alpha);
    } else {
        striker_pos = game->board.striker.position;
    }
    
    float aim_angle = game->computed_shot_plan.aim_angle;
    float power = game->computed_shot_plan.power;
    
    // Natural length = power * half-diagonal (board diagonal = sqrt(2), half = sqrt(2)/2 = 0.5*sqrt(2))
    float natural_len = power * 0.5f * sqrtf(2.0f);
    
    // Distance to board boundary (cushion inner edge)
    float boundary_dist = distance_to_board_boundary(striker_pos, aim_angle);
    if (boundary_dist < 0.0f) boundary_dist = natural_len;  // fallback
    
    // Clamped length: stop just before cushion (0.01f margin)
    float clamped_len = my_fminf(natural_len, boundary_dist - 0.01f);
    if (clamped_len < 0.0f) clamped_len = 0.0f;
    
    // Convert to screen coordinates
    Vec2 start_screen = math_world_to_screen(vp, striker_pos);
    Vec2 end_world = { striker_pos.x + cosf(aim_angle) * clamped_len, striker_pos.y + sinf(aim_angle) * clamped_len };
    Vec2 end_screen = math_world_to_screen(vp, end_world);
    
    // Line thickness in screen pixels (at least 3px)
    float thickness = my_fmaxf(3.0f, math_world_to_screen_dist(vp, 0.01f));
    
    // Draw line with dark outline (draw outline first, then line on top)
    // Outline: slightly thicker, black
    DrawLineEx((Vector2){start_screen.x, start_screen.y}, (Vector2){end_screen.x, end_screen.y}, thickness + 2.0f, COLOR_AIM_PREVIEW_OUTLINE);
    // Main line: yellow
    DrawLineEx((Vector2){start_screen.x, start_screen.y}, (Vector2){end_screen.x, end_screen.y}, thickness, COLOR_AIM_PREVIEW_LINE);
    
    // Draw arrowhead at far end
    // Arrowhead: triangle pointing along line direction
    float arrow_size = my_fmaxf(8.0f, thickness * 3.0f);
    Vec2 dir = { cosf(aim_angle), sinf(aim_angle) };
    Vec2 perp = { -dir.y, dir.x };
    
    Vec2 arrow_tip = end_screen;
    Vec2 arrow_base_left = { end_screen.x - dir.x * arrow_size + perp.x * (arrow_size * 0.5f), end_screen.y - dir.y * arrow_size + perp.y * (arrow_size * 0.5f) };
    Vec2 arrow_base_right = { end_screen.x - dir.x * arrow_size - perp.x * (arrow_size * 0.5f), end_screen.y - dir.y * arrow_size - perp.y * (arrow_size * 0.5f) };
    
    // Arrowhead outline
    DrawTriangle(
        (Vector2){ arrow_tip.x, arrow_tip.y },
        (Vector2){ arrow_base_left.x, arrow_base_left.y },
        (Vector2){ arrow_base_right.x, arrow_base_right.y },
        COLOR_AIM_PREVIEW_OUTLINE
    );
    // Arrowhead fill (slightly smaller for outline effect)
    float inset = 1.0f;
    Vec2 arrow_tip_inset = { end_screen.x - dir.x * inset, end_screen.y - dir.y * inset };
    Vec2 arrow_base_left_inset = { arrow_tip_inset.x - dir.x * (arrow_size - inset) + perp.x * ((arrow_size - inset) * 0.5f), arrow_tip_inset.y - dir.y * (arrow_size - inset) + perp.y * ((arrow_size - inset) * 0.5f) };
    Vec2 arrow_base_right_inset = { arrow_tip_inset.x - dir.x * (arrow_size - inset) - perp.x * ((arrow_size - inset) * 0.5f), arrow_tip_inset.y - dir.y * (arrow_size - inset) - perp.y * ((arrow_size - inset) * 0.5f) };
    DrawTriangle(
        (Vector2){ arrow_tip_inset.x, arrow_tip_inset.y },
        (Vector2){ arrow_base_left_inset.x, arrow_base_left_inset.y },
        (Vector2){ arrow_base_right_inset.x, arrow_base_right_inset.y },
        COLOR_AIM_PREVIEW_ARROW
    );
}

void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float alpha, const Layout* L, int game_phase, const GameState* game, double placement_timer) {
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
    Vec2 curr_striker_pos = {0, 0};
    Vec2 prev_striker_pos = {0, 0};
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
    
    // Board surface - draw from camera-local (0, 0) to (board_size, board_size)
    // math_world_to_screen flips Y, so world top-left (-0.5, 0.5) maps to camera-local (0, 0)
    // and world bottom-right (0.5, -0.5) maps to camera-local (board_size, board_size)
    float board_half = BOARD_SIDE_NORM * 0.5f;
    Vec2 tl = math_world_to_screen(vp, (Vec2){ -board_half, board_half });   // World top-left -> camera-local (0, 0)
    Vec2 br = math_world_to_screen(vp, (Vec2){ board_half, -board_half });   // World bottom-right -> camera-local (board_size, board_size)
    float board_w = br.x - tl.x;
    float board_h = br.y - tl.y;
    DrawRectangle((int)tl.x, (int)tl.y, (int)board_w, (int)board_h, COLOR_BOARD);
    
    // Cushions
    float cushion_t = math_world_to_screen_dist(vp, CUSHION_THICKNESS);
    
    // Top cushion
    DrawRectangle((int)tl.x, (int)tl.y, (int)board_w, (int)cushion_t, COLOR_CUSHION);
    // Bottom cushion
    DrawRectangle((int)tl.x, (int)(br.y - cushion_t), (int)board_w, (int)cushion_t, COLOR_CUSHION);
    // Left cushion
    DrawRectangle((int)tl.x, (int)tl.y, (int)cushion_t, (int)board_h, COLOR_CUSHION);
    // Right cushion
    DrawRectangle((int)(br.x - cushion_t), (int)tl.y, (int)cushion_t, (int)board_h, COLOR_CUSHION);
    
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
    
    // =========================================================================
    // FIGURE POSITIONING LOGIC (R8-2, R8-3)
    // =========================================================================
    
    // Compute figure height in screen pixels: head_radius + gap + torso_height
    // Figure faces toward board center, so extends from head toward board
    // Margin from board boundary = required_margin, where required_margin = max(board_size/10, 24px)
    // The figure's NEAREST point (bottom of torso for N/S) should be at board_boundary + required_margin
    // Head center is at board_boundary + required_margin + body_length
    float required_margin_px = my_fmaxf((float)L->board_size / 10.0f, 24.0f);
    float margin_world = required_margin_px / vp.world_to_screen;
    
    // Figure body length in world units (head_radius + gap + torso_height)
    float head_radius = (float)L->board_size / 25.0f;
    float torso_height = (float)L->board_size / 12.0f;
    float gap = 2.0f; // pixels
    float body_length_world = (head_radius + gap + torso_height) / vp.world_to_screen;
    
    // Figure perpendicular offset: distance from cushion INNER edge to figure's nearest point
    // Figure faces toward board, so nearest point is bottom of torso (N/S) or side of torso (E/W)
    // Head center must be at: cushion_inner + margin + body_length
    // Board boundaries (where the playing area ends, at ±0.5)
    // The figure must stay entirely OUTSIDE the board boundaries (±0.5)
    const float BOARD_BOUNDARY_Y_NORTH = 0.5f;
    const float BOARD_BOUNDARY_Y_SOUTH = -0.5f;
    const float BOARD_BOUNDARY_X_EAST = 0.5f;
    const float BOARD_BOUNDARY_X_WEST = -0.5f;
    
    // Figure perpendicular offset: distance from board boundary to figure's nearest point
    // Figure faces toward board, so nearest point is bottom of torso (N/S) or side of torso (E/W)
    // Head center is at: board_boundary + margin + body_length
    // The fixed offset for head center = margin + body_length
    float figure_head_offset = margin_world + body_length_world;
    
    // Store fixed perpendicular offset per seat (world units) - NEVER changes
    // Figure head center at board_boundary + margin + body_length
    float figure_fixed_offset_world[4] = {
        figure_head_offset,  // SEAT_NORTH: head at BOARD_BOUNDARY_Y_NORTH + margin + body_length
        figure_head_offset,  // SEAT_EAST:  head at BOARD_BOUNDARY_X_EAST + margin + body_length
        figure_head_offset,  // SEAT_SOUTH: head at BOARD_BOUNDARY_Y_SOUTH - margin - body_length
        figure_head_offset   // SEAT_WEST:  head at BOARD_BOUNDARY_X_WEST - margin - body_length
    };
    
    // Figure alpha: pulsing 40-100% during THINKING and AIM_PREVIEW phases (synced with striker flash), 100% otherwise
    double wall_time = GetTime();
    float figure_alpha = 1.0f;
    bool is_thinking_or_preview = (game_phase == PHASE_THINKING || game_phase == PHASE_AIM_PREVIEW);
    if (is_thinking_or_preview) {
        figure_alpha = compute_flash_alpha(wall_time);
    }
    
    // Current time for halo pulse animation
    float current_time = (float)wall_time;
    
    // Per-seat figure world positions
    Vec2 north_world = {0, 0};
    Vec2 south_world = {0, 0};
    Vec2 east_world = {0, 0};
    Vec2 west_world = {0, 0};
    
    // Phase-specific logic
    bool is_thinking = (game_phase == PHASE_THINKING);
    bool is_placement = (game_phase == PHASE_PLACEMENT);
    bool is_aim_preview = (game_phase == PHASE_AIM_PREVIEW);
    
    // For PLACEMENT phase: ease from THINKING-end position to final position
    // placement_timer goes from PLACEMENT_HOLD_TIME (1.0s) down to 0
    // Progress = 1 - (placement_timer / PLACEMENT_HOLD_TIME), so 0 at start, 1 at end
    const float PLACEMENT_HOLD_TIME = 1.0f;
    float placement_progress = 0.0f;
    if (is_placement && placement_timer > 0.0) {
        placement_progress = 1.0f - (float)(placement_timer / PLACEMENT_HOLD_TIME);
        if (placement_progress < 0.0f) placement_progress = 0.0f;
        if (placement_progress > 1.0f) placement_progress = 1.0f;
    } else if (is_aim_preview) {
        // During AIM_PREVIEW, hold figure at final position (progress = 1.0)
        placement_progress = 1.0f;
    }
    
    // NORTH seat (top) - WHITE team, faces down (angle = -PI/2) toward board center
    // Figure extends DOWNWARD from head. Head center at CUSHION_INNER_Y_NORTH + margin + body_length.
    float halo_pulse_n = 0.0f;
    if (current_turn_seat == SEAT_NORTH) {
        halo_pulse_n = (sinf(current_time * 2.0f) * 0.5f + 0.5f); // 0-1 pulse
    }
    
    // Fixed perpendicular coordinate (y) for NORTH: head at BOARD_BOUNDARY_Y_NORTH + figure_fixed_offset_world
    // figure_fixed_offset_world already includes margin + body_length
    float north_fixed_y = BOARD_BOUNDARY_Y_NORTH + figure_fixed_offset_world[SEAT_NORTH];
    
    // Baseline-axis coordinate (x) varies by phase
    float north_baseline_x;
    if (is_thinking && game && game->turn_seat == SEAT_NORTH) {
        // THINKING: mirror striker's oscillating baseline coordinate
        north_baseline_x = compute_thinking_striker_baseline_coord(SEAT_NORTH, wall_time);
    } else if ((is_placement || is_aim_preview) && game && game->turn_seat == SEAT_NORTH) {
        // PLACEMENT: ease from THINKING-end position to final striker position
        // AIM_PREVIEW: hold at final position
        float thinking_x = compute_thinking_striker_baseline_coord(SEAT_NORTH, wall_time);
        float final_x = board->striker.position.x;  // striker already placed
        north_baseline_x = thinking_x + placement_progress * (final_x - thinking_x);
    } else {
        // IDLE, or not current turn: rest at center of baseline (0.0)
        north_baseline_x = 0.0f;
    }
    north_world = (Vec2){ north_baseline_x, north_fixed_y };
    
    // SOUTH seat (bottom) - WHITE team, faces up (angle = PI/2) toward board center
    // Figure extends UPWARD from head. Head center at BOARD_BOUNDARY_Y_SOUTH - margin - body_length.
    float halo_pulse_s = 0.0f;
    if (current_turn_seat == SEAT_SOUTH) {
        halo_pulse_s = (sinf(current_time * 2.0f) * 0.5f + 0.5f);
    }
    
    // Fixed perpendicular coordinate (y) for SOUTH: head at BOARD_BOUNDARY_Y_SOUTH - figure_fixed_offset_world
    // figure_fixed_offset_world already includes margin + body_length
    float south_fixed_y = BOARD_BOUNDARY_Y_SOUTH - figure_fixed_offset_world[SEAT_SOUTH];
    
    // Baseline-axis coordinate (x) varies by phase
    float south_baseline_x;
    if (is_thinking && game && game->turn_seat == SEAT_SOUTH) {
        south_baseline_x = compute_thinking_striker_baseline_coord(SEAT_SOUTH, wall_time);
    } else if ((is_placement || is_aim_preview) && game && game->turn_seat == SEAT_SOUTH) {
        float thinking_x = compute_thinking_striker_baseline_coord(SEAT_SOUTH, wall_time);
        float final_x = board->striker.position.x;
        south_baseline_x = thinking_x + placement_progress * (final_x - thinking_x);
    } else {
        south_baseline_x = 0.0f;
    }
    south_world = (Vec2){ south_baseline_x, south_fixed_y };
    
    // EAST seat (right) - BLACK team, faces left (angle = PI) toward board center
    // Figure extends LEFTWARD from head. Head center should be RIGHT of board boundary by margin + body_length.
    float halo_pulse_e = 0.0f;
    if (current_turn_seat == SEAT_EAST) {
        halo_pulse_e = (sinf(current_time * 2.0f) * 0.5f + 0.5f);
    }
    
    // Fixed perpendicular coordinate (x) for EAST: head at BOARD_BOUNDARY_X_EAST + figure_fixed_offset_world
    // figure_fixed_offset_world already includes margin + body_length
    float east_fixed_x = BOARD_BOUNDARY_X_EAST + figure_fixed_offset_world[SEAT_EAST];
    
    // Baseline-axis coordinate (y) varies by phase
    float east_baseline_y;
    if (is_thinking && game && game->turn_seat == SEAT_EAST) {
        east_baseline_y = compute_thinking_striker_baseline_coord(SEAT_EAST, wall_time);
    } else if ((is_placement || is_aim_preview) && game && game->turn_seat == SEAT_EAST) {
        float thinking_y = compute_thinking_striker_baseline_coord(SEAT_EAST, wall_time);
        float final_y = board->striker.position.y;
        east_baseline_y = thinking_y + placement_progress * (final_y - thinking_y);
    } else {
        east_baseline_y = 0.0f;
    }
    east_world = (Vec2){ east_fixed_x, east_baseline_y };
    
    // WEST seat (left) - BLACK team, faces right (angle = 0) toward board center
    // Figure extends RIGHTWARD from head. Head center should be LEFT of board boundary by margin + body_length.
    float halo_pulse_w = 0.0f;
    if (current_turn_seat == SEAT_WEST) {
        halo_pulse_w = (sinf(current_time * 2.0f) * 0.5f + 0.5f);
    }
    
    // Fixed perpendicular coordinate (x) for WEST: head at BOARD_BOUNDARY_X_WEST - figure_fixed_offset_world
    // figure_fixed_offset_world already includes margin + body_length
    float west_fixed_x = BOARD_BOUNDARY_X_WEST - figure_fixed_offset_world[SEAT_WEST];
    
    // Baseline-axis coordinate (y) varies by phase
    float west_baseline_y;
    if (is_thinking && game && game->turn_seat == SEAT_WEST) {
        west_baseline_y = compute_thinking_striker_baseline_coord(SEAT_WEST, wall_time);
    } else if ((is_placement || is_aim_preview) && game && game->turn_seat == SEAT_WEST) {
        float thinking_y = compute_thinking_striker_baseline_coord(SEAT_WEST, wall_time);
        float final_y = board->striker.position.y;
        west_baseline_y = thinking_y + placement_progress * (final_y - thinking_y);
    } else {
        west_baseline_y = 0.0f;
    }
    west_world = (Vec2){ west_fixed_x, west_baseline_y };
    
    // Draw human figures for all four seats
    draw_human_figure(vp, L, north_world, -M_PI / 2.0f, TEAM_WHITE, current_turn_seat == SEAT_NORTH, halo_pulse_n, figure_alpha);
    draw_human_figure(vp, L, south_world, M_PI / 2.0f, TEAM_WHITE, current_turn_seat == SEAT_SOUTH, halo_pulse_s, figure_alpha);
    draw_human_figure(vp, L, east_world, M_PI, TEAM_BLACK, current_turn_seat == SEAT_EAST, halo_pulse_e, figure_alpha);
    draw_human_figure(vp, L, west_world, 0.0f, TEAM_BLACK, current_turn_seat == SEAT_WEST, halo_pulse_w, figure_alpha);
    
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
    
    // Striker (interpolated) - only draw if not in thinking phase (thinking draws its own in effects)
    if (board->striker.on_baseline && !board->striker.pocketed && game_phase != PHASE_THINKING) {
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
    
    // AIM_PREVIEW phase: draw aim preview line (INSIDE camera, AFTER striker draw)
    // Only draw when in AIM_PREVIEW phase AND computed shot is valid (phase gate)
    if (is_aim_preview && game && game->computed_shot_valid) {
        draw_aim_preview_line(vp, game, L, alpha, use_physics, curr_striker_pos, prev_striker_pos);
    }
}