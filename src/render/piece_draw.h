#ifndef CARROM_PIECE_DRAW_H
#define CARROM_PIECE_DRAW_H

#include "common/types.h"
#include "common/vecmath.h"

/* Decide where (and whether) a live piece is drawn from physics. Pieces the game state
 * says are off the board, or that physics reports pocketed, are never drawn from physics
 * (their body is gone; interpolating would sweep them toward the origin). */
static inline bool board_view_piece_draw_pos(bool on_board, bool physics_pocketed, bool use_physics,
                                             Vec2 board_pos, Vec2 prev_pos, Vec2 curr_pos,
                                             float alpha, Vec2* out) {
    if (!on_board || physics_pocketed) return false;
    *out = use_physics ? vec2_lerp(prev_pos, curr_pos, alpha) : board_pos;
    return true;
}

/* Length of the aim-preview line, in board widths: strictly proportional to the launch power (0..1) */
#define AIM_LINE_FULL_POWER_LEN 0.55f
static inline float aim_line_length(float power) {
    if (power < 0.0f) power = 0.0f;
    if (power > 1.0f) power = 1.0f;
    return power * AIM_LINE_FULL_POWER_LEN;
}

/* Where the nth coin (0-based) pocketed in pocket `pocket_index` is stashed: a 3x3 lineup in the outside corner beside
 * the pocket, coins touching but never overlapping, growing away from the board. */
static inline Vec2 pocket_stash_position(int pocket_index, int nth) {
    const float first = 0.5f + PIECE_RADIUS_NORM + 0.012f;
    const float step = 2.0f * PIECE_RADIUS_NORM + 0.004f;
    float sx = (POCKET_CENTERS[pocket_index].x < 0.0f) ? -1.0f : 1.0f;
    float sy = (POCKET_CENTERS[pocket_index].y > 0.0f) ? 1.0f : -1.0f;
    return (Vec2){ sx * (first + (float)(nth % 3) * step), sy * (first + (float)((nth / 3) % 3) * step) };
}

#endif /* CARROM_PIECE_DRAW_H */
