#include "effects.h"
#include "common/types.h"
#define __USE_MINGW_ANSI_STDIO 1
#include <raylib.h>
#include <stdio.h>
#include <math.h>
#include "common/math.h"

#define MAX_POCKET_FADE_TIME 0.2f  // 200ms fade
#define PLACEMENT_HOLD_TIME 1.0f   // 1 second at 1x playback

typedef struct {
    Vec2 pocket_center;
    float timer;
    bool active;
} PocketFadeEffect;

static PocketFadeEffect pocket_fades[4] = {0};

/* Shared flash alpha computation for syncing figure and striker */
static inline float compute_flash_alpha(double wall_time) {
    // alpha = 0.4 + 0.6 * (0.5 + 0.5 * sin(2π * t)) where t = wall time in seconds, ~1Hz
    float t = (float)wall_time;
    return 0.4f + 0.6f * (0.5f + 0.5f * sinf(t * 2.0f * M_PI));
}

void effects_draw(Viewport vp, const GameState* game, double placement_timer, const Layout* L) {
    double wall_time = GetTime();  // Wall time for animations
    
    // Thinking phase animation: striker sliding + pulse (synced with figure flash)
    if (game->phase == PHASE_THINKING) {
        // Draw thinking striker animation for current turn seat
        Seat seat = game->turn_seat;
        
        // Striker baseline position
        if (game->board.striker.on_baseline && !game->board.striker.pocketed && game->board.striker.owner_seat == seat) {
            
            // Slide back and forth along baseline: one full pass every 1.5s
            float slide_period = 1.5f;
            float slide_phase = fmodf((float)wall_time, slide_period) / slide_period;
            float slide_t = slide_phase <= 0.5f ? slide_phase * 2.0f : (1.0f - slide_phase) * 2.0f;
            
            float min_offset = BASELINE_MIN_OFFSET;
            float max_offset = BASELINE_MAX_OFFSET;
            float slide_offset = min_offset + slide_t * (max_offset - min_offset);
            if (slide_phase > 0.5f) slide_offset = max_offset - slide_t * (max_offset - min_offset);
            
            Vec2 striker_world = {0, 0};
            switch (seat) {
                case SEAT_NORTH:
                    striker_world.x = slide_offset;
                    striker_world.y = BASELINE_Y_NORTH;
                    break;
                case SEAT_SOUTH:
                    striker_world.x = slide_offset;
                    striker_world.y = BASELINE_Y_SOUTH;
                    break;
                case SEAT_EAST:
                    striker_world.x = BASELINE_X_EAST;
                    striker_world.y = slide_offset;
                    break;
                case SEAT_WEST:
                    striker_world.x = BASELINE_X_WEST;
                    striker_world.y = slide_offset;
                    break;
            }
            
            Vec2 screen = math_world_to_screen(vp, striker_world);
            
            // Synced flash pulse: alpha = 0.4 + 0.6 * (0.5 + 0.5 * sin(2π * t))
            float alpha = compute_flash_alpha(wall_time);
            
            Color striker_color = (Color){ 255, 215, 0, (unsigned char)(alpha * 255) };
            Color line_color = (Color){ 255, 255, 255, (unsigned char)(alpha * 100) };
            
            DrawCircle((int)screen.x, (int)screen.y, L->striker_r_px, striker_color);
            DrawCircleLines((int)screen.x, (int)screen.y, L->striker_r_px, line_color);
        }
    }
    
    // Striker placement phase: draw pulsing halo and countdown banner
    if (game->phase == PHASE_PLACEMENT && game->board.striker.on_baseline && !game->board.striker.pocketed && placement_timer > 0.0) {
        Vec2 striker_pos = game->board.striker.position;
        Vec2 screen = math_world_to_screen(vp, striker_pos);
        float striker_r = math_world_to_screen_dist(vp, STRIKER_RADIUS_NORM);
        
        // Pulsing halo: 3 concentric rings fading out, animated with time
        float time = (float)wall_time;
        for (int ring = 0; ring < 3; ring++) {
            float ring_f = (float)ring;
            float ring_phase = time * 3.0f + ring_f * 2.0f;
            float ring_radius = striker_r * 1.5f + ring_f * striker_r * 0.8f + sinf(ring_phase) * striker_r * 0.3f;
            float ring_alpha = 0.6f - ring_f * 0.15f + 0.2f * sinf(ring_phase);
            if (ring_alpha < 0.1f) ring_alpha = 0.1f;
            if (ring_alpha > 0.8f) ring_alpha = 0.8f;
            
            Color halo_color = (Color){ 255, 215, 0, (unsigned char)(ring_alpha * 255) };
            DrawCircleLines((int)screen.x, (int)screen.y, ring_radius, halo_color);
        }
    }
    
    // Aim line and power bar during aiming/placement phase (when not in placement hold)
    if (game->phase == PHASE_AIMING) {
        Vec2 striker_pos = game->board.striker.position;
        Vec2 screen = math_world_to_screen(vp, striker_pos);
        
        float aim_angle = 0.0f;
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
        
        float power = 0.5f;
        float bar_w = math_world_to_screen_dist(vp, 0.2f);
        float bar_h = math_world_to_screen_dist(vp, 0.02f);
        Vec2 bar_pos = { screen.x - bar_w * 0.5f, screen.y - bar_h - math_world_to_screen_dist(vp, 0.03f) };
        DrawRectangle((int)bar_pos.x, (int)bar_pos.y, (int)(bar_w * power), (int)bar_h, (Color){0, 255, 0, 200});
        DrawRectangleLines((int)bar_pos.x, (int)bar_pos.y, (int)bar_w, (int)bar_h, WHITE);
    }
    
    // Pocket fade effects for recently pocketed pieces
    for (int i = 0; i < 4; i++) {
        if (pocket_fades[i].active) {
            pocket_fades[i].timer -= GetFrameTime();
            if (pocket_fades[i].timer <= 0) {
                pocket_fades[i].active = false;
            } else {
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