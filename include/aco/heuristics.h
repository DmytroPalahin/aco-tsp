/**
 * @file heuristics.h
 * @brief Reference solvers: nearest neighbour, 2-opt and exact dynamic programming.
 *
 * These serve three purposes: they initialise the pheromone level of the
 * colony, they optionally polish the tours built by the ants, and they give
 * the baselines (and, for small instances, the optimum) against which the
 * colony is evaluated.
 */
#ifndef ACO_HEURISTICS_H
#define ACO_HEURISTICS_H

#include "aco/instance.h"
#include "aco/status.h"
#include "aco/tour.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Largest instance aco_exact() accepts (about 90 MB of tables at this size). */
#define ACO_EXACT_MAX_CITIES 20

/**
 * Greedy construction: from @p start, repeatedly move to the closest city
 * not visited yet. Ties are broken towards the lowest city index.
 *
 * Runs in O(n^2) time.
 *
 * @param[in,out] tour Initialised tour; resized to the instance size.
 */
aco_status aco_nearest_neighbour(const aco_instance *instance, size_t start, aco_tour *tour);

/**
 * Improves @p tour in place with 2-opt until no exchange of two edges
 * shortens it, then updates its length.
 *
 * Each sweep costs O(n^2). The tour must be a valid tour of @p instance.
 *
 * @param[out] moves If not `NULL`, receives the number of improving exchanges.
 */
aco_status aco_two_opt(const aco_instance *instance, aco_tour *tour, size_t *moves);

/**
 * Computes an optimal tour with the Held-Karp dynamic programme.
 *
 * Time O(n^2 2^n), memory O(n 2^n).
 *
 * @return ::ACO_ERR_UNSUPPORTED when the instance has more than
 *         ::ACO_EXACT_MAX_CITIES cities.
 */
aco_status aco_exact(const aco_instance *instance, aco_tour *tour, aco_error *error);

#ifdef __cplusplus
}
#endif

#endif /* ACO_HEURISTICS_H */
