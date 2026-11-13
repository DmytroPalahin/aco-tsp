#include "aco/rng.h"

/*
 * xoshiro256** 1.0 and SplitMix64 are public-domain algorithms by David
 * Blackman and Sebastiano Vigna (https://prng.di.unimi.it/). SplitMix64
 * expands the 64-bit seed into the 256-bit state, as its authors recommend.
 */

static uint64_t rotate_left(uint64_t value, unsigned count)
{
    return (value << count) | (value >> (64U - count));
}

static uint64_t splitmix64(uint64_t *state)
{
    uint64_t z = (*state += UINT64_C(0x9E3779B97F4A7C15));

    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

void aco_rng_seed(aco_rng *rng, uint64_t seed)
{
    for (size_t i = 0; i < 4; ++i) {
        rng->state[i] = splitmix64(&seed);
    }
}

uint64_t aco_rng_next(aco_rng *rng)
{
    uint64_t *s = rng->state;
    const uint64_t result = rotate_left(s[1] * 5U, 7) * 9U;
    const uint64_t shifted = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= shifted;
    s[3] = rotate_left(s[3], 45);
    return result;
}

double aco_rng_uniform(aco_rng *rng)
{
    /* The top 53 bits fill the mantissa of a double exactly: k / 2^53. */
    return (double)(aco_rng_next(rng) >> 11) * (1.0 / 9007199254740992.0);
}

size_t aco_rng_below(aco_rng *rng, size_t bound)
{
    const uint64_t range = (uint64_t)bound;
    uint64_t threshold;
    uint64_t value;

    if (range == 0) {
        return 0;
    }
    /* 2^64 mod range: values below it would make some residues more likely. */
    threshold = (UINT64_C(0) - range) % range;
    do {
        value = aco_rng_next(rng);
    } while (value < threshold);
    return (size_t)(value % range);
}
