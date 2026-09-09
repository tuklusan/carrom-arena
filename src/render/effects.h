#ifndef CARROM_EFFECTS_H
#define CARROM_EFFECTS_H

#include "types.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

void effects_draw(Viewport vp, const GameState* game, double placement_timer, const Layout* layout);
void effects_trigger_pocket_fade(int pocket_index);

#ifdef __cplusplus
}
#endif

#endif // CARROM_EFFECTS_H