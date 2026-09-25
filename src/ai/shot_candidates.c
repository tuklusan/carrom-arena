#include <math.h>
#include "shot_candidates.h"
#include "geometry_planner.h"
#include "types.h"
#include "common/vecmath.h"
#include "board.h"
#include <stdlib.h>

/* -----------------------------------------------------------------------------
 * Legal Placements
 * --------------------------------------------------------------------------- */
int shot_candidates_placements(Seat seat, Vec2* out_placements, int max_placements) {
    return board_get_legal_placements(seat, out_placements, max_placements);
}

/* -----------------------------------------------------------------------------
 * Aim/Power Variants
 * --------------------------------------------------------------------------- */
int shot_candidates_variants(const DecisionSnapshot* snap, ShotCandidate* base, ShotCandidate* out_variants, int max_variants, PCG32* rng) {
    (void)snap;
    (void)rng;
    int count = 0;
    const ShotPlan* plan = &base->plan;

    /* the geometric estimate, a little softer, a little harder */
    const float scale[3] = { 1.0f, 0.88f, 1.12f };
    for (int i = 0; i < 3 && count < max_variants; i++) {
        out_variants[count] = *base;
        out_variants[count].plan.power = math_clamp(plan->power * scale[i], 0.1f, 1.0f);
        count++;
    }
    return count;
}

/* -----------------------------------------------------------------------------
 * Full Candidate Generation Pipeline: geometric planner, then power variants.
 * Candidates come out best geometric shot first; the estimated power of every shot is
 * listed before the softer/harder variants, so a limited time budget is spent on the
 * most promising shots.
 * --------------------------------------------------------------------------- */
int shot_candidates_generate(const DecisionSnapshot* snap, ShotCandidate* out_candidates, int max_candidates, PCG32* rng) {
    if (!snap || !snap->board || !out_candidates || max_candidates <= 0) return 0;

    /* room for every shot the planner can construct (15 placements x 19 targets x 4 pockets x 9 families would be the
     * theoretical bound, far above what geometry allows): a smaller cap silently dropped the last placements */
    const int geom_cap = 4096;
    GeomShot* geom = malloc(sizeof(GeomShot) * (size_t)geom_cap);
    if (!geom) return 0;
    int gcount = geometry_plan_shots(snap->board, snap->active_seat, geom, geom_cap);

    int total = 0;
    for (int pass = 0; pass < 3; pass++) {
        for (int g = 0; g < gcount && total < max_candidates; g++) {
            ShotCandidate base = {0};
            base.plan = (ShotPlan){
                .placement = geom[g].placement, .aim_angle = math_wrap_angle(geom[g].aim_angle),
                .power = geom[g].power, .tactic = geom[g].tactic, .rng_draw = 0
            };
            base.tactic = geom[g].tactic;
            base.geom_score = geom[g].geom_score;
            ShotCandidate variants[3];
            int vc = shot_candidates_variants(snap, &base, variants, 3, rng);
            if (pass < vc) out_candidates[total++] = variants[pass];
        }
    }

    free(geom);
    return total;
}
