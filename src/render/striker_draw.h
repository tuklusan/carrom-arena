#ifndef CARROM_STRIKER_DRAW_H
#define CARROM_STRIKER_DRAW_H

/* The striker's on-screen look: a polished metal disc. Raylib-dependent, so it lives apart from piece_draw.h
 * (shared with the headless tests, which do not link raylib). Included by board_view.c and effects.c, both
 * already part of the render/graphics target. */
#define __USE_MINGW_ANSI_STDIO 1
#include <raylib.h>

static inline float striker_draw_maxf(float a, float b) { return (a > b) ? a : b; }

/* A radial gradient (raylib's DrawCircleGradient) from a bright warm highlight at the centre to a darker antique-gold edge,
 * a thin dark rim for definition, and a small offset specular highlight for shine. `alpha` (0..255) fades the whole disc
 * (used while it glides in after a foul, or is handed to the next player). */
static inline void draw_striker_polished(Vector2 center, float r, unsigned char alpha) {
    Color inner = { 255, 226, 150, alpha };
    Color outer = { 150, 104, 32, alpha };
    Color rim   = { 60, 40, 14, alpha };
    Color shine = { 255, 250, 232, (unsigned char)((int)alpha * 200 / 255) };
    DrawCircleGradient((int)center.x, (int)center.y, r, inner, outer);
    float rim_w = striker_draw_maxf(1.0f, r * 0.06f);
    if (rim_w > r) rim_w = r * 0.5f;
    DrawRing(center, r - rim_w, r, 0, 360, 24, rim);
    DrawEllipse((int)(center.x - r * 0.32f), (int)(center.y - r * 0.34f), r * 0.30f, r * 0.20f, shine);
}

#endif /* CARROM_STRIKER_DRAW_H */
