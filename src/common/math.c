/*
 * Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs.
 * Licensed under the SANYALnet Labs Non-Commercial License; see LICENSE.
 */

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

