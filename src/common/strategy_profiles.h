#ifndef CARROM_STRATEGY_PROFILES_H
#define CARROM_STRATEGY_PROFILES_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Strategy Profiles (CEO Approved D5 - Immutable after Phase 3 start)
 * 
 * Weight Vector: (pocket, queen, cover, striker_risk, opponent_leave, positional)
 * Imperfection: (aim_noise_std, power_noise_std)
 * --------------------------------------------------------------------------- */

// Profile indices matching seat assignment
#define STRATEGY_AGGRESSIVE   0  // NORTH (White)
#define STRATEGY_BALANCED     1  // SOUTH (White)
#define STRATEGY_DEFENSIVE    2  // EAST  (Black)
#define STRATEGY_TRICKSTER    3  // WEST  (Black)
#define NUM_STRATEGIES        4

/* Default imperfection parameters (tuned for each archetype) */
#define AGGRESSIVE_AIM_NOISE   0.02f   // ~1.1 degrees
#define AGGRESSIVE_POWER_NOISE 0.03f
#define BALANCED_AIM_NOISE     0.03f   // ~1.7 degrees
#define BALANCED_POWER_NOISE   0.04f
#define DEFENSIVE_AIM_NOISE    0.04f   // ~2.3 degrees
#define DEFENSIVE_POWER_NOISE  0.05f
#define TRICKSTER_AIM_NOISE    0.05f   // ~2.9 degrees
#define TRICKSTER_POWER_NOISE  0.06f

/* Immutable strategy profile table */
static const StrategyProfile STRATEGY_PROFILES[NUM_STRATEGIES] = {
    // NORTH (White) - AGGRESSIVE: favors direct pockets, queen attempts
    [STRATEGY_AGGRESSIVE] = {
        .weight_pocket         = 1.0f,
        .weight_queen          = 1.5f,
        .weight_cover          = 1.2f,
        .weight_striker_risk   = -0.8f,
        .weight_opponent_leave = -0.6f,
        .weight_positional     = 0.4f,
        .aim_noise_std         = AGGRESSIVE_AIM_NOISE,
        .power_noise_std       = AGGRESSIVE_POWER_NOISE
    },
    
    // SOUTH (White) - BALANCED: standard tournament play
    [STRATEGY_BALANCED] = {
        .weight_pocket         = 1.0f,
        .weight_queen          = 1.0f,
        .weight_cover          = 1.0f,
        .weight_striker_risk   = -1.0f,
        .weight_opponent_leave = -0.8f,
        .weight_positional     = 0.6f,
        .aim_noise_std         = BALANCED_AIM_NOISE,
        .power_noise_std       = BALANCED_POWER_NOISE
    },
    
    // EAST (Black) - DEFENSIVE: prioritizes cover, safety, positional control
    [STRATEGY_DEFENSIVE] = {
        .weight_pocket         = 0.8f,
        .weight_queen          = 0.8f,
        .weight_cover          = 1.5f,
        .weight_striker_risk   = -1.2f,
        .weight_opponent_leave = -1.0f,
        .weight_positional     = 1.0f,
        .aim_noise_std         = DEFENSIVE_AIM_NOISE,
        .power_noise_std       = DEFENSIVE_POWER_NOISE
    },
    
    // WEST (Black) - TRICKSTER: favors banks, cuts, unconventional shots
    [STRATEGY_TRICKSTER] = {
        .weight_pocket         = 1.2f,
        .weight_queen          = 1.3f,
        .weight_cover          = 0.8f,
        .weight_striker_risk   = -0.6f,
        .weight_opponent_leave = -0.4f,
        .weight_positional     = 0.3f,
        .aim_noise_std         = TRICKSTER_AIM_NOISE,
        .power_noise_std       = TRICKSTER_POWER_NOISE
    }
};

/* Seat to strategy mapping (CEO approved) */
static inline uint8_t seat_to_strategy(Seat seat) {
    switch (seat) {
        case SEAT_NORTH: return STRATEGY_AGGRESSIVE;
        case SEAT_SOUTH: return STRATEGY_BALANCED;
        case SEAT_EAST:  return STRATEGY_DEFENSIVE;
        case SEAT_WEST:  return STRATEGY_TRICKSTER;
        default:         return STRATEGY_BALANCED;
    }
}

/* Get strategy profile for a seat */
static inline const StrategyProfile* strategy_for_seat(Seat seat) {
    return &STRATEGY_PROFILES[seat_to_strategy(seat)];
}

/* Get strategy profile by index */
static inline const StrategyProfile* strategy_by_index(uint8_t index) {
    if (index < NUM_STRATEGIES) {
        return &STRATEGY_PROFILES[index];
    }
    return &STRATEGY_PROFILES[STRATEGY_BALANCED];
}

/* Strategy name for logging/telemetry */
static inline const char* strategy_name(uint8_t index) {
    switch (index) {
        case STRATEGY_AGGRESSIVE: return "AGGRESSIVE";
        case STRATEGY_BALANCED:   return "BALANCED";
        case STRATEGY_DEFENSIVE:  return "DEFENSIVE";
        case STRATEGY_TRICKSTER:  return "TRICKSTER";
        default:                  return "UNKNOWN";
    }
}

#ifdef __cplusplus
}
#endif

#endif // CARROM_STRATEGY_PROFILES_H