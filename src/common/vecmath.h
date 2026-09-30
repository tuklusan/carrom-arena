/*
 * Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs.
 * Licensed under the SANYALnet Labs Non-Commercial License; see LICENSE.
 */

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

/* Float absolute value - wrapper for fabsf */
static inline float math_fabsf(float x) {
    return x < 0.0f ? -x : x;
}

/* Float square root - wrapper for sqrtf (implemented in math.c) */
float math_sqrtf(float x);
Vec2 math_vec2_from_angle(float angle);

/* Vector2 linear interpolation */
static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, float t) {
    return (Vec2){ a.x + t * (b.x - a.x), a.y + t * (b.y - a.y) };
}


/* -----------------------------------------------------------------------------
 * World -> Screen Conversion (render only)
 * --------------------------------------------------------------------------- */
Vec2 math_world_to_screen(Viewport vp, Vec2 world);
float math_world_to_screen_dist(Viewport vp, float world_dist);

/* -----------------------------------------------------------------------------
 * Ray-Board Boundary Intersection
 * --------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif

#endif // CARROM_MATH_H