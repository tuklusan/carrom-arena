#include "effects.h"
#include "common/types.h"
#include "common/math.h"
#include <raylib.h>
#define __USE_MINGW_ANSI_STDIO 1
#include <math.h>
#include <stdio.h>

#define MAX_POCKET_FADE_TIME 0.2f  // 200ms fade
#define PLACEMENT_HOLD_TIME 1.0f   // 1 second at 1x playback

typedef struct {
    Vec2 pocket_center;
    float timer;
    bool active;
} PocketFadeEffect;

static PocketFadeEffect pocket_fades[4] = {0};

void effects_draw(Viewport vp, const GameState* game, double placement_timer) {
    // Striker placement phase: draw pulsing halo and countdown banner
    if (game->phase == PHASE_PLACEMENT && game->board.striker.on_baseline && !game->board.striker.pocketed && placement_timer > 0.0) {
        Vec2 striker_pos = game->board.striker.position;
        Vec2 screen = math_world_to_screen(vp, striker_pos);
        float striker_r = math_world_to_screen_dist(vp, STRIKER_RADIUS_NORM);
        
        // Pulsing halo: 3 concentric rings fading out, animated with time
        float time = (float)GetTime();
        for (int ring = 0; ring < 3; ring++) {
            float ring_f = (float)ring;
            float ring_phase = time * 3.0f + ring_f * 2.0f;  // Staggered phases
            float ring_radius = striker_r * 1.5f + ring_f * striker_r * 0.8f + sinf(ring_phase) * striker_r * 0.3f;
            float ring_alpha = 0.6f - ring_f * 0.15f + 0.2f * sinf(ring_phase);
            if (ring_alpha < 0.1f) ring_alpha = 0.1f;
            if (ring_alpha > 0.8f) ring_alpha = 0.8f;
            
            Color halo_color = (Color){ 255, 215, 0, (unsigned char)(ring_alpha * 255) };  // Gold
            DrawCircleLines((int)screen.x, (int)screen.y, ring_radius, halo_color);
        }
        
        // Countdown HUD banner at top of game surface
        float gs_left = vp.board_center_px.x - vp.board_size_px * 0.5f;
        float gs_top = vp.board_center_px.y - vp.board_size_px * 0.5f;
        float scale = vp.board_size_px / 600.0f;
        if (scale < 0.6f) scale = 0.6f;
        if (scale > 1.5f) scale = 1.5f;
        int banner_font = (int)(20.0f * scale);
        int banner_height = (int)(40.0f * scale);
        
        // Background banner
        int banner_x = (int)(gs_left);
        int banner_y = (int)(gs_top - (float)banner_height - 10.0f * scale);
        int banner_w = (int)vp.board_size_px;
        DrawRectangle(banner_x, banner_y, banner_w, banner_height, (Color){ 0, 0, 0, 200 });
        DrawRectangleLines(banner_x, banner_y, banner_w, banner_height, (Color){ 255, 215, 0, 255 });
        
        // Countdown text
        char countdown_text[128];
        snprintf(countdown_text, sizeof(countdown_text), 
                 "Striker placed at (%.2f, %.2f) - striking in %.1fs", 
                 striker_pos.x, striker_pos.y, placement_timer);
        int text_width = MeasureText(countdown_text, banner_font);
        int text_x = banner_x + (banner_w - text_width) / 2;
        int text_y = banner_y + (banner_height - banner_font) / 2;
        DrawText(countdown_text, text_x, text_y, banner_font, (Color){ 255, 215, 0, 255 });
    }
    
    // Aim line and power bar during aiming/placement phase (when not in placement hold)
    if (game->phase == PHASE_AIMING) {
        Vec2 striker_pos = game->board.striker.position;
        Vec2 screen = math_world_to_screen(vp, striker_pos);
        
        // Draw aim line from striker (use a default aim angle for now)
        // In a real implementation, this would come from the current aim input
        float aim_angle = 0.0f;  // Default aim toward center
        if (game->turn_seat == SEAT_NORTH) aim_angle = -M_PI / 2.0f;
        else if (game->turn_seat == SEAT_SOUTH) aim_angle = M_PI / 2.0f;
        else if (game->turn_seat == SEAT_EAST) aim_angle = M_PI;
        else aim_angle = 0.0f;
        
        float line_len = math_world_to_screen_dist(vp, 0.5f);
        Vec2 end = {
            screen.x + cosf(aim_angle) * line_len,
            screen.y + sinf(aim_angle) * line_len
        };
        DrawLine((int)screen.x, (int)screen.y, (int)end.x, (int)end.y, (Color){255, 255, 0, 150});
        
        // Power bar (placeholder - would be controlled by input)
        float power = 0.5f;
        float bar_w = math_world_to_screen_dist(vp, 0.2f);
        float bar_h = math_world_to_screen_dist(vp, 0.02f);
        Vec2 bar_pos = { screen.x - bar_w * 0.5f, screen.y - bar_h - math_world_to_screen_dist(vp, 0.03f) };
        DrawRectangle((int)bar_pos.x, (int)bar_pos.y, (int)(bar_w * power), (int)bar_h, (Color){0, 255, 0, 200});
        DrawRectangleLines((int)bar_pos.x, (int)bar_pos.y, (int)bar_w, (int)bar_h, WHITE);
    }
    
    // Pocket fade effects for recently pocketed pieces
    // Update fade timers
    for (int i = 0; i < 4; i++) {
        if (pocket_fades[i].active) {
            pocket_fades[i].timer -= GetFrameTime();
            if (pocket_fades[i].timer <= 0) {
                pocket_fades[i].active = false;
            } else {
                // Draw fade circle at pocket
                float alpha = pocket_fades[i].timer / MAX_POCKET_FADE_TIME;
                float r = math_world_to_screen_dist(vp, POCKET_RADIUS_NORM * 1.5f);
                Vec2 p = math_world_to_screen(vp, POCKET_CENTERS[i]);
                DrawCircle((int)p.x, (int)p.y, r, (Color){255, 255, 0, (unsigned char)(alpha * 100)});
            }
        }
    }
}

void effects_trigger_pocket_fade(int pocket_index) {
    if (pocket_index >= 0 && pocket_index < 4) {
        pocket_fades[pocket_index].pocket_center = POCKET_CENTERS[pocket_index];
        pocket_fades[pocket_index].timer = MAX_POCKET_FADE_TIME;
        pocket_fades[pocket_index].active = true;
    }
}