#include <math.h>
#include "types.h"
#include "vecmath.h"
#define __USE_MINGW_ANSI_STDIO 1
#ifdef __MINGW32__
extern float cosf(float);
extern float sinf(float);
extern float atan2f(float, float);
extern float sqrtf(float);
extern float fminf(float, float);
#endif

/* Define POCKET_CENTERS */
const Vec2 POCKET_CENTERS[4] = {
    { -0.5f + POCKET_RADIUS_NORM,  0.5f - POCKET_RADIUS_NORM },
    {  0.5f - POCKET_RADIUS_NORM,  0.5f - POCKET_RADIUS_NORM },
    { -0.5f + POCKET_RADIUS_NORM, -0.5f + POCKET_RADIUS_NORM },
    {  0.5f - POCKET_RADIUS_NORM, -0.5f + POCKET_RADIUS_NORM }
};

/* -----------------------------------------------------------------------------
 * Math Utilities Implementation
 * --------------------------------------------------------------------------- */

float math_clamp(float v, float min, float max) {
    return v < min ? min : (v > max ? max : v);
}

float math_lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float math_wrap_angle(float angle) {
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
    return angle;
}

Vec2 math_vec2_from_angle(float angle) {
    return (Vec2){ cosf(angle), sinf(angle) };
}

Vec2 math_world_to_screen(Viewport vp, Vec2 world) {
    return (Vec2){
        vp.board_center_px.x + world.x * vp.world_to_screen,
        vp.board_center_px.y - world.y * vp.world_to_screen
    };
}

float math_world_to_screen_dist(Viewport vp, float world_dist) {
    return world_dist * vp.world_to_screen;
}

float math_sqrtf(float x) {
    return sqrtf(x);
}

/* -----------------------------------------------------------------------------
 * Ray-Board Boundary Intersection
 * --------------------------------------------------------------------------- */
float distance_to_board_boundary(Vec2 pos, float angle) {
    // Board inner boundary (cushion lines) in normalized coordinates
    // Board is [-0.5, 0.5] x [-0.5, 0.5], cushions are at the edges
    // Cushion inner edges are at ±(0.5 - CUSHION_THICKNESS)
    const float cushion_limit = 0.5f - CUSHION_THICKNESS;
    
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    
    float t_min = -1.0f;  // Track smallest positive t
    
    // Check intersection with vertical cushion lines: x = ±cushion_limit
    if (fabsf(cos_a) > 1e-6f) {
        // Right cushion (x = +cushion_limit)
        float t_right = (cushion_limit - pos.x) / cos_a;
        if (t_right > 1e-6f) {
            float y_at_right = pos.y + t_right * sin_a;
            if (y_at_right >= -cushion_limit && y_at_right <= cushion_limit) {
                t_min = t_right;
            }
        }
        
        // Left cushion (x = -cushion_limit)
        float t_left = (-cushion_limit - pos.x) / cos_a;
        if (t_left > 1e-6f) {
            float y_at_left = pos.y + t_left * sin_a;
            if (y_at_left >= -cushion_limit && y_at_left <= cushion_limit) {
                if (t_min < 0.0f || t_left < t_min) {
                    t_min = t_left;
                }
            }
        }
    }
    
    // Check intersection with horizontal cushion lines: y = ±cushion_limit
    if (fabsf(sin_a) > 1e-6f) {
        // Top cushion (y = +cushion_limit) - note: in world coords, north is +y
        float t_top = (cushion_limit - pos.y) / sin_a;
        if (t_top > 1e-6f) {
            float x_at_top = pos.x + t_top * cos_a;
            if (x_at_top >= -cushion_limit && x_at_top <= cushion_limit) {
                if (t_min < 0.0f || t_top < t_min) {
                    t_min = t_top;
                }
            }
        }
        
        // Bottom cushion (y = -cushion_limit)
        float t_bottom = (-cushion_limit - pos.y) / sin_a;
        if (t_bottom > 1e-6f) {
            float x_at_bottom = pos.x + t_bottom * cos_a;
            if (x_at_bottom >= -cushion_limit && x_at_bottom <= cushion_limit) {
                if (t_min < 0.0f || t_bottom < t_min) {
                    t_min = t_bottom;
                }
            }
        }
    }
    
    return t_min;
}
