/*
 * Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs.
 * Licensed under the SANYALnet Labs Non-Commercial License; see LICENSE.
 */

#ifndef CARROM_EFFECTS_H
#define CARROM_EFFECTS_H

#include "types.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

void effects_draw(Viewport vp, const GameState* game, double placement_timer, const Layout* layout);
void effects_trigger_pocket_fade(int pocket_index);
void effects_trigger_pocket_fade_long(int pocket_index, float seconds);

/* Piece (id 0..MAX_PIECES-1) or striker (id EFFECTS_STRIKER_ID) falling into a pocket: it keeps its
 * speed into the hole, then sinks. Timed in SIMULATION seconds so it follows the game speed. */
#define EFFECTS_STRIKER_ID MAX_PIECES
void effects_trigger_pocket_fall(int id, PieceColor color, Vec2 from, Vec2 vel, int pocket_index);
void effects_update(float sim_dt);
bool effects_piece_falling(int id);
/* A coin (or the queen) the rules put back on the board: it slides from the pocket it fell into to its place (no teleporting).
 * Timed in wall seconds; the coin is not drawn on the board by board_view while it slides. */
void effects_trigger_return(int id, PieceColor color, int from_pocket, Vec2 to);
bool effects_piece_returning(int id);
/* Slide a coin from `from` to `to` along a curve (bend: sideways swing as a fraction of the distance), after `delay` seconds,
 * taking `duration` seconds. Used to arrange a new board. Until it has arrived the coin is drawn by effects.c, not board_view. */
void effects_trigger_slide(int id, PieceColor color, Vec2 from, Vec2 to, float delay, float duration, float bend);
#define EFFECTS_RETURN_SLIDE_TIME 0.9f   /* seconds a returned coin takes to slide in (see effects_trigger_return) */
/* The striker, pocketed, slides back to the player who has the next turn along `pts` (a route around the coins, see
 * game/striker_path.h) after `delay` seconds, at about `speed` board widths a second. Returns how long the slide takes. While it
 * runs board_view does not draw the striker; when it arrives effects_take_striker_slide_done gives its end point once. */
float effects_trigger_striker_slide(const Vec2* pts, int n, float delay, float speed);
bool effects_striker_recovering(void);
bool effects_take_striker_slide_done(Vec2* end_pos);
void effects_reset(void);
unsigned int effects_falling_mask(void);   /* bit i set while piece i (bit MAX_PIECES = striker) is falling in */

#ifdef __cplusplus
}
#endif

#endif // CARROM_EFFECTS_H