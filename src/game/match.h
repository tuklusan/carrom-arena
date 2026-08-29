#ifndef CARROM_MATCH_H
#define CARROM_MATCH_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Match State Machine
 * --------------------------------------------------------------------------- */

void match_state_init(MatchState* match);
bool match_is_over(const MatchState* match);
void match_start_board(MatchState* match, GameState* game, RNGContext* rng);
void match_advance_turn(GameState* game);

#ifdef __cplusplus
}
#endif

#endif // CARROM_MATCH_H