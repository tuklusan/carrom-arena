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

#endif /* CARROM_PIECE_DRAW_H */
