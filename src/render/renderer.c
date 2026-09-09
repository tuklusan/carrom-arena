#include "renderer.h"
#include "board_view.h"
#include "hud.h"
#include "effects.h"
#include "common/math.h"
#include "common/types.h"
#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Fixed layout constants for 1000x600 window */
#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 600

#define TITLE_BAND_HEIGHT 30
#define TITLE_FONT_SIZE 22
#define TITLE_Y 5

#define MAIN_BODY_TOP 30
#define MAIN_BODY_BOTTOM 470
#define MAIN_BODY_HEIGHT 440

#define HUD_SIDEBAR_LEFT 10
#define HUD_SIDEBAR_RIGHT 200
#define HUD_SIDEBAR_WIDTH 190

#define BOARD_REGION_LEFT 210
#define BOARD_REGION_RIGHT 810
#define BOARD_REGION_WIDTH 600

#define RIGHT_SIDEBAR_LEFT 820
#define RIGHT_SIDEBAR_RIGHT 990

#define BOARD_SURFACE_SIZE 360
#define BOARD_SURFACE_LEFT 330
#define BOARD_SURFACE_TOP 70
#define BOARD_SURFACE_RIGHT 690
#define BOARD_SURFACE_BOTTOM 430

#define BOARD_CENTER_X 510
#define BOARD_CENTER_Y 250

#define FOOTER_BAND_TOP 470
#define FOOTER_BAND_HEIGHT 130
#define FOOTER_RULE_Y 475
#define FOOTER_LINK_Y 495
#define FOOTER_LINK_FONT 16
#define FOOTER_COPYRIGHT_Y 525
#define FOOTER_COPYRIGHT_FONT 14

#define HUD_START_Y 40
#define HUD_LINE_HEIGHT 18
#define HUD_FONT_SIZE 14

static const char* TITLE_TEXT = "SANYALnet Labs Carrom Arena";
static const char* COPYRIGHT_TEXT = "\xC2\xA9 Supratim Sanyal";  // UTF-8 ©
static const char* BLOG_LINK = "https://blog.sanyalnet.com/carrom";

struct Renderer {
    int width;
    int height;
    bool capture_mode;
    bool paused;
    float playback_speed;
    Viewport viewport;
    Camera2D camera;
    RenderTexture2D capture_texture;
    char capture_dir[256];
    
    /* Precomputed text widths */
    int title_width;
    int blog_link_width;
    int copyright_width;
};

/* Draw HUD sidebar in window coordinates (called before BeginMode2D) */
static void draw_hud_sidebar(const MatchState* match, const GameState* game, float playback_speed) {
    float x = HUD_SIDEBAR_LEFT;
    float y = HUD_START_Y;
    int font = HUD_FONT_SIZE;
    float lh = HUD_LINE_HEIGHT;
    
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
        "IDLE", "PLACEMENT", "AIMING", "SHOT", "SETTLING", "RESOLVING",
        "BOARD_OVER", "GAME_OVER", "MATCH_OVER"
    };
    DrawText(TextFormat("Phase: %s", phase_names[game->phase]), (int)x, (int)y, font, GREEN);
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

static void draw_title_bar(Renderer* r) {
    int title_x = (WINDOW_WIDTH - r->title_width) / 2;
    DrawText(TITLE_TEXT, title_x, TITLE_Y, TITLE_FONT_SIZE, WHITE);
}

static void draw_footer_band(Renderer* r) {
    DrawLineEx((Vector2){ 10, FOOTER_RULE_Y }, (Vector2){ 990, FOOTER_RULE_Y }, 1, LIGHTGRAY);
    
    int link_x = (WINDOW_WIDTH - r->blog_link_width) / 2;
    DrawText(BLOG_LINK, link_x, FOOTER_LINK_Y, FOOTER_LINK_FONT, LIGHTGRAY);
    
    int copyright_x = (WINDOW_WIDTH - r->copyright_width) / 2;
    DrawText(COPYRIGHT_TEXT, copyright_x, FOOTER_COPYRIGHT_Y, FOOTER_COPYRIGHT_FONT, (Color){ 180, 180, 180, 255 });
}

