#ifndef CARROM_RENDERER_H
#define CARROM_RENDERER_H

#include "types.h"
#include <raylib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Renderer - raylib Draw Loop
 * Read-only view of authoritative state
 * --------------------------------------------------------------------------- */

/* Dynamic layout computed each frame from actual window size */
typedef struct {
    int sw, sh;                    // Screen width/height
    int title_band_h;              // Top title band height
    int footer_band_h;             // Bottom footer band height
    int body_h;                    // Body height (between title and footer)
    int hud_w;                     // Left HUD sidebar width
    int right_sidebar_w;           // Right sidebar width
    int board_region_x;            // Board region left edge
    int board_region_w;            // Board region width
    int board_size;                // Board surface size (square)
    int board_x, board_y;          // Board top-left position
    int font_size_title;           // Title font size
    int font_size_footer_link;     // Footer link font size
    int font_size_footer_copyright;// Footer copyright font size
    int font_size_hud;             // HUD font size
    int hud_line_height;           // HUD line height
    int hud_start_y;               // HUD start Y
    float figure_scale;            // Scale factor for human figures
    float figure_halo_base_r;      // Base halo radius for figures
    float striker_r_px;            // Striker radius in pixels
    float piece_r_px;              // Piece radius in pixels
    int figure_band_h;             // Height of each figure band (N and S)
    int n_figure_center_y;         // Screen Y for N figure center
    int s_figure_center_y;         // Screen Y for S figure center
    int placement_banner_y;        // Screen Y for placement banner (in N band)
} Layout;

/* Compute layout from actual screen dimensions */
void layout_compute(int sw, int sh, Layout* out);

typedef struct Renderer Renderer;

Renderer* renderer_create(int width, int height, const char* title, bool capture_mode, bool hidden_window);
void renderer_destroy(Renderer* renderer);

void renderer_poll_events(Renderer* renderer);
bool renderer_should_close(Renderer* renderer);
bool renderer_is_paused(Renderer* renderer);
float renderer_get_playback_speed(const Renderer* renderer);
void renderer_set_playback_speed(Renderer* renderer, float speed);

void renderer_begin(Renderer* renderer);
void renderer_draw_hud_sidebar(Renderer* renderer, const MatchState* match, const GameState* game, float playback_speed);
void renderer_begin_board(Renderer* renderer);
void renderer_end_board(Renderer* renderer);
void renderer_end(Renderer* renderer);

void renderer_draw_board(Renderer* renderer, const BoardState* board, const PhysicsWorld* physics, float alpha, int game_phase, const GameState* game);
void renderer_draw_effects(Renderer* renderer, const GameState* game, double placement_timer);
void renderer_draw_placement_banner(Renderer* renderer, const GameState* game, double placement_timer);

void renderer_capture_frame(Renderer* renderer, const char* dir, uint64_t frame_num);

#ifdef __cplusplus
}
#endif

#endif // CARROM_RENDERER_H