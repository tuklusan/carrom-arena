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
/* Arena kickoff: a genuine coin toss for which pair breaks (and holds white for) the match's first board.
 * Opt-in - match_state_init() alone leaves break_offset at 0 (north breaks first, the old deterministic
 * behaviour), so nothing changes unless this is called explicitly. */
void match_randomize_first_breaker(MatchState* match, RNGContext* rng);

#ifdef __cplusplus
}
#endif

#endif // CARROM_MATCH_H