#include "aco/heuristics.h"

#include "internal.h"

/*
 * An exchange must shorten the tour by more than this fraction of the edges
 * it removes. Without the margin, rounding noise could make two tours each
 * look shorter than the other and 2-opt would never terminate.
 */
#define MIN_RELATIVE_GAIN 1e-12

static void swap_cities(size_t *a, size_t *b)
{
    const size_t swap = *a;

    *a = *b;
    *b = swap;
}

aco_status aco_nearest_neighbour(const aco_instance *instance, size_t start, aco_tour *tour)
{
    aco_status status;
    size_t size;
    size_t *cities;

    if (instance == NULL || tour == NULL || start >= instance->size) {
        return ACO_ERR_INVALID_ARGUMENT;
    }
    size = instance->size;
    status = aco_tour_resize(tour, size);
    if (status != ACO_OK) {
        return status;
    }

    /* cities[0..step) is the path so far, cities[step..size) the cities left. */
    cities = tour->cities;
    for (size_t i = 0; i < size; ++i) {
        cities[i] = i;
    }
    swap_cities(&cities[0], &cities[start]);

    for (size_t step = 1; step < size; ++step) {
        const double *row = instance->distances + cities[step - 1] * size;
        size_t nearest = step;

        for (size_t k = step + 1; k < size; ++k) {
            const double candidate = row[cities[k]];
            const double incumbent = row[cities[nearest]];

            if (candidate < incumbent || (candidate == incumbent && cities[k] < cities[nearest])) {
                nearest = k;
            }
        }
        swap_cities(&cities[step], &cities[nearest]);
    }

    tour->length = aco_cycle_length(instance->distances, size, cities);
    return ACO_OK;
}

size_t aco_two_opt_cycle(const double *distances, size_t size, size_t *cities)
{
    size_t moves = 0;
    bool improved = true;

    if (size < 4) {
        return 0; /* With three cities or fewer every tour is the same cycle. */
    }

    while (improved) {
        improved = false;
        for (size_t i = 0; i + 2 < size; ++i) {
            /* Consider replacing edges (a, b) and (c, d) by (a, c) and (b, d). */
            const size_t a = cities[i];
            size_t b = cities[i + 1];
            /* For i == 0 the last edge shares city `a` and cannot be exchanged. */
            const size_t last = (i == 0) ? size - 1 : size;

            for (size_t j = i + 2; j < last; ++j) {
                const size_t c = cities[j];
                const size_t d = cities[(j + 1) % size];
                const double removed = distances[a * size + b] + distances[c * size + d];
                const double added = distances[a * size + c] + distances[b * size + d];

                if (added < removed - MIN_RELATIVE_GAIN * removed) {
                    size_t low = i + 1;
                    size_t high = j;

                    while (low < high) {
                        swap_cities(&cities[low++], &cities[high--]);
                    }
                    b = cities[i + 1];
                    improved = true;
                    ++moves;
                }
            }
        }
    }
    return moves;
}

aco_status aco_two_opt(const aco_instance *instance, aco_tour *tour, size_t *moves)
{
    size_t count;

    if (instance == NULL || tour == NULL || tour->cities == NULL || tour->size != instance->size) {
        return ACO_ERR_INVALID_ARGUMENT;
    }
    count = aco_two_opt_cycle(instance->distances, tour->size, tour->cities);
    tour->length = aco_cycle_length(instance->distances, tour->size, tour->cities);
    if (moves != NULL) {
        *moves = count;
    }
    return ACO_OK;
}
