#ifndef CARROM_BOARD_VIEW_H
#define CARROM_BOARD_VIEW_H

#include "types.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* What the last board_view_draw put on screen (for the flight recorder) */
typedef struct {
    bool striker_valid;
    Vec2 striker_vis;          /* drawn striker position (world units) */
    float figures[4];          /* drawn baseline coordinate of the N,E,S,W players */
    bool aim_drawn;
    Vec2 aim_start, aim_end;   /* drawn aim line, world units */
    uint32_t drawn_from_physics_mask;   /* bit i: piece i was drawn from its physics body */
} BoardViewDebug;
void board_view_get_debug(BoardViewDebug* out);

void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float alpha, const Layout* layout, int game_phase, const GameState* game, double placement_timer);

#ifdef __cplusplus
}
#endif

#endif // CARROM_BOARD_VIEW_H