#include "controller.h"
#include "common/types.h"
#include "common/math.h"
#include "common/rng.h"
#include "ai/shot_candidates.h"
#include "ai/shot_evaluator.h"
#include "game/rules.h"
#include "game/board.h"
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------------
 * Base Controller Implementation
 * --------------------------------------------------------------------------- */

struct ControllerImpl {
    Seat seat;
    StrategyProfile profile;
    PCG32* rng;
};

Controller* controller_create(Seat seat, const StrategyProfile* profile, PCG32* rng_stream) {
    Controller* ctrl = calloc(1, sizeof(Controller));
    if (!ctrl) return NULL;
    
    struct ControllerImpl* impl = calloc(1, sizeof(struct ControllerImpl));
    if (!impl) {
        free(ctrl);
        return NULL;
    }
    
    impl->seat = seat;
    impl->profile = *profile;
    impl->rng = rng_stream;
    
    ctrl->impl_state = impl;
    ctrl->rng_state = rng_stream->state;
    ctrl->profile = *profile;
    ctrl->seat = seat;
    
    return ctrl;
}

void controller_destroy(Controller* controller) {
    if (!controller) return;
    free(controller->impl_state);
    free(controller);
}

ShotPlan controller_decide(Controller* self, const DecisionSnapshot* snap, PCG32* rng) {
    if (self && self->decide) {
        return self->decide(self, snap, rng);
    }
    
    // Default implementation - returns zeroed plan (fallback)
    (void)snap;
    (void)rng;
    
    ShotPlan plan;
    shot_plan_init(&plan);
    return plan;
}

ShotPlan controller_fallback_shot(Controller* self, const DecisionSnapshot* snap, PCG32* rng) {
    // Minimal legal shot: place at center of baseline, aim straight, low power
    (void)self;
    (void)rng;
    
    ShotPlan plan;
    shot_plan_init(&plan);
    plan.tactic = TACTIC_FALLBACK;
    
    // Get legal placement at center
    Vec2 placements[8];
    int count = board_get_legal_placements(snap->active_seat, placements, 8);
    if (count > 0) {
        plan.placement = placements[count / 2];  // Middle placement
    } else {
        // Emergency fallback
        switch (snap->active_seat) {
            case SEAT_NORTH: plan.placement = (Vec2){0, BASELINE_Y_NORTH}; break;
            case SEAT_SOUTH: plan.placement = (Vec2){0, BASELINE_Y_SOUTH}; break;
            case SEAT_EAST:  plan.placement = (Vec2){BASELINE_X_EAST, 0}; break;
            case SEAT_WEST:  plan.placement = (Vec2){BASELINE_X_WEST, 0}; break;
        }
    }
    
    // Aim toward center of board
    plan.aim_angle = 0.0f;
    plan.power = 0.3f;
    
    return plan;
}

/* -----------------------------------------------------------------------------
 * Baseline Controller (Random Legal)
 * --------------------------------------------------------------------------- */

struct BaselineController {
    Controller base;
};

static ShotPlan baseline_decide(Controller* self, const DecisionSnapshot* snap, PCG32* rng) {
    (void)self;
    (void)rng;
    
    // Get legal placements
    Vec2 placements[8];
    int count = board_get_legal_placements(snap->active_seat, placements, 8);
    
    if (count == 0) {
        return controller_fallback_shot(self, snap, rng);
    }
    
    // Pick random placement
    uint32_t idx = pcg32_random_bounded(rng, (uint32_t)count);
    ShotPlan plan;
    shot_plan_init(&plan);
    plan.placement = placements[idx];
    plan.tactic = TACTIC_FALLBACK;
    
    // Random aim toward opponent side
    float aim = 0;
    switch (snap->active_seat) {
        case SEAT_NORTH: aim = -M_PI/2.0f + pcg32_random_float(rng) * M_PI; break;
        case SEAT_SOUTH: aim = M_PI/2.0f + pcg32_random_float(rng) * M_PI; break;
        case SEAT_EAST:  aim = M_PI + pcg32_random_float(rng) * M_PI; break;
        case SEAT_WEST:  aim = pcg32_random_float(rng) * M_PI; break;
    }
    plan.aim_angle = math_wrap_angle(aim);
    plan.power = 0.3f + pcg32_random_float(rng) * 0.4f;
    
    return plan;
}

