/*
 * Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs.
 * Licensed under the SANYALnet Labs Non-Commercial License; see LICENSE.
 */

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "striker_path.h"

#define SP_MAX_OBSTACLES (MAX_PIECES + 4)
#define SP_ANGLES 12
#define SP_MAX_NODES (2 + SP_MAX_OBSTACLES * SP_ANGLES)
#define SP_INF 1e30f

typedef struct { Vec2 c; float r; } Obstacle;

static float dist2(Vec2 a, Vec2 b) { float dx = a.x - b.x, dy = a.y - b.y; return dx * dx + dy * dy; }

/* distance from point p to the segment a-b */
static float seg_point_dist(Vec2 a, Vec2 b, Vec2 p) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float len2 = dx * dx + dy * dy;
    float t = len2 > 1e-12f ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    Vec2 q = { a.x + dx * t, a.y + dy * t };
    return sqrtf(dist2(q, p));
}

static bool seg_clear(Vec2 a, Vec2 b, const Obstacle* ob, int n) {
    for (int i = 0; i < n; i++) {
        if (seg_point_dist(a, b, ob[i].c) < ob[i].r - 1e-4f) return false;
    }
    return true;
}

float striker_path_length(const Vec2* pts, int n) {
    float len = 0.0f;
    for (int i = 1; i < n; i++) len += sqrtf(dist2(pts[i - 1], pts[i]));
    return len;
}

int striker_path_plan(Vec2 from, Vec2 to, const Vec2* coins, int coin_count, Vec2* out, int max_out) {
    Obstacle ob[SP_MAX_OBSTACLES];
    int nob = 0;
    for (int i = 0; i < coin_count && nob < MAX_PIECES; i++) ob[nob++] = (Obstacle){ coins[i], STRIKER_PATH_CLEAR };
    for (int p = 0; p < 4; p++) {                 /* the other pockets: keep clear of the hole (the one it fell into is where it starts) */
        if (dist2(POCKET_CENTERS[p], from) < 0.02f * 0.02f) continue;
        ob[nob++] = (Obstacle){ POCKET_CENTERS[p], POCKET_RADIUS_NORM + STRIKER_RADIUS_NORM };
    }

    static Vec2 node[SP_MAX_NODES];
    int n = 0;
    node[n++] = from;
    node[n++] = to;
    const float limit = 0.5f - CUSHION_THICKNESS - STRIKER_RADIUS_NORM;       /* the striker's centre stays inside this square */
    for (int i = 0; i < nob; i++) {
        for (int k = 0; k < SP_ANGLES; k++) {
            float a = (float)k * (2.0f * (float)M_PI / (float)SP_ANGLES);
            Vec2 p = { ob[i].c.x + cosf(a) * ob[i].r * 1.08f, ob[i].c.y + sinf(a) * ob[i].r * 1.08f };
            if (fabsf(p.x) > limit || fabsf(p.y) > limit) continue;
            bool inside = false;
            for (int j = 0; j < nob && !inside; j++) if (dist2(p, ob[j].c) < ob[j].r * ob[j].r - 1e-6f) inside = true;
            if (!inside) node[n++] = p;
        }
    }

    static float best[SP_MAX_NODES];
    static int prev[SP_MAX_NODES];
    static bool done[SP_MAX_NODES];
    for (int i = 0; i < n; i++) { best[i] = SP_INF; prev[i] = -1; done[i] = false; }
    best[0] = 0.0f;
    for (;;) {
        int u = -1;
        for (int i = 0; i < n; i++) if (!done[i] && best[i] < SP_INF && (u < 0 || best[i] < best[u])) u = i;
        if (u < 0 || u == 1) break;
        done[u] = true;
        for (int v = 0; v < n; v++) {
            if (done[v]) continue;
            float d = best[u] + sqrtf(dist2(node[u], node[v]));
            if (d < best[v] && seg_clear(node[u], node[v], ob, nob)) { best[v] = d; prev[v] = u; }
        }
    }

    int count = 0;
    if (best[1] < SP_INF) {
        int chain[SP_MAX_NODES], len = 0;
        for (int v = 1; v >= 0 && len < SP_MAX_NODES; v = prev[v]) { chain[len++] = v; if (v == 0) break; }
        if (len >= 2 && len <= max_out) {
            for (int i = 0; i < len; i++) out[count++] = node[chain[len - 1 - i]];
            return count;
        }
    }
    out[0] = from;
    out[1] = to;
    return 2;
}
