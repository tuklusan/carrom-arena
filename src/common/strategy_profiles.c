#include "strategy_profiles.h"

/* Strategy profiles are fully defined in header as static const */
/* This file exists for compilation unit completeness */

#include <stdio.h>

void strategy_profiles_print_all(void) {
    for (int i = 0; i < NUM_STRATEGIES; i++) {
        const StrategyProfile* p = &STRATEGY_PROFILES[i];
        printf("Strategy %d (%s):\n", i, strategy_name((uint8_t)i));
        printf("  pocket=%.2f queen=%.2f cover=%.2f striker_risk=%.2f opponent_leave=%.2f positional=%.2f\n",
               (double)p->weight_pocket, (double)p->weight_queen, (double)p->weight_cover,
               (double)p->weight_striker_risk, (double)p->weight_opponent_leave, (double)p->weight_positional);
        printf("  aim_noise=%.4f power_noise=%.4f\n", (double)p->aim_noise_std, (double)p->power_noise_std);
    }
}