Controller* baseline_controller_create(Seat seat, const StrategyProfile* profile, PCG32* rng_stream) {
    Controller* ctrl = controller_create(seat, profile, rng_stream);
    if (!ctrl) return NULL;
    
    struct BaselineController* impl = calloc(1, sizeof(struct BaselineController));
    if (!impl) {
        controller_destroy(ctrl);
        return NULL;
    }
    
    impl->base = *ctrl;
    free(ctrl->impl_state);
    ctrl->impl_state = impl;
    ctrl->decide = baseline_decide;
    
    return ctrl;
}

/* -----------------------------------------------------------------------------
 * Arena Controller (Full Pipeline)
 * --------------------------------------------------------------------------- */

struct ArenaController {
    Controller base;
    // Pipeline state
    ShotCandidate candidates[MAX_CANDIDATES];
    int candidate_count;
};

static ShotPlan arena_decide(Controller* self, const DecisionSnapshot* snap, PCG32* rng) {
    struct ArenaController* impl = (struct ArenaController*)self->impl_state;
    
    // Save RNG state for restoration after planning
    RNGSnapshot rng_snap = rng_snapshot(rng);
    
    // Step 1-2: Generate candidates (placements + tactical candidates)
    impl->candidate_count = shot_candidates_generate(snap, impl->candidates, MAX_CANDIDATES, rng);
    
    // Step 3-4: Bound aim/power variants (already done in generate)
    
    // Step 5: Scratch simulation for each candidate
    for (int i = 0; i < impl->candidate_count; i++) {
        shot_evaluator_evaluate(&impl->candidates[i], snap, rng);
    }
    
    // Step 6: Score candidates
    shot_evaluator_score_candidates(impl->candidates, impl->candidate_count, snap, &self->profile);
    
    // Step 7: Apply seeded imperfection to best candidate
    int best_idx = 0;
    float best_score = -1e9f;
    for (int i = 0; i < impl->candidate_count; i++) {
        if (impl->candidates[i].score > best_score) {
            best_score = impl->candidates[i].score;
            best_idx = i;
        }
    }
    
    ShotPlan best_plan = impl->candidates[best_idx].plan;
    
    // Apply imperfection
    float aim_noise = self->profile.aim_noise_std * (pcg32_random_float(rng) * 2.0f - 1.0f);
    float power_noise = self->profile.power_noise_std * (pcg32_random_float(rng) * 2.0f - 1.0f);
    best_plan.aim_angle = math_wrap_angle(best_plan.aim_angle + aim_noise);
    best_plan.power = math_clamp(best_plan.power + power_noise, 0.0f, 1.0f);
    best_plan.rng_draw = pcg32_random(rng);
    
    // Step 9: Restore RNG state
    rng_restore(rng, rng_snap);
    
    // Step 10: Validate and return
    if (!match_validate_shot(snap->game, &best_plan)) {
        return controller_fallback_shot(self, snap, rng);
    }
    
    return best_plan;
}

Controller* arena_controller_create(Seat seat, const StrategyProfile* profile, PCG32* rng_stream) {
    Controller* ctrl = controller_create(seat, profile, rng_stream);
    if (!ctrl) return NULL;
    
    struct ArenaController* impl = calloc(1, sizeof(struct ArenaController));
    if (!impl) {
        controller_destroy(ctrl);
        return NULL;
    }
    
    impl->base = *ctrl;
    free(ctrl->impl_state);
    ctrl->impl_state = impl;
    ctrl->decide = arena_decide;
    
    return ctrl;
}