#ifndef CARROM_RENDERER_H
#define CARROM_RENDERER_H

#include "types.h"
#include <raylib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Renderer - raylib Draw Loop
 * Read-only view of authoritative state
 * --------------------------------------------------------------------------- */

typedef struct Renderer Renderer;

Renderer* renderer_create(int width, int height, const char* title, bool capture_mode);
void renderer_destroy(Renderer* renderer);

void renderer_poll_events(Renderer* renderer);
bool renderer_should_close(Renderer* renderer);
bool renderer_is_paused(Renderer* renderer);

void renderer_begin(Renderer* renderer);
void renderer_end(Renderer* renderer);

void renderer_draw_board(Renderer* renderer, const BoardState* board, const PhysicsWorld* physics, float alpha);
void renderer_draw_hud(Renderer* renderer, const MatchState* match, const GameState* game);
void renderer_draw_effects(Renderer* renderer, const GameState* game);

void renderer_capture_frame(Renderer* renderer, const char* dir, uint64_t frame_num);

#ifdef __cplusplus
}
#endif

#endif // CARROM_RENDERER_H