#include <math.h>
#include "effects.h"
#include "common/types.h"
#define __USE_MINGW_ANSI_STDIO 1
#include <raylib.h>
#include <stdio.h>
#include "common/vecmath.h"

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
    
    (void)L;
    (void)wall_time;

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
