/*
 * Instances and helpers shared by several test programs.
 */
#ifndef ACO_TEST_FIXTURES_H
#define ACO_TEST_FIXTURES_H

#include "aco/aco.h"

#include <math.h>
#include <stddef.h>

/* Largest circle fixture_circle() can build. */
#define FIXTURE_CIRCLE_MAX 64

/*
 * `size` cities evenly spaced on a circle, numbered in a scrambled order.
 *
 * For points in convex position the optimal tour is the convex hull, here a
 * regular polygon, so the optimum is known in closed form. `size` must not
 * be a multiple of 7 (the scrambling stride) nor exceed FIXTURE_CIRCLE_MAX.
 *
 * Returns NULL on failure.
 */
static inline aco_instance *fixture_circle(size_t size, double radius, double *optimum)
{
    const double pi = 3.14159265358979323846;
    aco_point points[FIXTURE_CIRCLE_MAX];
    aco_instance *instance = NULL;

    if (size > FIXTURE_CIRCLE_MAX || size % 7 == 0) {
        return NULL;
    }
    for (size_t city = 0; city < size; ++city) {
        const double angle = 2.0 * pi * (double)((city * 7) % size) / (double)size;

        points[city].x = radius * cos(angle);
        points[city].y = radius * sin(angle);
    }
    if (optimum != NULL) {
        *optimum = (double)size * 2.0 * radius * sin(pi / (double)size);
    }
    if (aco_instance_create_from_points(points, size, ACO_METRIC_EUCLIDEAN, &instance, NULL) !=
        ACO_OK) {
        return NULL;
    }
    return instance;
}

/* Fisher-Yates shuffle of a tour, driven by the library generator. */
static inline void fixture_shuffle(aco_tour *tour, aco_rng *rng)
{
    for (size_t i = tour->size; i > 1; --i) {
        const size_t j = aco_rng_below(rng, i);
        const size_t swap = tour->cities[i - 1];

        tour->cities[i - 1] = tour->cities[j];
        tour->cities[j] = swap;
    }
}

#endif /* ACO_TEST_FIXTURES_H */
