#ifndef CARROM_HUD_H
#define CARROM_HUD_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void hud_draw(Viewport vp, const MatchState* match, const GameState* game);

#ifdef __cplusplus
}
#endif

#endif // CARROM_HUD_H