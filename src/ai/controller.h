#ifndef CARROM_CONTROLLER_H
#define CARROM_CONTROLLER_H

#include "types.h"
#include "shot_candidates.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * AI Controller Interface
 * --------------------------------------------------------------------------- */

#define MAX_CANDIDATES 320

typedef struct Controller Controller;

// Controller decision function
typedef ShotPlan (*ControllerDecideFn)(Controller* self, const DecisionSnapshot* snap, PCG32* rng);

struct Controller {
    ControllerDecideFn decide;
    void* impl_state;
    uint64_t rng_state;
    StrategyProfile profile;
    Seat seat;
};

/* Controller lifecycle */
Controller* controller_create(Seat seat, const StrategyProfile* profile, PCG32* rng_stream);
void controller_destroy(Controller* controller);

/* Main decision function */
ShotPlan controller_decide(Controller* self, const DecisionSnapshot* snap, PCG32* rng);

/* Fallback shot (used when primary decision is invalid) */
ShotPlan controller_fallback_shot(Controller* self, const DecisionSnapshot* snap, PCG32* rng);

/* Controller variants */
Controller* baseline_controller_create(Seat seat, const StrategyProfile* profile, PCG32* rng_stream);
Controller* arena_controller_create(Seat seat, const StrategyProfile* profile, PCG32* rng_stream);

#ifdef __cplusplus
}
#endif

#endif // CARROM_CONTROLLER_H