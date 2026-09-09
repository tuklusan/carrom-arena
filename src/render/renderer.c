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
#include <math.h>

#define MAX_BOARD_SIZE 500

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

/* Compute dynamic layout from actual screen dimensions */
void layout_compute(int sw, int sh, Layout* out) {
    out->sw = sw;
    out->sh = sh;
    float fsw = (float)sw;
    float fsh = (float)sh;

    out->title_band_h = (int)(fsh * 0.055f);        // Was 0.05
    out->footer_band_h = (int)(fsh * 0.14f);         // Was 0.22
    out->hud_w = (int)(fsw * 0.19f);
    out->right_sidebar_w = (int)(fsw * 0.17f);
    out->board_region_x = out->hud_w + 10;
    out->board_region_w = sw - out->hud_w - out->right_sidebar_w - 10;

    // Estimate figure_band_h for available_h calc (will refine after board_size)
    int est_figure_band_h = (int)(fsh * 0.08f);  // ~58px at 720
    
    int available_h = sh - out->title_band_h - 2 * est_figure_band_h - out->footer_band_h - 40;
    int available_w = out->board_region_w - 80;
    int candidate_board_size = (int)fminf((float)available_h, (float)available_w);
    out->board_size = candidate_board_size > MAX_BOARD_SIZE ? MAX_BOARD_SIZE : candidate_board_size;
    if (out->board_size < 200) out->board_size = 200;

    // Now refine figure_band_h from actual board_size
    out->figure_band_h = out->board_size / 8;
    
    // N figure band center
    out->n_figure_center_y = out->title_band_h + out->figure_band_h + 10;
    // Board top edge
    out->board_y = out->title_band_h + 2 * out->figure_band_h + 20;
    // Board centered horizontally
    out->board_x = out->board_region_x + (out->board_region_w - out->board_size) / 2;
    // S figure band center (symmetric from bottom)
    out->s_figure_center_y = sh - out->footer_band_h - out->figure_band_h - 10;
    
    // Placement banner in N figure band
    out->placement_banner_y = out->title_band_h + 6;

    out->body_h = sh - out->title_band_h - out->footer_band_h;  // Keep for compat

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
        "IDLE", "THINKING", "PLACEMENT", "AIMING", "SHOT", "SETTLING", "RESOLVING",
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

static void draw_title_bar(Renderer* r, const Layout* L) {
    int title_x = (L->sw - r->title_width) / 2;
    DrawText(TITLE_TEXT, title_x, 5, L->font_size_title, WHITE);
}

static void draw_footer_band(Renderer* r, const Layout* L) {
    int rule_y = L->title_band_h + L->body_h + 5;
    DrawLineEx((Vector2){ 10, (float)rule_y }, (Vector2){ (float)(L->sw - 10), (float)rule_y }, 1, LIGHTGRAY);
    
    int link_x = (L->sw - r->blog_link_width) / 2;
    int link_y = rule_y + 20;
    DrawText(BLOG_LINK, link_x, link_y, L->font_size_footer_link, LIGHTGRAY);
    
    int copyright_x = (L->sw - r->copyright_width) / 2;
    int copyright_y = link_y + L->font_size_footer_link + 10;
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

void renderer_draw_placement_banner(Renderer* r, const GameState* game, double placement_timer, const Layout* L) {
    draw_placement_banner(r, game, placement_timer, L);
}

Renderer* renderer_create(int width, int height, const char* title, bool capture_mode, bool hidden_window) {
    (void)title;
    
    Renderer* r = calloc(1, sizeof(Renderer));
    if (!r) return NULL;
    
    r->capture_mode = capture_mode;
    r->paused = false;
    r->playback_speed = 0.05f;  // R3: 1/10th speed default
    r->width = width;
    r->height = height;
    
    /* Initial viewport - will be updated each frame in renderer_begin_board */
    r->viewport = (Viewport){0};
    
    r->camera = (Camera2D){ 0 };
    r->camera.offset = (Vector2){ 0, 0 };
    r->camera.target = (Vector2){ 0, 0 };
    r->camera.rotation = 0.0f;
    r->camera.zoom = 1.0f;
    
    unsigned int flags = FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT;
    if (hidden_window) {
        flags |= FLAG_WINDOW_HIDDEN;
    }
    SetConfigFlags(flags);
    InitWindow(width, height, "SANYALnet Labs Carrom Arena");
    SetTargetFPS(15);  // R4: 15 FPS
    
    /* Measure text widths AFTER InitWindow so default font is loaded */
    Layout dummy;
    layout_compute(GetScreenWidth(), GetScreenHeight(), &dummy);
    r->title_width = MeasureText(TITLE_TEXT, dummy.font_size_title);
    r->blog_link_width = MeasureText(BLOG_LINK, dummy.font_size_footer_link);
    r->copyright_width = MeasureText(COPYRIGHT_TEXT, dummy.font_size_footer_copyright);
    
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

void renderer_set_playback_speed(Renderer* r, float speed) {
    if (speed < 0.05f) speed = 0.05f;
    if (speed > 4.0f) speed = 4.0f;
    r->playback_speed = speed;
}

void renderer_begin(Renderer* r) {
    BeginDrawing();
    ClearBackground((Color){ 30, 30, 40, 255 });
    
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Layout L;
    layout_compute(sw, sh, &L);
    
    draw_title_bar(r, &L);
    draw_footer_band(r, &L);
    
    if (r->capture_mode) {
        BeginTextureMode(r->capture_texture);
        ClearBackground((Color){ 30, 30, 40, 255 });
        draw_title_bar(r, &L);
        draw_footer_band(r, &L);
    }
}

void renderer_draw_hud_sidebar(Renderer* r, const MatchState* match, const GameState* game, float playback_speed, const Layout* L) {
    draw_hud_sidebar(match, game, playback_speed, L);
}

void renderer_begin_board(Renderer* r) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Layout L;
    layout_compute(sw, sh, &L);
    
    /* Update viewport for board drawing */
    r->viewport.screen_width = L.board_size;
    r->viewport.screen_height = L.board_size;
    r->viewport.board_size_px = (float)L.board_size;
    r->viewport.board_center_px = (Vec2){ (float)L.board_size * 0.5f, (float)L.board_size * 0.5f };
    r->viewport.world_to_screen = (float)L.board_size / BOARD_SIDE_NORM;
    
    /* Camera maps world (board surface pixels, origin at top-left) to window coords */
    r->camera.offset = (Vector2){ (float)L.board_x, (float)L.board_y };
    r->camera.target = (Vector2){ 0, 0 };
    r->camera.rotation = 0.0f;
    r->camera.zoom = 1.0f;
    
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

void renderer_draw_board(Renderer* r, const BoardState* board, const PhysicsWorld* physics, float alpha, const Layout* L) {
    board_view_draw(r->viewport, board, physics, alpha, L);
}

void renderer_draw_effects(Renderer* r, const GameState* game, double placement_timer, const Layout* L) {
    effects_draw(r->viewport, game, placement_timer, L);
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