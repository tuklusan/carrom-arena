#include <math.h>
#include "board_view.h"
#include "piece_draw.h"
#include "effects.h"
#include "theme.h"
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

/* Shared flash alpha computation for syncing figure and striker */
static inline float compute_flash_alpha(double wall_time) {
    // alpha = 0.4 + 0.6 * (0.5 + 0.5 * sin(2π * t)) where t = wall time in seconds, ~1Hz
    float t = (float)wall_time;
    return 0.4f + 0.6f * (0.5f + 0.5f * sinf(t * 2.0f * M_PI));
}

/* Colors */
#define COLOR_BOARD (Color){ 10, 20, 40, 120 }         // tinted glass: the backdrop shows through, a shade darker (visual only)
#define COLOR_CUSHION (Color){ 100, 70, 40, 255 }     // Darker brown
#define COLOR_POCKET (Color){ 0, 0, 0, 255 }          // Black
#define COLOR_WHITE_PIECE (Color){ 240, 240, 240, 255 }
#define COLOR_BLACK_PIECE (Color){ 62, 64, 74, 255 }       /* charcoal, with a silver rim: readable on the dark glass */
#define COLOR_QUEEN (Color){ 220, 30, 30, 255 }       // Red
#define COLOR_STRIKER (Color){ 255, 215, 0, 255 }     // Gold
#define COLOR_LINE (Color){ 255, 255, 255, 100 }      // White translucent
#define COLOR_WHITE_COIN_OUTLINE (Color){ 50, 50, 50, 230 }   // thin dark rim: light coins look bigger than dark ones otherwise
#define COLOR_BLACK_COIN_RIM (Color){ 200, 205, 215, 255 }
static Color coin_outline_color(PieceColor c) { return c == PIECE_WHITE ? COLOR_WHITE_COIN_OUTLINE : (c == PIECE_BLACK ? COLOR_BLACK_COIN_RIM : COLOR_LINE); }
/* the rim: 1 px for the light coins, 2 px for the dark ones */
static void draw_coin_rim(Vec2 screen, float r, PieceColor c) {
    Color k = coin_outline_color(c);
    DrawCircleLines((int)screen.x, (int)screen.y, r, k);
    if (c == PIECE_BLACK) DrawCircleLines((int)screen.x, (int)screen.y, r - 1.0f, k);
}
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

static void draw_tri_any_winding(Vector2 a, Vector2 b, Vector2 c, Color col);

/* A rectangle in the robot's own frame: u runs from the head toward the board, v to the robot's right. */
typedef struct { Vec2 o, back, right; } RobotFrame;

static Vector2 robot_pt(const RobotFrame* f, float u, float v) {
    return (Vector2){ f->o.x + f->back.x * u + f->right.x * v, f->o.y + f->back.y * u + f->right.y * v };
}

static void robot_box(const RobotFrame* f, float u0, float u1, float v0, float v1, Color fill, Color line) {
    Vector2 a = robot_pt(f, u0, v0), b = robot_pt(f, u0, v1), c = robot_pt(f, u1, v1), d = robot_pt(f, u1, v0);
    draw_tri_any_winding(a, b, c, fill);
    draw_tri_any_winding(a, c, d, fill);
    DrawLineEx(a, b, 1.5f, line);
    DrawLineEx(b, c, 1.5f, line);
    DrawLineEx(c, d, 1.5f, line);
    DrawLineEx(d, a, 1.5f, line);
}

/* Like robot_box, but rotated by `ang` about the pivot (pu, pv) in the robot frame (used for flapping arms). */
static void robot_rbox(const RobotFrame* f, float pu, float pv, float ang, float u0, float u1, float v0, float v1, Color fill, Color line) {
    float c = cosf(ang), sn = sinf(ang);
    float us[4] = { u0, u0, u1, u1 }, vs[4] = { v0, v1, v1, v0 };
    Vector2 p[4];
    for (int i = 0; i < 4; i++) {
        float du = us[i] - pu, dv = vs[i] - pv;
        p[i] = robot_pt(f, pu + du * c - dv * sn, pv + du * sn + dv * c);
    }
    draw_tri_any_winding(p[0], p[1], p[2], fill);
    draw_tri_any_winding(p[0], p[2], p[3], fill);
    for (int i = 0; i < 4; i++) DrawLineEx(p[i], p[(i + 1) % 4], 1.5f, line);
}

