#ifndef CARROM_GEOMETRY_PLANNER_H
#define CARROM_GEOMETRY_PLANNER_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Geometric shot planner (the decision engine's shot generator)
 * Direct, cut, bank (rebound / double) and advance shots built from plane geometry.
 * --------------------------------------------------------------------------- */

typedef struct {
    Vec2 placement;
    float aim_angle;
    float power;         /* estimated launch power 0..1 (verified later by simulation) */
    TacticType tactic;   /* DIRECT, CUT, BANK or BREAK */
    float geom_score;    /* higher = geometrically better; DIRECT > CUT > BANK > BREAK */
    int target_id;       /* piece the striker is meant to drive (-1 for none) */
    int pocket_index;    /* pocket the piece is meant to reach (-1 for none) */
} GeomShot;

/* Plan shots for a seat on the given board. Returns the number written (best first). */
int geometry_plan_shots(const BoardState* board, Seat seat, GeomShot* out, int max_out);

/* Building blocks, exposed for tests */
Vec2 geometry_ghost_ball(Vec2 target, Vec2 pocket);
bool geometry_segment_clear(Vec2 a, Vec2 b, float clearance, const Vec2* obstacles, const int* ids, int n, int skip_id);
float geometry_power_for(float object_dist, float striker_dist, float cos_cut, float object_keep, float striker_keep);

#ifdef __cplusplus
}
#endif

#endif /* CARROM_GEOMETRY_PLANNER_H */
