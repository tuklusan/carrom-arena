#ifndef CARROM_BOARD_VIEW_H
#define CARROM_BOARD_VIEW_H

#include "types.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float alpha, const Layout* layout, int game_phase, const GameState* game, double placement_timer);

#ifdef __cplusplus
}
#endif

#endif // CARROM_BOARD_VIEW_H