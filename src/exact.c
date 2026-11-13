/*
 * Exact solver: the Held-Karp dynamic programme.
 *
 * City 0 is fixed as the start. For every non-empty subset S of the other
 * cities and every city j in S, cost[S][j] is the length of the shortest path
 * that leaves city 0, visits exactly the cities of S and ends in j:
 *
 *     cost[{j}][j] = d(0, j)
 *     cost[S][j]   = min over k in S \ {j} of cost[S \ {j}][k] + d(k, j)
 *
 * The optimal tour closes the best full path back to city 0.
 */
#include "aco/heuristics.h"

#include "internal.h"

#include <math.h>
#include <stdlib.h>

aco_status aco_exact(const aco_instance *instance, aco_tour *tour, aco_error *error)
{
    size_t size;
    size_t others; /* cities 1 .. size - 1, indexed from zero in the tables */
    size_t subsets;
    size_t full;
    size_t last = 0;
    size_t subset;
    const double *distances;
    double *cost;
    unsigned char *previous;
    double best = HUGE_VAL;
    aco_status status;

    if (instance == NULL || tour == NULL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "instance and tour must not be NULL");
    }
    size = instance->size;
    if (size > ACO_EXACT_MAX_CITIES) {
        return ACO_FAIL(error, ACO_ERR_UNSUPPORTED,
                        "the exact solver handles at most %d cities, this instance has %zu",
                        ACO_EXACT_MAX_CITIES, size);
    }
    status = aco_tour_resize(tour, size);
    if (status != ACO_OK) {
        return ACO_FAIL(error, status, "out of memory");
    }

    distances = instance->distances;
    others = size - 1;
    subsets = (size_t)1 << others;
    full = subsets - 1;

    cost = malloc(subsets * others * sizeof *cost);
    previous = malloc(subsets * others * sizeof *previous);
    if (cost == NULL || previous == NULL) {
        free(cost);
        free(previous);
        return ACO_FAIL(error, ACO_ERR_OUT_OF_MEMORY,
                        "out of memory: the exact solver needs %zu table entries",
                        subsets * others);
    }

    for (subset = 1; subset < subsets; ++subset) {
        for (size_t j = 0; j < others; ++j) {
            const size_t without_j = subset & ~((size_t)1 << j);
            double shortest = HUGE_VAL;
            size_t argument = 0;

            if (without_j == subset) {
                continue; /* j is not in the subset: the state does not exist. */
            }
            if (without_j == 0) {
                cost[subset * others + j] = distances[j + 1];
                previous[subset * others + j] = 0;
                continue;
            }
            for (size_t k = 0; k < others; ++k) {
                if ((without_j >> k) & 1U) {
                    const double candidate =
                        cost[without_j * others + k] + distances[(k + 1) * size + (j + 1)];

                    if (candidate < shortest) {
                        shortest = candidate;
                        argument = k;
                    }
                }
            }
            cost[subset * others + j] = shortest;
            previous[subset * others + j] = (unsigned char)argument;
        }
    }

    for (size_t j = 0; j < others; ++j) {
        const double candidate = cost[full * others + j] + distances[(j + 1) * size];

        if (candidate < best) {
            best = candidate;
            last = j;
        }
    }

    /* Walk the table backwards to recover the path, last city first. */
    tour->cities[0] = 0;
    subset = full;
    for (size_t position = size - 1; position >= 1; --position) {
        const size_t before = previous[subset * others + last];

        tour->cities[position] = last + 1;
        subset &= ~((size_t)1 << last);
        last = before;
    }
    tour->length = aco_cycle_length(distances, size, tour->cities);

    free(cost);
    free(previous);
    return ACO_OK;
}
