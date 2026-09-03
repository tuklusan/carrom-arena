#include "renderer.h"
#include "board_view.h"
#include "hud.h"
#include "effects.h"
#include "common/math.h"
#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Layout constants for the new fixed 600x600 gameplay surface */
#define GAME_SURFACE_SIZE 600
#define TITLE_FONT_SIZE 24
#define COPYRIGHT_FONT_SIZE 16
#define TITLE_PADDING 16
#define COPYRIGHT_PADDING 12

static const char* TITLE_TEXT = "SANYALnet Labs Carrom Arena";
static const char* COPYRIGHT_TEXT = "\xC2\xA9 Supratim Sanyal";  // UTF-8 ©

struct Renderer {
    int width;
    int height;
    bool capture_mode;
    bool paused;
    Viewport viewport;
    Camera2D camera;
    RenderTexture2D capture_texture;
    char capture_dir[256];
    
    /* Layout */
    int game_surface_x;
    int game_surface_y;
    int title_width;
    int copyright_width;
};

/* Helper to measure text width */
static int measure_text_width(const char* text, int font_size) {
    return MeasureText(text, font_size);
}

/* Helper to center window on primary monitor */
static void center_window_on_monitor(int window_width, int window_height) {
    int monitor = GetCurrentMonitor();
    int monitor_width = GetMonitorWidth(monitor);
    int monitor_height = GetMonitorHeight(monitor);
    int pos_x = (monitor_width - window_width) / 2;
    int pos_y = (monitor_height - window_height) / 2;
    SetWindowPosition(pos_x, pos_y);
}

Renderer* renderer_create(int width, int height, const char* title, bool capture_mode) {
    Renderer* r = calloc(1, sizeof(Renderer));
    if (!r) return NULL;
    
    r->capture_mode = capture_mode;
    r->paused = false;
    
    /* Calculate layout dimensions */
    r->title_width = measure_text_width(TITLE_TEXT, TITLE_FONT_SIZE);
    r->copyright_width = measure_text_width(COPYRIGHT_TEXT, COPYRIGHT_FONT_SIZE);
    
    int title_area_height = TITLE_FONT_SIZE + TITLE_PADDING * 2;
    int copyright_area_height = COPYRIGHT_FONT_SIZE + COPYRIGHT_PADDING * 2;
    
    /* Clamp requested size to monitor bounds with margins */
    int monitor = GetCurrentMonitor();
    int monitor_width = GetMonitorWidth(monitor);
    int monitor_height = GetMonitorHeight(monitor);
    
    // Margins: 40px horizontal, 80px vertical (taskbar + titlebar)
    int max_width = monitor_width - 40;
    int max_height = monitor_height - 80;
    
    // Minimum required: 600 game surface + 40px horizontal padding = 640
    // Minimum height: title(24+32) + 600 + copyright(16+24) = ~696
    int min_width = 640;
    int min_height = 696;
    
    // Start with CLI values, fallback to calculated
    int window_width = (width > 0) ? width : GAME_SURFACE_SIZE + 40;
    int window_height = (height > 0) ? height : title_area_height + GAME_SURFACE_SIZE + copyright_area_height + 40;
    
    // Clamp to [min, max]
    if (window_width < min_width) window_width = min_width;
    if (window_width > max_width) window_width = max_width;
    if (window_height < min_height) window_height = min_height;
    if (window_height > max_height) window_height = max_height;
    
    // Ensure text fits
    if (r->title_width + 40 > window_width) window_width = r->title_width + 40;
    if (r->copyright_width + 40 > window_width) window_width = r->copyright_width + 40;
    
    r->width = window_width;
    r->height = window_height;
    
    /* Game surface position within window - centered with margins */
    r->game_surface_x = (r->width - GAME_SURFACE_SIZE) / 2;
    r->game_surface_y = title_area_height;
    
    /* Create viewport for the fixed 600x600 game surface */
    r->viewport = math_viewport_create(GAME_SURFACE_SIZE, GAME_SURFACE_SIZE);
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(r->width, r->height, "SANYALnet Labs Carrom Arena");
    SetTargetFPS(60);
    
    /* Center window on monitor */
    center_window_on_monitor(r->width, r->height);
    
    r->camera = (Camera2D){ 0 };
    r->camera.offset = (Vector2){ (float)GAME_SURFACE_SIZE * 0.5f, (float)GAME_SURFACE_SIZE * 0.5f };
    r->camera.target = (Vector2){ 0, 0 };
    r->camera.rotation = 0.0f;
    r->camera.zoom = 1.0f;
    
    if (capture_mode) {
        r->capture_texture = LoadRenderTexture(r->width, r->height);
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
        // Handled by WindowShouldClose
    }
}

bool renderer_should_close(Renderer* r) {
    return WindowShouldClose();
}

bool renderer_is_paused(Renderer* r) {
    return r->paused;
}

void renderer_begin(Renderer* r) {
    BeginDrawing();
    ClearBackground((Color){ 30, 30, 40, 255 });
    
    /* Draw title above game surface */
    int title_x = r->game_surface_x + (GAME_SURFACE_SIZE - r->title_width) / 2;
    int title_y = TITLE_PADDING + (TITLE_FONT_SIZE / 2);
    DrawText(TITLE_TEXT, title_x, title_y, TITLE_FONT_SIZE, WHITE);
    
    /* Draw copyright below game surface */
    int copyright_x = r->game_surface_x + (GAME_SURFACE_SIZE - r->copyright_width) / 2;
    int copyright_y = r->game_surface_y + GAME_SURFACE_SIZE + COPYRIGHT_PADDING + (COPYRIGHT_FONT_SIZE / 2);
    DrawText(COPYRIGHT_TEXT, copyright_x, copyright_y, COPYRIGHT_FONT_SIZE, (Color){ 180, 180, 180, 255 });
    
    if (r->capture_mode) {
        BeginTextureMode(r->capture_texture);
        ClearBackground((Color){ 30, 30, 40, 255 });
        /* Also draw title/copyright in capture */
        DrawText(TITLE_TEXT, title_x, title_y, TITLE_FONT_SIZE, WHITE);
        DrawText(COPYRIGHT_TEXT, copyright_x, copyright_y, COPYRIGHT_FONT_SIZE, (Color){ 180, 180, 180, 255 });
    }
    
    BeginMode2D(r->camera);
}

void renderer_end(Renderer* r) {
    EndMode2D();
    
    if (r->capture_mode) {
        EndTextureMode();
    }
    
    EndDrawing();
}

void renderer_draw_board(Renderer* r, const BoardState* board, const PhysicsWorld* physics, float alpha) {
    board_view_draw(r->viewport, board, physics, alpha);
}

void renderer_draw_hud(Renderer* r, const MatchState* match, const GameState* game) {
    hud_draw(r->viewport, match, game);
}

void renderer_draw_effects(Renderer* r, const GameState* game) {
    effects_draw(r->viewport, game);
}

void renderer_capture_frame(Renderer* r, const char* dir, uint64_t frame_num) {
    if (!r->capture_mode) return;
    
    char path[512];
    snprintf(path, sizeof(path), "%s/frame_%06llu.png", dir, (unsigned long long)frame_num);
    
    Image img = LoadImageFromTexture(r->capture_texture.texture);
    ImageFlipVertical(&img);  // raylib textures are upside down
    ExportImage(img, path);
    UnloadImage(img);
}