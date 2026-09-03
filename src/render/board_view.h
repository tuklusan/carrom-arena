#ifndef CARROM_BOARD_VIEW_H
#define CARROM_BOARD_VIEW_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float alpha);

#ifdef __cplusplus
}
#endif

#endif // CARROM_BOARD_VIEW_H