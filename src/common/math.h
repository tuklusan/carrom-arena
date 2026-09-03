#ifndef CARROM_MATH_H
#define CARROM_MATH_H

#include "types.h"
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Math Utilities
 * --------------------------------------------------------------------------- */
float math_clamp(float v, float min, float max);
float math_lerp(float a, float b, float t);
float math_wrap_angle(float angle);
float math_angle_diff(float a, float b);

/* Float absolute value - wrapper for fabsf */
static inline float math_fabsf(float x) {
    return x < 0.0f ? -x : x;
}

/* Float square root - wrapper for sqrtf (implemented in math.c) */
float math_sqrtf(float x);
Vec2 math_vec2_from_angle(float angle);
float math_vec2_to_angle(Vec2 v);
Vec2 math_vec2_perp(Vec2 v);
float math_vec2_cross(Vec2 a, Vec2 b);
Vec2 math_vec2_reflect(Vec2 v, Vec2 n);

/* Vector2 linear interpolation */
static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, float t) {
    return (Vec2){ a.x + t * (b.x - a.x), a.y + t * (b.y - a.y) };
}

bool math_point_in_circle(Vec2 p, Vec2 center, float radius);
bool math_circles_overlap(Vec2 c1, float r1, Vec2 c2, float r2);
Vec2 math_closest_point_on_segment(Vec2 p, Vec2 a, Vec2 b);
float math_distance_point_segment(Vec2 p, Vec2 a, Vec2 b);

/* -----------------------------------------------------------------------------
 * Screen <-> World Conversion (render only)
 * --------------------------------------------------------------------------- */
Viewport math_viewport_create(int width, int height);
Vec2 math_world_to_screen(Viewport vp, Vec2 world);
Vec2 math_screen_to_world(Viewport vp, Vec2 screen);
float math_world_to_screen_dist(Viewport vp, float world_dist);

#ifdef __cplusplus
}
#endif

#endif // CARROM_MATH_H