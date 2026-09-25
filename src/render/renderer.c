#include "renderer.h"
#include "board_view.h"
#include "effects.h"
#include "common/vecmath.h"
#include "common/types.h"
#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Inline math functions to avoid implicit declaration issues */
static inline float my_fminf(float a, float b) {
    return (a < b) ? a : b;
}

static const char* TITLE_TEXT = "SANYALnet Labs Carrom Arena";
static const char* COPYRIGHT_TEXT = "\xC2\xA9 Supratim Sanyal";  // UTF-8 ©
static const char* BLOG_LINK = "https://supratim-sanyal.blogspot.com/";

struct Renderer {
    int width;
    int height;
    bool capture_mode;
    bool hidden_window;
    bool paused;
    float playback_speed;
    bool debug_phase;           // Enable per-frame phase debug logging
    int debug_frame_count;      // Frame counter for debug logging (first 30 frames)
    Viewport viewport;
    RenderTexture2D capture_texture;
    char capture_dir[256];
    Layout current_layout;
};

/* Compute dynamic layout from actual screen dimensions */
void layout_compute(int sw, int sh, Layout* out) {
    out->sw = sw;
    out->sh = sh;
    float fsw = (float)sw;
    float fsh = (float)sh;

    out->title_band_h = (int)(fsh * 0.055f);
    out->footer_band_h = (int)(fsh * 0.07f);
    out->hud_w = (int)(fsw * 0.21f);
    out->right_sidebar_w = 0;
    out->board_region_x = out->hud_w;
    out->board_region_w = sw - out->hud_w - 8;
    out->body_h = sh - out->title_band_h - out->footer_band_h;

    // A figure (head + torso) plus its margin extends fig_extent = FIG_MARGIN_PX + 0.105 * board beyond
    // the board edge on every side; the board is the largest square that leaves room for all four.
    float fig_k = 0.105f;
    int fig_fixed = FIG_MARGIN_PX + 8;
    int vert = (int)(((float)out->body_h - 2.0f * (float)fig_fixed) / (1.0f + 2.0f * fig_k));
    int horiz = (int)(((float)out->board_region_w - 2.0f * (float)fig_fixed) / (1.0f + 2.0f * fig_k));
    int candidate = (vert < horiz) ? vert : horiz;
    if (candidate < 200) candidate = 200;
    if (candidate > 1400) candidate = 1400;
    out->board_size = candidate;
    out->figure_band_h = fig_fixed + (int)(fig_k * (float)candidate);

    // Board centred in the body (between title and footer) and in the region right of the HUD
    out->board_y = out->title_band_h + (out->body_h - out->board_size) / 2;
    out->board_x = out->board_region_x + (out->board_region_w - out->board_size) / 2;

    // N figure band center (above board, in the figure band)
    out->n_figure_center_y = out->title_band_h + out->figure_band_h / 2;
    // S figure band center (below board, in the figure band)
    out->s_figure_center_y = sh - out->footer_band_h - out->figure_band_h / 2;

    // Placement banner in N figure band
    out->placement_banner_y = out->title_band_h + 6;

    // Font sizes (same as before)
    float board_size_f = (float)out->board_size;
    out->font_size_title = (int)(board_size_f / 22.0f);
    if (out->font_size_title < 18) out->font_size_title = 18;
    if (out->font_size_title > 36) out->font_size_title = 36;
    out->font_size_footer_link = (int)(board_size_f / 30.0f);
    if (out->font_size_footer_link < 12) out->font_size_footer_link = 12;
    if (out->font_size_footer_link > 20) out->font_size_footer_link = 20;
    out->font_size_footer_copyright = (int)(board_size_f / 35.0f);
    if (out->font_size_footer_copyright < 10) out->font_size_footer_copyright = 10;
    if (out->font_size_footer_copyright > 16) out->font_size_footer_copyright = 16;
    out->font_size_hud = (int)(board_size_f / 25.0f);
    if (out->font_size_hud < 12) out->font_size_hud = 12;
    if (out->font_size_hud > 20) out->font_size_hud = 20;
    out->hud_line_height = (int)((float)out->font_size_hud * 1.3f);
    out->hud_start_y = out->title_band_h + 10;

    out->figure_scale = board_size_f / 360.0f;
    out->figure_halo_base_r = 10.0f * out->figure_scale;
    out->striker_r_px = board_size_f * STRIKER_RADIUS_NORM / BOARD_SIDE_NORM;
    out->piece_r_px = board_size_f * PIECE_RADIUS_NORM / BOARD_SIDE_NORM;
}

