#ifndef CARROM_SHOT_CANDIDATES_H
#define CARROM_SHOT_CANDIDATES_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Shot Candidate Generation
 * Appendix A.6 Steps 1-4
 * --------------------------------------------------------------------------- */

typedef struct {
    ShotPlan plan;
    float score;
    TacticType tactic;
    // Scratch simulation result
    ShotResult sim_result;
    bool sim_valid;
} ShotCandidate;

/* Generate all candidates for a decision */
int shot_candidates_generate(const DecisionSnapshot* snap, ShotCandidate* out_candidates, int max_candidates, PCG32* rng);

/* Generate legal striker placements for a seat */
int shot_candidates_placements(Seat seat, Vec2* out_placements, int max_placements);

/* Generate tactical candidates for a placement */
int shot_candidates_tactical(const DecisionSnapshot* snap, Vec2 placement, ShotCandidate* out_candidates, int max_candidates, PCG32* rng);

/* Generate aim/power variants for a tactical candidate */
int shot_candidates_variants(const DecisionSnapshot* snap, ShotCandidate* base, ShotCandidate* out_variants, int max_variants, PCG32* rng);

#ifdef __cplusplus
}
#endif

#endif // CARROM_SHOT_CANDIDATES_H