/* Idle animation for a waiting robot: every seat gets its own incommensurate frequencies and phases, so no two match. */
static float idle_wave(int seat, int k, float t) {
    float fa = 0.9f + 0.37f * (float)seat + 0.23f * (float)k;
    float fb = 0.5f + 0.29f * (float)((seat * 3 + k) % 5);
    float pa = 1.7f * (float)(seat + 1) + 2.3f * (float)k, pb = 0.9f * (float)(seat + 2) + 1.1f * (float)k;
    return 0.6f * sinf(t * fa + pa) + 0.4f * sinf(t * fb + pb);   /* -1..1, irregular */
}

/* The robots belong to the PAIRS, not to a coin colour: north/south are always red, east/west always blue. (The `team`
 * argument keeps the old name: TEAM_WHITE draws red, TEAM_BLACK blue.) */
/* A robot player, seen from above: antenna and head at the seat, arms, shoulders and a chest panel reaching toward the board.
 * `angle` is the direction pointing away from the board (as the old figures used it), so the body extends the other way. */
static void draw_human_figure(Viewport vp, const Layout* L, Vec2 world_pos, float angle, Team team, bool is_current_turn, float halo_pulse, float alpha, int seat, float t) {
    Vec2 screen = math_world_to_screen(vp, world_pos);
    float fref = (float)L->board_size * FIG_SCALE;
    float hr = fref / 25.0f;                       /* head half-size */

    Color base  = (team == TEAM_WHITE) ? THEME_RED : THEME_BLUE;
    Color light = (team == TEAM_WHITE) ? THEME_RED_LIGHT : THEME_BLUE_LIGHT;
    Color dark  = (team == TEAM_WHITE) ? THEME_RED_DARK : THEME_BLUE_DARK;
    Color line  = (Color){ 20, 20, 28, 255 };
    Color steel = (Color){ 150, 156, 168, 255 };
    Color eye   = (Color){ 255, 236, 110, 255 };
    Color* all[] = { &base, &light, &dark, &line, &steel, &eye };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) all[i]->a = (unsigned char)(all[i]->a * alpha);

    Vec2 away = { cosf(angle), sinf(angle) };      /* pointing away from the board */
    RobotFrame f = { screen, { -away.x, -away.y }, { away.y, -away.x } };

    float gap = 2.0f;
    float sh0 = hr + gap;                          /* shoulders start */
    float sh1 = sh0 + 0.75f * hr;
    float torso_end = sh0 + fref / 12.0f;          /* same reach as the old figure */

    /* arms hang from the shoulders, with square hands; a waiting robot flaps each arm on its own, swinging about the shoulder */
    float arm_v = 1.3f * hr;
    float flap_r = 0.0f, flap_l = 0.0f;
    if (!is_current_turn) { flap_r = 0.9f * idle_wave(seat, 0, t * 1.8f); flap_l = 0.9f * idle_wave(seat, 1, t * 1.8f); }
    else { flap_r = 14.0f * t; flap_l = 10.5f * t + 2.0f; }   /* the robot whose turn it is spins BOTH arms fast (about 2 and 1.7 turns a second), out of step with each other */
    float pu = sh0 + 0.1f * hr;
    robot_rbox(&f, pu, arm_v + 0.2f * hr, flap_r, sh0 + 0.1f * hr, torso_end - 0.2f * hr, arm_v, arm_v + 0.4f * hr, steel, line);
    robot_rbox(&f, pu, arm_v + 0.2f * hr, flap_r, torso_end - 0.55f * hr, torso_end + 0.15f * hr, arm_v - 0.05f * hr, arm_v + 0.45f * hr, dark, line);
    robot_rbox(&f, pu, -arm_v - 0.2f * hr, flap_l, sh0 + 0.1f * hr, torso_end - 0.2f * hr, -arm_v - 0.4f * hr, -arm_v, steel, line);
    robot_rbox(&f, pu, -arm_v - 0.2f * hr, flap_l, torso_end - 0.55f * hr, torso_end + 0.15f * hr, -arm_v - 0.45f * hr, -arm_v + 0.05f * hr, dark, line);
    /* torso with a chest panel and three lights */
    robot_box(&f, sh0, torso_end, -1.1f * hr, 1.1f * hr, base, line);
    robot_box(&f, sh0 + 0.9f * hr, torso_end - 0.25f * hr, -0.8f * hr, 0.8f * hr, dark, line);
    for (int k = -1; k <= 1; k++) {
        Vector2 p = robot_pt(&f, sh0 + 1.35f * hr, (float)k * 0.42f * hr);
        DrawCircleV(p, hr * 0.15f, eye);
    }
    /* shoulders */
    robot_box(&f, sh0, sh1, -1.45f * hr, 1.45f * hr, light, line);
    /* neck */
    robot_box(&f, hr - 1.0f, sh0 + 1.0f, -0.32f * hr, 0.32f * hr, steel, line);
    /* head, with two eyes toward the board and a mouth grille */
    robot_box(&f, -hr, hr, -1.05f * hr, 1.05f * hr, light, line);
    robot_box(&f, 0.05f * hr, 0.55f * hr, -0.7f * hr, -0.2f * hr, eye, line);
    robot_box(&f, 0.05f * hr, 0.55f * hr, 0.2f * hr, 0.7f * hr, eye, line);
    /* pupils: a waiting robot rolls its eyes along its own circle, at its own speed and direction */
    if (!is_current_turn) {
        float dir = (seat & 1) ? -1.0f : 1.0f;
        float ph = dir * t * (1.6f + 0.55f * (float)seat) + 1.3f * (float)seat + 0.8f * idle_wave(seat, 2, t);
        float ru = 0.13f * hr * cosf(ph), rv = 0.13f * hr * sinf(ph);
        Vector2 pl = robot_pt(&f, 0.3f * hr + ru, -0.45f * hr + rv), pr = robot_pt(&f, 0.3f * hr + ru, 0.45f * hr + rv);
        DrawCircleV(pl, hr * 0.11f, (Color){ 20, 20, 28, (unsigned char)(255 * alpha) });
        DrawCircleV(pr, hr * 0.11f, (Color){ 20, 20, 28, (unsigned char)(255 * alpha) });
    }
    robot_box(&f, 0.72f * hr, 0.9f * hr, -0.5f * hr, 0.5f * hr, dark, line);
    /* antenna with a ball */
    Vector2 a0 = robot_pt(&f, -hr, 0.0f), a1 = robot_pt(&f, -1.75f * hr, 0.0f);
    DrawLineEx(a0, a1, 2.0f, line);
    DrawCircleV(a1, hr * 0.3f, is_current_turn ? COLOR_TURN_HIGHLIGHT : base);
    DrawCircleLines((int)a1.x, (int)a1.y, hr * 0.3f, line);

    /* if it is this robot's turn, a pulsing gold halo ring around the head */
    if (is_current_turn) {
        float halo_r_inner = L->figure_halo_base_r + halo_pulse * 5.0f * L->figure_scale;
        float halo_r_outer = halo_r_inner + 2.0f * L->figure_scale;
        DrawRing((Vector2){ screen.x, screen.y }, halo_r_inner, halo_r_outer, 0, 360, 32, COLOR_TURN_HIGHLIGHT);
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
    (void)is_thinking_or_preview;   /* the figures no longer fade; waiting robots animate instead */
    
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
    draw_human_figure(vp, L, north_world, -M_PI / 2.0f, TEAM_WHITE, current_turn_seat == SEAT_NORTH, halo_pulse_n, figure_alpha, SEAT_NORTH, current_time);
    draw_human_figure(vp, L, south_world, M_PI / 2.0f, TEAM_WHITE, current_turn_seat == SEAT_SOUTH, halo_pulse_s, figure_alpha, SEAT_SOUTH, current_time);
    draw_human_figure(vp, L, east_world, 0.0f, TEAM_BLACK, current_turn_seat == SEAT_EAST, halo_pulse_e, figure_alpha, SEAT_EAST, current_time);
    draw_human_figure(vp, L, west_world, M_PI, TEAM_BLACK, current_turn_seat == SEAT_WEST, halo_pulse_w, figure_alpha, SEAT_WEST, current_time);
    
    // Pockets
    float pocket_r = math_world_to_screen_dist(vp, POCKET_RADIUS_NORM);
    for (int i = 0; i < 4; i++) {
        Vec2 p = math_world_to_screen(vp, POCKET_CENTERS[i]);
        DrawCircle((int)p.x, (int)p.y, pocket_r, COLOR_POCKET);
    }
    
    // Pieces (interpolated from physics for smooth animation, fall back to board state)
    for (int i = 0; i < MAX_PIECES; i++) {
        Vec2 pos;
        if (effects_piece_returning(i)) continue;   /* it is sliding back from its pocket: effects.c draws it */
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
        draw_coin_rim(screen, piece_r, board->pieces[i].color);
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
        draw_coin_rim(screen, piece_r, board->pocketed_pieces[i].color);
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