/* Helper to convert radians to degrees+minutes string */
static void format_angle_deg_min(float rad, char* buf, size_t buf_size) {
    float deg_f = rad * 180.0f / M_PI;
    if (deg_f < 0) deg_f += 360.0f;
    int deg = (int)deg_f;
    float min_f = (deg_f - (float)deg) * 60.0f;
    int min = (int)(min_f + 0.5f);  // Round to nearest minute
    if (min >= 60) { min -= 60; deg = (deg + 1) % 360; }
    snprintf(buf, buf_size, "Angle: %d°%02d'", deg, min);
}

/* Draw HUD sidebar in window coordinates using layout */
static void draw_hud_sidebar(const MatchState* match, const GameState* game, float playback_speed, const Layout* L) {
    float x = (float)10;
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
        "IDLE", "THINKING", "PLACEMENT", "AIM_PREVIEW", "AIMING", "SHOT", "SETTLING", "RESOLVING",
        "BOARD_OVER", "GAME_OVER", "MATCH_OVER"
    };
    DrawText(TextFormat("Phase: %s", phase_names[game->phase]), (int)x, (int)y, font, GREEN);
    y += lh;
    
    // AIM_PREVIEW commentary text block
    if (game->phase == PHASE_AIM_PREVIEW && game->computed_shot_valid) {
        float aim_angle = game->computed_shot_plan.aim_angle;
        float power = game->computed_shot_plan.power;
        
        char angle_deg_min[64];
        format_angle_deg_min(aim_angle, angle_deg_min, sizeof(angle_deg_min));
        
        DrawText(TextFormat("%s  (%.3f rad)", angle_deg_min, aim_angle), (int)x, (int)y, font, YELLOW);
        y += lh;
        
        DrawText(TextFormat("Power: %d%%", (int)(power * 100.0f + 0.5f)), (int)x, (int)y, font, YELLOW);
        y += lh;
    }
    
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

static void draw_title_bar(Renderer* r, const Layout* L) {
    (void)r;
    // Draw full-width title bar background line at even y for verification
    int title_bar_y = 8;
    DrawLineEx((Vector2){ 10, (float)title_bar_y }, (Vector2){ (float)(L->sw - 10), (float)title_bar_y }, 2, (Color){ 100, 100, 120, 255 });
    DrawLineEx((Vector2){ 10, (float)(title_bar_y + 1) }, (Vector2){ (float)(L->sw - 10), (float)(title_bar_y + 1) }, 1, (Color){ 100, 100, 120, 255 });
    
    int title_width = MeasureText(TITLE_TEXT, L->font_size_title);
    int title_x = (L->sw - title_width) / 2;
    DrawText(TITLE_TEXT, title_x, title_bar_y + 4, L->font_size_title, WHITE);
}

static void draw_footer_band(Renderer* r, const Layout* L) {
    (void)r;
    int rule_y = L->title_band_h + L->body_h + 5;
    // Ensure rule_y is even for verification script sampling
    if (rule_y % 2 != 0) rule_y++;
    // Draw 2px thick line for better detection
    DrawLineEx((Vector2){ 10, (float)rule_y }, (Vector2){ (float)(L->sw - 10), (float)rule_y }, 2, LIGHTGRAY);
    DrawLineEx((Vector2){ 10, (float)(rule_y + 1) }, (Vector2){ (float)(L->sw - 10), (float)(rule_y + 1) }, 1, LIGHTGRAY);
    
    int blog_link_width = MeasureText(BLOG_LINK, L->font_size_footer_link);
    int link_x = (L->sw - blog_link_width) / 2;
    int link_y = rule_y + 4;
    DrawText(BLOG_LINK, link_x, link_y, L->font_size_footer_link, LIGHTGRAY);
    
    int copyright_width = MeasureText(COPYRIGHT_TEXT, L->font_size_footer_copyright);
    int copyright_x = (L->sw - copyright_width) / 2;
    int copyright_y = link_y + L->font_size_footer_link + 2;
    DrawText(COPYRIGHT_TEXT, copyright_x, copyright_y, L->font_size_footer_copyright, (Color){ 180, 180, 180, 255 });
}

