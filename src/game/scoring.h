#ifndef CARROM_SCORING_H
#define CARROM_SCORING_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Scoring Rules
 * --------------------------------------------------------------------------- */

// Calculate score delta for a shot
void scoring_calculate(const ShotFacts* facts, const GameState* game, TeamScores* delta);

// Queen cover logic
bool scoring_is_queen_covered(const ShotFacts* facts, Team active_team);
int scoring_queen_points(bool covered);

// Due piece handling
void scoring_apply_dues(BoardState* board, Team team, int count);

#ifdef __cplusplus
}
#endif

#endif // CARROM_SCORING_H