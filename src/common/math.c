#include "types.h"
#include "math.h"
#define __USE_MINGW_ANSI_STDIO 1
#include <math.h>
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

float math_angle_diff(float a, float b) {
    float d = math_wrap_angle(a - b);
    return d;
}

Vec2 math_vec2_from_angle(float angle) {
    return (Vec2){ cosf(angle), sinf(angle) };
}

float math_vec2_to_angle(Vec2 v) {
    return atan2f(v.y, v.x);
}

Vec2 math_vec2_perp(Vec2 v) {
    return (Vec2){ -v.y, v.x };
}

float math_vec2_cross(Vec2 a, Vec2 b) {
    return a.x * b.y - a.y * b.x;
}

Vec2 math_vec2_reflect(Vec2 v, Vec2 n) {
    float dot = v.x * n.x + v.y * n.y;
    return (Vec2){ v.x - 2 * dot * n.x, v.y - 2 * dot * n.y };
}

/* -----------------------------------------------------------------------------
 * Geometry Utilities
 * --------------------------------------------------------------------------- */
bool math_point_in_circle(Vec2 p, Vec2 center, float radius) {
    float dx = p.x - center.x;
    float dy = p.y - center.y;
    return dx*dx + dy*dy <= radius*radius;
}

bool math_circles_overlap(Vec2 c1, float r1, Vec2 c2, float r2) {
    float dx = c1.x - c2.x;
    float dy = c1.y - c2.y;
    float r = r1 + r2;
    return dx*dx + dy*dy <= r*r;
}

Vec2 math_closest_point_on_segment(Vec2 p, Vec2 a, Vec2 b) {
    Vec2 ab = { b.x - a.x, b.y - a.y };
    float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / (ab.x * ab.x + ab.y * ab.y);
    t = math_clamp(t, 0.0f, 1.0f);
    return (Vec2){ a.x + ab.x * t, a.y + ab.y * t };
}

float math_distance_point_segment(Vec2 p, Vec2 a, Vec2 b) {
    Vec2 cp = math_closest_point_on_segment(p, a, b);
    float dx = p.x - cp.x;
    float dy = p.y - cp.y;
    return sqrtf(dx*dx + dy*dy);
}

/* -----------------------------------------------------------------------------
 * Screen <-> World Conversion (render only)
 * --------------------------------------------------------------------------- */
Viewport math_viewport_create(int width, int height) {
    Viewport vp;
    vp.screen_width = width;
    vp.screen_height = height;
    float w = (float)width;
    float h = (float)height;
    vp.board_size_px = fminf(w, h) * 0.9f;
    vp.board_center_px = (Vec2){ w * 0.5f, h * 0.5f };
    vp.world_to_screen = vp.board_size_px / BOARD_SIDE_NORM;
    return vp;
}

Vec2 math_world_to_screen(Viewport vp, Vec2 world) {
    return (Vec2){
        vp.board_center_px.x + world.x * vp.world_to_screen,
        vp.board_center_px.y - world.y * vp.world_to_screen
    };
}

Vec2 math_screen_to_world(Viewport vp, Vec2 screen) {
    return (Vec2){
        (screen.x - vp.board_center_px.x) / vp.world_to_screen,
        (vp.board_center_px.y - screen.y) / vp.world_to_screen
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