static void draw_placement_banner(Renderer* r, const GameState* game, double placement_timer, const Layout* L) {
    if (game->phase != PHASE_PLACEMENT) return;
    if (!game->board.striker.on_baseline || game->board.striker.pocketed) return;
    if (game->turn_seat != SEAT_NORTH) return;  // Only show for N seat (human-readable)

    Vec2 striker_pos = game->board.striker.position;
    char countdown_text[128];
    snprintf(countdown_text, sizeof(countdown_text),
             "Striker placed at (%.2f, %.2f) - striking in %.1fs",
             striker_pos.x, striker_pos.y, placement_timer);

    int font_size = L->font_size_hud;  // Or scale from board_size
    if (font_size < 16) font_size = 16;
    int text_width = MeasureText(countdown_text, font_size);
    int banner_w = text_width + 40;
    int banner_h = font_size + 16;
    int banner_x = (L->sw - banner_w) / 2;
    int banner_y = L->placement_banner_y;

    DrawRectangle(banner_x, banner_y, banner_w, banner_h, (Color){ 0, 0, 0, 200 });
    DrawRectangleLines(banner_x, banner_y, banner_w, banner_h, (Color){ 255, 215, 0, 255 });
    DrawText(countdown_text, banner_x + 20, banner_y + 8, font_size, (Color){ 255, 215, 0, 255 });
}

void renderer_draw_placement_banner(Renderer* r, const GameState* game, double placement_timer) {
    Layout* L = &r->current_layout;
    draw_placement_banner(r, game, placement_timer, L);
}

Renderer* renderer_create(int width, int height, const char* title, bool capture_mode, bool hidden_window, bool debug_phase, float initial_speed) {
    (void)title;
    
    Renderer* r = calloc(1, sizeof(Renderer));
    if (!r) return NULL;
    
    r->capture_mode = capture_mode;
    r->hidden_window = hidden_window;
    r->paused = false;
    r->playback_speed = initial_speed;  // R14f: use initial speed
    r->debug_phase = debug_phase;
    r->debug_frame_count = 0;
    r->width = width;
    r->height = height;
    
    r->viewport = (Viewport){0};
    
    unsigned int flags = FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT;
    if (hidden_window) {
        flags |= FLAG_WINDOW_HIDDEN;
    }
    SetConfigFlags(flags);
    InitWindow(width, height, "SANYALnet Labs Carrom Arena");
    SetTargetFPS(60);
    
    if (capture_mode) {
        r->capture_texture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    }
    
    return r;
}

void renderer_destroy(Renderer* r) {
    if (!r) return;
    
    if (r->capture_mode) {
        UnloadRenderTexture(r->capture_texture);
    }
    
    CloseWindow();
    free(r);
}

void renderer_poll_events(Renderer* r) {
    if (WindowShouldClose()) return;
    
    if (IsKeyPressed(KEY_SPACE)) {
        r->paused = !r->paused;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
    }
    if (IsKeyPressed(KEY_KP_ADD) || IsKeyPressed(KEY_EQUAL)) {
        r->playback_speed *= 2.0f;
        if (r->playback_speed > 4.0f) r->playback_speed = 4.0f;
    }
    if (IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_MINUS)) {
        r->playback_speed *= 0.5f;
        if (r->playback_speed < 0.05f) r->playback_speed = 0.05f;
    }
}

bool renderer_should_close(Renderer* r) {
    return WindowShouldClose();
}

bool renderer_is_paused(Renderer* r) {
    return r->paused;
}

float renderer_get_playback_speed(const Renderer* r) {
    return r->playback_speed;
}

