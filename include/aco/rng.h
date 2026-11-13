/**
 * @file rng.h
 * @brief Small, fast and reproducible pseudo-random number generator.
 *
 * The generator is xoshiro256** (Blackman and Vigna) seeded through
 * SplitMix64. Unlike `rand()`, its output is identical on every platform,
 * its state is explicit (so independent colonies never interfere) and its
 * low bits are of good quality.
 *
 * It is not cryptographically secure.
 */
#ifndef ACO_RNG_H
#define ACO_RNG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Generator state. Treat it as opaque and initialise it with aco_rng_seed(). */
typedef struct aco_rng {
    uint64_t state[4];
} aco_rng;

/** Initialises @p rng from a 64-bit seed. Any seed, including zero, is valid. */
void aco_rng_seed(aco_rng *rng, uint64_t seed);

/** Returns the next 64 uniformly distributed bits. */
uint64_t aco_rng_next(aco_rng *rng);

/** Returns a double uniformly distributed in the half-open interval [0, 1). */
double aco_rng_uniform(aco_rng *rng);

/**
 * Returns an integer uniformly distributed in [0, @p bound).
 *
 * Rejection sampling removes the modulo bias. A @p bound of zero is a
 * contract violation; the function then returns zero.
 */
size_t aco_rng_below(aco_rng *rng, size_t bound);

#ifdef __cplusplus
}
#endif

#endif /* ACO_RNG_H */
