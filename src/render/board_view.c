#include <math.h>
#include "board_view.h"
#include "piece_draw.h"
#include "effects.h"
#include "renderer.h"
#include "common/types.h"
#include "common/vecmath.h"
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
#define COLOR_WHITE_COIN_OUTLINE (Color){ 50, 50, 50, 230 }   // thin dark rim: light coins look bigger than dark ones otherwise
static Color coin_outline_color(PieceColor c) { return c == PIECE_WHITE ? COLOR_WHITE_COIN_OUTLINE : COLOR_LINE; }
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
    float fref = (float)L->board_size * FIG_SCALE;
    float head_radius = fref / 25.0f;
    float shoulder_half_width = fref / 18.0f;
    float shoulder_height = fref / 45.0f;
    float torso_height = fref / 12.0f;
    float torso_bottom_half_width = fref / 22.5f;
    
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

/* -----------------------------------------------------------------------------
 * Smooth visual state: nothing on screen teleports. The striker and each player figure keep a
 * tracked position and glide toward wherever they should be at VISUAL_SLIDE_SPEED (board units
 * per wall-clock second); during a shot the striker simply follows the physics body.
 * --------------------------------------------------------------------------- */
#define VISUAL_SLIDE_SPEED 2.0f

typedef struct {
    bool striker_valid;
    Vec2 striker;
    float fig[4];
    bool fig_valid;
    bool was_gone;      /* the striker fell into a pocket during the last shot */
    float appear;       /* 0..1 fade-in of a fresh striker handed to the next player */
} VisualState;

static VisualState g_vis = { .appear = 1.0f };
static BoardViewDebug g_dbg;

void board_view_get_debug(BoardViewDebug* out) {
    *out = g_dbg;
    out->striker_valid = g_vis.striker_valid;
    out->striker_vis = g_vis.striker;
    for (int i = 0; i < 4; i++) out->figures[i] = g_vis.fig[i];
}

static float approach_f(float cur, float target, float max_step) {
    float d = target - cur;
    if (d > max_step) return cur + max_step;
    if (d < -max_step) return cur - max_step;
    return target;
}

static Vec2 approach_v(Vec2 cur, Vec2 target, float max_step) {
    float dx = target.x - cur.x, dy = target.y - cur.y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist <= max_step || dist < 1e-6f) return target;
    float k = max_step / dist;
    return (Vec2){ cur.x + dx * k, cur.y + dy * k };
}

/* Compute thinking striker baseline coordinate for a given seat and time.
 * Returns the oscillating baseline coordinate (x for N/S, y for E/W).
 * Mirrors the logic in effects.c:draw_thinking_striker()
 */
static float compute_thinking_striker_baseline_coord(Seat seat, double wall_time) {
    // Triangle wave along the baseline (continuous, no jump): one full there-and-back every 2.4 s
    float slide_period = 2.4f;
    float slide_phase = my_fmodf((float)wall_time, slide_period) / slide_period;  // 0 to 1
    float slide_t = slide_phase <= 0.5f ? slide_phase * 2.0f : (1.0f - slide_phase) * 2.0f;  // 0->1->0
    float slide_offset = BASELINE_MIN_OFFSET + slide_t * (BASELINE_MAX_OFFSET - BASELINE_MIN_OFFSET);
    return slide_offset;
}