void renderer_begin(Renderer* r) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    
    // Check if window was resized and recreate capture texture if needed
    if (r->capture_mode && (sw != r->width || sh != r->height)) {
        r->width = sw;
        r->height = sh;
        UnloadRenderTexture(r->capture_texture);
        r->capture_texture = LoadRenderTexture(sw, sh);
    }
    
    // Compute layout once per frame, store in renderer
    layout_compute(sw, sh, &r->current_layout);
    Layout* L = &r->current_layout;
    
    // Debug phase logging for first 30 frames
    if (r->debug_phase && r->debug_frame_count < 30) {
        fprintf(stderr, "[DEBUG-PHASE] frame=%d sw=%d sh=%d board_size=%d board_x=%d board_y=%d board_region_w=%d hud_w=%d title_band_h=%d footer_band_h=%d figure_band_h=%d\n",
                r->debug_frame_count, sw, sh, L->board_size, L->board_x, L->board_y,
                L->board_region_w, L->hud_w, L->title_band_h, L->footer_band_h, L->figure_band_h);
        r->debug_frame_count++;
    }
    
    if (r->capture_mode && r->hidden_window) {
        // Headless capture: render directly to texture, no main window drawing
        BeginTextureMode(r->capture_texture);
        ClearBackground((Color){ 30, 30, 40, 255 });
    } else {
        // Interactive or windowed capture: render to main window
        BeginDrawing();
        ClearBackground((Color){ 30, 30, 40, 255 });
    }
    
    draw_title_bar(r, L);
    draw_footer_band(r, L);
}

void renderer_draw_hud_sidebar(Renderer* r, const MatchState* match, const GameState* game, float playback_speed) {
    Layout* L = &r->current_layout;
    draw_hud_sidebar(match, game, playback_speed, L);
}

void renderer_begin_board(Renderer* r) {
    Layout* L = &r->current_layout;
    
    /* Update viewport for board drawing - using window coordinates directly */
    r->viewport.screen_width = L->board_size;
    r->viewport.screen_height = L->board_size;
    r->viewport.board_size_px = (float)L->board_size;
    r->viewport.board_center_px = (Vec2){ (float)L->board_x + (float)L->board_size * 0.5f, (float)L->board_y + (float)L->board_size * 0.5f };
    r->viewport.world_to_screen = (float)L->board_size / BOARD_SIDE_NORM;
}

void renderer_end_board(Renderer* r) {
    // No camera mode to end - using viewport-only rendering
    (void)r;
}

void renderer_end(Renderer* r) {
    if (r->capture_mode && r->hidden_window) {
        // Headless capture: end texture mode
        EndTextureMode();
    } else {
        // Interactive or windowed capture: end main window drawing
        EndDrawing();
    }
}

void renderer_draw_board(Renderer* r, const BoardState* board, const PhysicsWorld* physics, float alpha, int game_phase, const GameState* game, double placement_timer) {
    Layout* L = &r->current_layout;
    
    /* Create viewport that maps world coordinates directly to window coordinates */
    Viewport vp = (Viewport){
        .screen_width = L->board_size,
        .screen_height = L->board_size,
        .board_size_px = (float)L->board_size,
        .board_center_px = (Vec2){ (float)L->board_x + (float)L->board_size * 0.5f, (float)L->board_y + (float)L->board_size * 0.5f },
        .world_to_screen = (float)L->board_size / BOARD_SIDE_NORM
    };
    
    board_view_draw(vp, board, physics, alpha, L, game_phase, game, placement_timer);
}

void renderer_draw_effects(Renderer* r, const GameState* game, double placement_timer) {
    Layout* L = &r->current_layout;
    
    /* Create viewport that maps world coordinates directly to window coordinates */
    Viewport vp = (Viewport){
        .screen_width = L->board_size,
        .screen_height = L->board_size,
        .board_size_px = (float)L->board_size,
        .board_center_px = (Vec2){ (float)L->board_x + (float)L->board_size * 0.5f, (float)L->board_y + (float)L->board_size * 0.5f },
        .world_to_screen = (float)L->board_size / BOARD_SIDE_NORM
    };
    
    effects_draw(vp, game, placement_timer, L);
}

void renderer_capture_frame(Renderer* r, const char* dir, uint64_t frame_num, int game_phase, double placement_timer, float playback_speed, const BoardState* board) {
    if (!r->capture_mode) return;
    
    char path[512];
    snprintf(path, sizeof(path), "%s/frame_%06llu.png", dir, (unsigned long long)frame_num);
    
    Image img = LoadImageFromTexture(r->capture_texture.texture);
    ImageFlipVertical(&img);
    ExportImage(img, path);
    UnloadImage(img);
    
    // Debug phase logging
    if (r->debug_phase) {
        fprintf(stderr, "frame=%llu phase=%d placement_timer=%.3f playback_speed=%.2f striker_x=%.4f striker_y=%.4f\n",
                (unsigned long long)frame_num, game_phase, placement_timer, playback_speed, 
                board->striker.position.x, board->striker.position.y);
    }
}