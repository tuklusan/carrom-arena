#ifndef CARROM_HUD_H
#define CARROM_HUD_H

#include "types.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

void hud_draw(Viewport vp, const MatchState* match, const GameState* game, float playback_speed, const Layout* layout, int candidates_evaluated);

#ifdef __cplusplus
}
#endif

#endif // CARROM_HUD_H