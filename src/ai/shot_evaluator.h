#ifndef CARROM_SHOT_EVALUATOR_H
#define CARROM_SHOT_EVALUATOR_H

#include "types.h"
#include "shot_candidates.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Shot Evaluator - Scratch Simulation & Scoring
 * Appendix A.6 Steps 5-6
 * --------------------------------------------------------------------------- */

void shot_evaluator_evaluate(ShotCandidate* candidate, const DecisionSnapshot* snap, PCG32* rng);
void shot_evaluator_score_candidates(ShotCandidate* candidates, int count, const DecisionSnapshot* snap, const StrategyProfile* profile);

/* Scoring components */
float score_pocket_value(const ShotResult* result, Team team, const StrategyProfile* profile);
float score_queen_value(const ShotResult* result, const BoardState* board, Team team, const StrategyProfile* profile);
float score_cover_bonus(const ShotResult* result, const BoardState* board, Team team, const StrategyProfile* profile);
float score_striker_risk(const ShotResult* result, const StrategyProfile* profile);
float score_opponent_leave(const ShotResult* result, const BoardState* board, Team team, const StrategyProfile* profile);
float score_positional(const ShotResult* result, const BoardState* board, Team team, const StrategyProfile* profile);

#ifdef __cplusplus
}
#endif

#endif // CARROM_SHOT_EVALUATOR_H