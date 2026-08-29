#ifndef CARROM_RNG_H
#define CARROM_RNG_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * PCG32 Random Number Generator (Single-stream, deterministic)
 * Reference: "PCG: A Family of Simple Fast Space-Efficient Statistically
 * Good Algorithms for Random Number Generation" - Melissa O'Neill
 * --------------------------------------------------------------------------- */

/* Generate next 32-bit random number */
static inline uint32_t pcg32_random(PCG32* rng) {
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

/* Initialize with seed and sequence (stream) identifier */
static inline void pcg32_init(PCG32* rng, uint64_t seed, uint64_t seq) {
    rng->state = 0;
    rng->inc = (seq << 1) | 1;
    pcg32_random(rng);
    rng->state += seed;
    pcg32_random(rng);
}

/* Generate random float in [0, 1) */
static inline float pcg32_random_float(PCG32* rng) {
    return (float)pcg32_random(rng) / 4294967296.0f;
}

/* Generate random float in [min, max) */
static inline float pcg32_random_range(PCG32* rng, float min, float max) {
    return min + pcg32_random_float(rng) * (max - min);
}

/* Generate random int in [0, bound) - unbiased */
static inline uint32_t pcg32_random_bounded(PCG32* rng, uint32_t bound) {
    uint32_t threshold = -bound % bound;
    for (;;) {
        uint32_t r = pcg32_random(rng);
        if (r >= threshold) return r % bound;
    }
}

/* Advance RNG by n steps (for stream splitting) */
static inline void pcg32_advance(PCG32* rng, uint64_t n) {
    uint64_t cur_mult = 6364136223846793005ULL;
    uint64_t cur_plus = rng->inc;
    uint64_t acc_mult = 1;
    uint64_t acc_plus = 0;
    
    while (n > 0) {
        if (n & 1) {
            acc_mult *= cur_mult;
            acc_plus = acc_plus * cur_mult + cur_plus;
        }
        cur_plus = (cur_mult + 1) * cur_plus;
        cur_mult *= cur_mult;
        n >>= 1;
    }
    rng->state = acc_mult * rng->state + acc_plus;
}

/* -----------------------------------------------------------------------------
 * Multi-stream RNG for 4 seats (Article 11, A.6)
 * Global seed splits into 4 independent streams via large advance
 * --------------------------------------------------------------------------- */

/* Initialize all 5 streams from master seed */
static inline void rng_context_init(RNGContext* ctx, uint64_t master_seed) {
    ctx->master_seed = master_seed;
    
    // Global stream (seq = 0)
    pcg32_init(&ctx->global, master_seed, 0);
    
    // Per-seat streams
    pcg32_init(&ctx->streams[SEAT_NORTH], master_seed, 1);
    pcg32_init(&ctx->streams[SEAT_EAST],  master_seed, 2);
    pcg32_init(&ctx->streams[SEAT_SOUTH], master_seed, 3);
    pcg32_init(&ctx->streams[SEAT_WEST],  master_seed, 4);
}

/* Get mutable reference to seat's RNG stream */
static inline PCG32* rng_get_seat_stream(RNGContext* ctx, Seat seat) {
    return &ctx->streams[seat];
}

/* Get mutable reference to global RNG stream */
static inline PCG32* rng_get_global(RNGContext* ctx) {
    return &ctx->global;
}

/* Save/restore seat RNG state (for AI scratch simulation isolation) */
static inline RNGSnapshot rng_snapshot(const PCG32* rng) {
    return (RNGSnapshot){ rng->state, rng->inc };
}

static inline void rng_restore(PCG32* rng, RNGSnapshot snap) {
    rng->state = snap.state;
    rng->inc = snap.inc;
}

/* -----------------------------------------------------------------------------
 * Deterministic Seeding Helpers
 * --------------------------------------------------------------------------- */

/* Create seed from game/board/shot identifiers for reproducibility */
static inline uint64_t rng_make_shot_seed(uint64_t master_seed, 
                                           uint32_t game_id, 
                                           uint32_t board_id, 
                                           uint32_t shot_number) {
    // Mix identifiers into seed using splitmix64
    uint64_t z = master_seed + 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    
    z += game_id * 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    
    z += board_id * 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    
    z += shot_number * 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    
    return z;
}

#ifdef __cplusplus
}
#endif

#endif // CARROM_RNG_H