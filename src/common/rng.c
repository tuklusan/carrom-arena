#include "rng.h"

/* PCG32 implementation - all functions inlined in header */
/* This file exists for compilation unit completeness */

#include <stdint.h>

/* Additional non-inline functions if needed */

void pcg32_srandom(PCG32* rng, uint64_t seed, uint64_t seq) {
    pcg32_init(rng, seed, seq);
}

uint32_t pcg32_random_r(PCG32* rng) {
    return pcg32_random(rng);
}