static Vec2 thinking_striker_world(Seat seat, double wall_time) {
    float c = compute_thinking_striker_baseline_coord(seat, wall_time);
    switch (seat) {
        case SEAT_NORTH: return (Vec2){ c, BASELINE_Y_NORTH };
        case SEAT_SOUTH: return (Vec2){ c, BASELINE_Y_SOUTH };
        case SEAT_EAST:  return (Vec2){ BASELINE_X_EAST, c };
        default:         return (Vec2){ BASELINE_X_WEST, c };
    }
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
/* raylib culls clockwise triangles, so draw both windings to be sure the arrowhead is visible */
static void draw_tri_any_winding(Vector2 a, Vector2 b, Vector2 c, Color col) {
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}

static void draw_aim_preview_line(Viewport vp, const GameState* game, const Layout* L, Vec2 striker_pos) {
    if (!game || !game->computed_shot_valid) return;

    float aim_angle = game->computed_shot_plan.aim_angle;
    float power = game->computed_shot_plan.power;

    /* Length is strictly proportional to the strike force: full power = AIM_LINE_FULL_POWER_LEN board widths */
    float clamped_len = aim_line_length(power);
    (void)L;

    // Convert to screen coordinates
    Vec2 start_screen = math_world_to_screen(vp, striker_pos);
    Vec2 end_world = { striker_pos.x + cosf(aim_angle) * clamped_len, striker_pos.y + sinf(aim_angle) * clamped_len };
    Vec2 end_screen = math_world_to_screen(vp, end_world);
    g_dbg.aim_drawn = true;
    g_dbg.aim_start = striker_pos;
    g_dbg.aim_end = end_world;
    
    // Line thickness in screen pixels (at least 3px)
    float thickness = my_fmaxf(3.0f, math_world_to_screen_dist(vp, 0.01f));
    
    // Draw line with dark outline (draw outline first, then line on top)
    // Outline: slightly thicker, black
    DrawLineEx((Vector2){start_screen.x, start_screen.y}, (Vector2){end_screen.x, end_screen.y}, thickness + 2.0f, COLOR_AIM_PREVIEW_OUTLINE);
    // Main line: yellow
    DrawLineEx((Vector2){start_screen.x, start_screen.y}, (Vector2){end_screen.x, end_screen.y}, thickness, COLOR_AIM_PREVIEW_LINE);
    
    // Draw arrowhead at far end
    // Arrowhead: triangle pointing along line direction
    float arrow_size = my_fmaxf(14.0f, thickness * 4.5f);
    /* Direction in SCREEN space (world y is flipped on screen), so the head points along the drawn line */
    float sdx = end_screen.x - start_screen.x, sdy = end_screen.y - start_screen.y;
    float slen = sqrtf(sdx * sdx + sdy * sdy);
    Vec2 dir = (slen > 1e-3f) ? (Vec2){ sdx / slen, sdy / slen } : (Vec2){ 1.0f, 0.0f };
    Vec2 perp = { -dir.y, dir.x };
    
    Vec2 arrow_tip = end_screen;
    Vec2 arrow_base_left = { end_screen.x - dir.x * arrow_size + perp.x * (arrow_size * 0.5f), end_screen.y - dir.y * arrow_size + perp.y * (arrow_size * 0.5f) };
    Vec2 arrow_base_right = { end_screen.x - dir.x * arrow_size - perp.x * (arrow_size * 0.5f), end_screen.y - dir.y * arrow_size - perp.y * (arrow_size * 0.5f) };
    
    // Arrowhead outline
    draw_tri_any_winding(
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
    draw_tri_any_winding(
        (Vector2){ arrow_tip_inset.x, arrow_tip_inset.y },
        (Vector2){ arrow_base_left_inset.x, arrow_base_left_inset.y },
        (Vector2){ arrow_base_right_inset.x, arrow_base_right_inset.y },
        COLOR_AIM_PREVIEW_ARROW
    );
}

void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float alpha, const Layout* L, int game_phase, const GameState* game, double placement_timer) {
    g_dbg.aim_drawn = false;
    g_dbg.drawn_from_physics_mask = 0;
    // Determine current turn seat from game turn (not striker owner, which is stale during THINKING/PLACEMENT/AIM_PREVIEW)
    Seat current_turn_seat = game ? game->turn_seat : board->striker.owner_seat;
    
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
    float required_margin_px = (float)FIG_MARGIN_PX;
    float margin_world = required_margin_px / vp.world_to_screen;
    
    // Figure body length in world units (head_radius + gap + torso_height)
    float head_radius = (float)L->board_size * FIG_SCALE / 25.0f;
    float torso_height = (float)L->board_size * FIG_SCALE / 12.0f;
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
    
    // ---- Smooth visual state update (striker + figures)
    {
        static double last_wall = -1.0;
        float frame_dt = (last_wall < 0.0) ? 0.0f : (float)(wall_time - last_wall);
        last_wall = wall_time;
        if (frame_dt < 0.0f) frame_dt = 0.0f;
        if (frame_dt > 0.1f) frame_dt = 0.1f;
        float step = VISUAL_SLIDE_SPEED * frame_dt;
        bool planning = is_thinking || is_placement || is_aim_preview;
        bool striker_gone = physics && physics_is_striker_pocketed(physics);
        if (striker_gone && !planning) g_vis.was_gone = true;
        if (g_vis.appear < 1.0f) { g_vis.appear += frame_dt / 0.6f; if (g_vis.appear > 1.0f) g_vis.appear = 1.0f; }

        if (game && is_thinking) {
            Vec2 target = thinking_striker_world(game->turn_seat, wall_time);
            if (g_vis.was_gone) { g_vis.striker = target; g_vis.appear = 0.0f; g_vis.was_gone = false; g_vis.striker_valid = true; }
            g_vis.striker = g_vis.striker_valid ? approach_v(g_vis.striker, target, step) : target;
            g_vis.striker_valid = true;
        } else if (game && (is_placement || is_aim_preview)) {
            Vec2 target = board->striker.position;
            if (g_vis.was_gone) { g_vis.striker = target; g_vis.appear = 0.0f; g_vis.was_gone = false; g_vis.striker_valid = true; }
            g_vis.striker = g_vis.striker_valid ? approach_v(g_vis.striker, target, step) : target;
            g_vis.striker_valid = true;
        } else if (use_physics && !striker_gone && !board->striker.pocketed) {
            g_vis.striker = vec2_lerp(prev_striker_pos, curr_striker_pos, alpha);
            g_vis.striker_valid = true;
        }

        for (int s = 0; s < 4; s++) {
            float target = 0.0f;
            if (game && planning && game->turn_seat == (Seat)s) {
                target = (s == SEAT_NORTH || s == SEAT_SOUTH) ? g_vis.striker.x : g_vis.striker.y;
            }
            g_vis.fig[s] = g_vis.fig_valid ? approach_f(g_vis.fig[s], target, step) : target;
        }
        g_vis.fig_valid = true;
        north_world.x = g_vis.fig[SEAT_NORTH];
        south_world.x = g_vis.fig[SEAT_SOUTH];
        east_world.y  = g_vis.fig[SEAT_EAST];
        west_world.y  = g_vis.fig[SEAT_WEST];
    }

    // Draw human figures for all four seats
    draw_human_figure(vp, L, north_world, -M_PI / 2.0f, board_team_of_seat(board, SEAT_NORTH), current_turn_seat == SEAT_NORTH, halo_pulse_n, figure_alpha);
    draw_human_figure(vp, L, south_world, M_PI / 2.0f, board_team_of_seat(board, SEAT_SOUTH), current_turn_seat == SEAT_SOUTH, halo_pulse_s, figure_alpha);
    draw_human_figure(vp, L, east_world, M_PI, board_team_of_seat(board, SEAT_EAST), current_turn_seat == SEAT_EAST, halo_pulse_e, figure_alpha);
    draw_human_figure(vp, L, west_world, 0.0f, board_team_of_seat(board, SEAT_WEST), current_turn_seat == SEAT_WEST, halo_pulse_w, figure_alpha);
    
    // Pockets
    float pocket_r = math_world_to_screen_dist(vp, POCKET_RADIUS_NORM);
    for (int i = 0; i < 4; i++) {
        Vec2 p = math_world_to_screen(vp, POCKET_CENTERS[i]);
        DrawCircle((int)p.x, (int)p.y, pocket_r, COLOR_POCKET);
    }
    
    // Pieces (interpolated from physics for smooth animation, fall back to board state)
    for (int i = 0; i < MAX_PIECES; i++) {
        Vec2 pos;
        bool phys_pocketed = use_physics && physics_is_piece_pocketed(physics, i);
        if (!board_view_piece_draw_pos(board->pieces[i].on_board, phys_pocketed, use_physics,
                                       board->pieces[i].position,
                                       prev_positions[i], curr_positions[i], alpha, &pos)) {
            continue;
        }
        if (use_physics) g_dbg.drawn_from_physics_mask |= (1u << i);
        
        Vec2 screen = math_world_to_screen(vp, pos);
        float piece_r = math_world_to_screen_dist(vp, PIECE_RADIUS_NORM);
        
        Color c;
        if (board->pieces[i].color == PIECE_WHITE) c = COLOR_WHITE_PIECE;
        else if (board->pieces[i].color == PIECE_BLACK) c = COLOR_BLACK_PIECE;
        else c = COLOR_QUEEN;
        
        DrawCircle((int)screen.x, (int)screen.y, piece_r, c);
        DrawCircleLines((int)screen.x, (int)screen.y, piece_r, coin_outline_color(board->pieces[i].color));
    }
    
    // Pocketed pieces (drawn at their pocketed positions near corners)
    for (int i = 0; i < board->pocketed_count; i++) {
        if (!board->pocketed_pieces[i].pocketed) continue;
        /* still sliding/sinking into the pocket: the slot copy appears when that ends */
        if (effects_piece_falling(board->pocketed_pieces[i].id)) continue;
        
        Vec2 pos = board->pocketed_pieces[i].pocketed_position;
        Vec2 screen = math_world_to_screen(vp, pos);
        float piece_r = math_world_to_screen_dist(vp, PIECE_RADIUS_NORM);
        
        Color c;
        if (board->pocketed_pieces[i].color == PIECE_WHITE) c = COLOR_WHITE_PIECE;
        else if (board->pocketed_pieces[i].color == PIECE_BLACK) c = COLOR_BLACK_PIECE;
        else c = COLOR_QUEEN;
        
        DrawCircle((int)screen.x, (int)screen.y, piece_r, c);
        DrawCircleLines((int)screen.x, (int)screen.y, piece_r, coin_outline_color(board->pocketed_pieces[i].color));
    }
    
    // Striker: drawn at its tracked visual position (glides between turns, follows physics during a shot)
    if (board->striker.on_baseline && !board->striker.pocketed && g_vis.striker_valid &&
        !(physics && physics_is_striker_pocketed(physics) && !(is_thinking || is_placement || is_aim_preview))) {
        Vec2 screen = math_world_to_screen(vp, g_vis.striker);
        float striker_r = math_world_to_screen_dist(vp, STRIKER_RADIUS_NORM);
        if (is_thinking) {
            float fa = compute_flash_alpha(wall_time);
            DrawCircle((int)screen.x, (int)screen.y, striker_r, (Color){ 255, 215, 0, (unsigned char)(fa * g_vis.appear * 255) });
            DrawCircleLines((int)screen.x, (int)screen.y, striker_r, (Color){ 255, 255, 255, (unsigned char)(fa * g_vis.appear * 100) });
        } else {
            Color sc = COLOR_STRIKER, lc = COLOR_LINE;
            sc.a = (unsigned char)(sc.a * g_vis.appear);
            lc.a = (unsigned char)(lc.a * g_vis.appear);
            DrawCircle((int)screen.x, (int)screen.y, striker_r, sc);
            DrawCircleLines((int)screen.x, (int)screen.y, striker_r, lc);
        }
    }

    // AIM_PREVIEW phase: draw aim preview line (INSIDE camera, AFTER striker draw)
    // Only draw when in AIM_PREVIEW phase AND computed shot is valid (phase gate)
    // Also ensure striker is stationary (velocity near zero) to prevent aim line during movement
    if (is_aim_preview && game && game->computed_shot_valid) {
        // Check striker velocity is near zero (aim line should not show during movement)
        float striker_speed = math_sqrtf(game->board.striker.velocity.x * game->board.striker.velocity.x + 
                                         game->board.striker.velocity.y * game->board.striker.velocity.y);
        if (striker_speed <= SETTLE_SPEED_EPS) {
            draw_aim_preview_line(vp, game, L, g_vis.striker);
        }
    }
}