#ifndef CARROM_RULES_H
#define CARROM_RULES_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Pure Rules Engine (Article 9.1, Appendix A.7)
 * Zero external dependencies - headless testable
 * --------------------------------------------------------------------------- */

// Rules resolution
RulesOutcome rules_resolve(const MatchState* prior_match, const GameState* prior_game, const ShotFacts* facts);

// Fact extraction (physics -> rules)
void match_extract_facts(const GameState* game, const ShotResult* result, ShotFacts* facts);

// Shot validation
bool match_validate_shot(const GameState* game, const ShotPlan* plan);

// Board initialization
void match_start_board(MatchState* match, GameState* game, RNGContext* rng);

// Match state queries
bool match_is_over(const MatchState* match);
bool game_is_over(const GameState* game);
bool board_is_over(const GameState* game);

#ifdef __cplusplus
}
#endif

#endif // CARROM_RULES_H