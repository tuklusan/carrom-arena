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
void effects_reset(void);
unsigned int effects_falling_mask(void);   /* bit i set while piece i (bit MAX_PIECES = striker) is falling in */

#ifdef __cplusplus
}
#endif

#endif // CARROM_EFFECTS_H