Renderer* renderer_create(int width, int height, const char* title, bool capture_mode, bool hidden_window) {
    (void)width; (void)height; (void)title;
    
    Renderer* r = calloc(1, sizeof(Renderer));
    if (!r) return NULL;
    
    r->capture_mode = capture_mode;
    r->paused = false;
    r->playback_speed = 0.5f;
    r->width = WINDOW_WIDTH;
    r->height = WINDOW_HEIGHT;
    
    r->title_width = MeasureText(TITLE_TEXT, TITLE_FONT_SIZE);
    r->blog_link_width = MeasureText(BLOG_LINK, FOOTER_LINK_FONT);
    r->copyright_width = MeasureText(COPYRIGHT_TEXT, FOOTER_COPYRIGHT_FONT);
    
    /* Viewport configured so math_world_to_screen returns WORLD coords:
     * board surface pixels (0..360, 0..360) with origin at top-left of board surface.
     * Normalized coords (-0.5..0.5, -0.5..0.5) -> World (0..360, 0..360).
     * math_world_to_screen: screen = center + world * scale (y inverted).
     * We want: world_x = (norm_x + 0.5) * 360, world_y = (0.5 - norm_y) * 360.
     * So: center_x = 180, center_y = 180, scale = 360.
     */
    r->viewport = math_viewport_create(BOARD_SURFACE_SIZE, BOARD_SURFACE_SIZE);
    r->viewport.screen_width = BOARD_SURFACE_SIZE;
    r->viewport.screen_height = BOARD_SURFACE_SIZE;
    r->viewport.board_size_px = BOARD_SURFACE_SIZE;
    r->viewport.board_center_px = (Vec2){ BOARD_SURFACE_SIZE * 0.5f, BOARD_SURFACE_SIZE * 0.5f };
    r->viewport.world_to_screen = (float)BOARD_SURFACE_SIZE / BOARD_SIDE_NORM;
    
    /* Camera maps world (board surface pixels, origin at top-left) to window coords.
     * World (0, 0) -> window (BOARD_SURFACE_LEFT, BOARD_SURFACE_TOP) = (330, 70).
     */
    r->camera = (Camera2D){ 0 };
    r->camera.offset = (Vector2){ (float)BOARD_SURFACE_LEFT, (float)BOARD_SURFACE_TOP };
    r->camera.target = (Vector2){ 0, 0 };
    r->camera.rotation = 0.0f;
    r->camera.zoom = 1.0f;
    
    unsigned int flags = FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT;
    if (hidden_window) {
        flags |= FLAG_WINDOW_HIDDEN;
    }
    SetConfigFlags(flags);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "SANYALnet Labs Carrom Arena");
    SetTargetFPS(30);
    
    if (capture_mode) {
        r->capture_texture = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT);
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

void renderer_set_playback_speed(Renderer* r, float speed) {
    if (speed < 0.05f) speed = 0.05f;
    if (speed > 4.0f) speed = 4.0f;
    r->playback_speed = speed;
}

void renderer_begin(Renderer* r) {
    BeginDrawing();
    ClearBackground((Color){ 30, 30, 40, 255 });
    
    /* Draw title bar, HUD sidebar, footer band in window coordinates (no camera) */
    draw_title_bar(r);
    draw_footer_band(r);
    /* HUD is drawn by renderer_draw_hud_sidebar which is called from app.c between begin/end */
    /* But we need it before BeginMode2D, so we'll draw it here if we have match/game state.
     * However, renderer_begin doesn't have match/game. So we'll draw HUD in renderer_draw_hud
     * but call it before BeginMode2D. Let's restructure: renderer_begin does NOT call BeginMode2D.
     * Instead, we'll have renderer_begin_board() that starts the camera, and renderer_end_board() that ends it.
     * But to minimize API changes, let's draw HUD in renderer_draw_hud which is called after renderer_begin.
     * We'll move BeginMode2D to after HUD drawing.
     */
    
    if (r->capture_mode) {
        BeginTextureMode(r->capture_texture);
        ClearBackground((Color){ 30, 30, 40, 255 });
        draw_title_bar(r);
        draw_footer_band(r);
    }
}

void renderer_draw_hud_sidebar(Renderer* r, const MatchState* match, const GameState* game, float playback_speed) {
    draw_hud_sidebar(match, game, playback_speed);
}

void renderer_begin_board(Renderer* r) {
    BeginMode2D(r->camera);
    
    if (r->capture_mode) {
        BeginMode2D(r->camera);
    }
}

void renderer_end_board(Renderer* r) {
    EndMode2D();
    
    if (r->capture_mode) {
        EndMode2D();
    }
}

void renderer_end(Renderer* r) {
    EndDrawing();
}

void renderer_draw_board(Renderer* r, const BoardState* board, const PhysicsWorld* physics, float alpha) {
    board_view_draw(r->viewport, board, physics, alpha);
}

void renderer_draw_hud(Renderer* r, const MatchState* match, const GameState* game, float playback_speed) {
    /* HUD now drawn in window coords, but this is called after renderer_begin which doesn't
     * start the camera yet. We'll draw it here in window coords. */
    draw_hud_sidebar(match, game, playback_speed);
}

void renderer_draw_effects(Renderer* r, const GameState* game, double placement_timer) {
    effects_draw(r->viewport, game, placement_timer);
}

void renderer_capture_frame(Renderer* r, const char* dir, uint64_t frame_num) {
    if (!r->capture_mode) return;
    
    char path[512];
    snprintf(path, sizeof(path), "%s/frame_%06llu.png", dir, (unsigned long long)frame_num);
    
    Image img = LoadImageFromTexture(r->capture_texture.texture);
    ImageFlipVertical(&img);
    ExportImage(img, path);
    UnloadImage(img);
}