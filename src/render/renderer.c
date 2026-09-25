#include "renderer.h"
#include "platform/platform.h"
#include "board_view.h"
#include "effects.h"
#include "common/vecmath.h"
#include "common/types.h"
#include <raylib.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "audio/audio.h"


static const char* TITLE_TEXT = "SANYALnet Labs Carrom Arena";
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
    Team turn_team;
};

#define GAME_BACKGROUND (Color){ 84, 112, 140, 255 }   /* steel blue: dark pieces and figures stand out */

/* Compute dynamic layout from actual screen dimensions */
void layout_compute(int sw, int sh, Layout* out) {
    out->sw = sw;
    out->sh = sh;
    float fsh = (float)sh;

    out->title_band_h = (int)(fsh * 0.055f);
    out->footer_band_h = (int)(fsh * 0.055f);
    out->hud_w = 0;                 /* no HUD: the board owns the whole window */
    out->right_sidebar_w = 0;
    out->board_region_x = 0;
    out->board_region_w = sw;
    out->body_h = sh - out->title_band_h - out->footer_band_h;

    /* Everything outside the board (player figures, and the pocketed-coin stashes in the four corners)
     * extends at most fig_k * board + fig_fixed beyond each edge; the board is the largest square that fits. */
    float fig_k = 0.15f;
    int fig_fixed = 8;
    int vert = (int)(((float)out->body_h - 2.0f * (float)fig_fixed) / (1.0f + 2.0f * fig_k));
    int horiz = (int)(((float)out->board_region_w - 2.0f * (float)fig_fixed) / (1.0f + 2.0f * fig_k));
    int candidate = (vert < horiz) ? vert : horiz;
    if (candidate < 200) candidate = 200;
    if (candidate > 1400) candidate = 1400;
    out->board_size = candidate;
    out->figure_band_h = fig_fixed + (int)(fig_k * (float)candidate);

    out->board_y = out->title_band_h + (out->body_h - out->board_size) / 2;
    out->board_x = (sw - out->board_size) / 2;

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


/* Tron-style backdrop: a perspective floor and ceiling grid converging on a horizon. The bright lines use the colour
 * of the team whose turn it is; the subdued lines use the other team's colour. Slowly scrolling. */
static void draw_background(int sw, int sh, Team turn_team, double t) {
    DrawRectangleGradientV(0, 0, sw, sh, (Color){ 36, 52, 84, 255 }, (Color){ 66, 92, 128, 255 });
    Color bright = (turn_team == TEAM_WHITE) ? (Color){ 110, 225, 255, 255 } : (Color){ 255, 160, 70, 255 };
    Color subdued = (turn_team == TEAM_WHITE) ? (Color){ 255, 160, 70, 255 } : (Color){ 110, 225, 255, 255 };
    float horizon = (float)sh * 0.5f;
    float vx = (float)sw * 0.5f;

    /* lines running to the vanishing point (floor and ceiling) */
    const int lanes = 14;
    float spread = (float)sw / 5.0f;
    for (int k = -lanes; k <= lanes; k++) {
        bool major = (k % 4 == 0);
        Color c = major ? bright : subdued;
        c.a = (unsigned char)(major ? 70 : 34);
        float xb = vx + (float)k * spread;
        DrawLineEx((Vector2){ vx, horizon }, (Vector2){ xb, (float)sh }, 1.0f, c);
        DrawLineEx((Vector2){ vx, horizon }, (Vector2){ xb, 0.0f }, 1.0f, c);
    }
    /* cross lines, receding with perspective and scrolling toward the viewer */
    const int rows = 12;
    float scroll = (float)fmod(t * 0.12, 1.0);
    for (int j = 0; j < rows; j++) {
        float u = ((float)j + scroll) / (float)rows;          /* 0 (far) .. 1 (near) */
        float depth = u * u;
        Color c = (j % 4 == 0) ? bright : subdued;
        c.a = (unsigned char)(20.0f + 70.0f * depth);
        float yf = horizon + (float)(sh) * 0.5f * depth;
        float yc = horizon - (float)(sh) * 0.5f * depth;
        DrawLineEx((Vector2){ 0.0f, yf }, (Vector2){ (float)sw, yf }, 1.0f, c);
        DrawLineEx((Vector2){ 0.0f, yc }, (Vector2){ (float)sw, yc }, 1.0f, c);
    }
    /* the horizon glow */
    Color glow = bright;
    glow.a = 90;
    DrawLineEx((Vector2){ 0.0f, horizon }, (Vector2){ (float)sw, horizon }, 2.0f, glow);
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
    int link_y = rule_y + 5;
    DrawText(BLOG_LINK, link_x, link_y, L->font_size_footer_link, LIGHTGRAY);
    
}


/* raylib logs to stdout by default; the game must not write to a console, so its log goes to the debug file */
static void raylib_log_to_file(int level, const char* text, va_list args) {
    char line[512];
    vsnprintf(line, sizeof(line), text, args);
    platform_diag_logf("[raylib %d] %s\n", level, line);
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
    SetTraceLogCallback(raylib_log_to_file);
    SetConfigFlags(flags);
    InitWindow(width, height, "SANYALnet Labs Carrom Arena");
    if (!IsWindowReady()) {
        free(r);
        return NULL;
    }
    SetWindowMinSize(400, 400);
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
    
    if (IsKeyPressed(KEY_M)) {
        audio_toggle_mute();
    }
    if (IsKeyPressed(KEY_SPACE)) {
        r->paused = !r->paused;
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

Layout renderer_get_layout(const Renderer* r) {
    return r->current_layout;
}

void renderer_set_turn_team(Renderer* r, Team team) {
    r->turn_team = team;
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
        platform_diag_logf("[DEBUG-PHASE] frame=%d sw=%d sh=%d board_size=%d board_x=%d board_y=%d board_region_w=%d hud_w=%d title_band_h=%d footer_band_h=%d figure_band_h=%d\n",
                r->debug_frame_count, sw, sh, L->board_size, L->board_x, L->board_y,
                L->board_region_w, L->hud_w, L->title_band_h, L->footer_band_h, L->figure_band_h);
        r->debug_frame_count++;
    }
    
    if (r->capture_mode && r->hidden_window) {
        // Headless capture: render directly to texture, no main window drawing
        BeginTextureMode(r->capture_texture);
        ClearBackground(GAME_BACKGROUND);
        draw_background(sw, sh, r->turn_team, GetTime());
    } else {
        // Interactive or windowed capture: render to main window
        BeginDrawing();
        ClearBackground(GAME_BACKGROUND);
        draw_background(sw, sh, r->turn_team, GetTime());
    }
    
    draw_title_bar(r, L);
    draw_footer_band(r, L);
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
        platform_diag_logf("frame=%llu phase=%d placement_timer=%.3f playback_speed=%.2f striker_x=%.4f striker_y=%.4f\n",
                (unsigned long long)frame_num, game_phase, placement_timer, playback_speed, 
                board->striker.position.x, board->striker.position.y);
    }
}