/**
 * @file tour.h
 * @brief A closed tour: a permutation of the cities plus its length.
 */
#ifndef ACO_TOUR_H
#define ACO_TOUR_H

#include "aco/instance.h"
#include "aco/status.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A Hamiltonian cycle.
 *
 * `cities[0], cities[1], ..., cities[size - 1]` is the visiting order; the
 * edge from the last city back to the first one closes the cycle and is
 * included in `length`.
 *
 * A tour owns its array. Start from ::ACO_TOUR_INIT (or aco_tour_init()) and
 * finish with aco_tour_free(). Library functions that produce a tour resize
 * it as needed, so one object can be reused across calls.
 */
typedef struct aco_tour {
    size_t *cities; /**< Visiting order, a permutation of 0 .. size - 1. */
    size_t size;    /**< Number of cities. */
    double length;  /**< Length of the closed tour. */
} aco_tour;

/** Initialiser for an empty tour that owns no memory. */
#define ACO_TOUR_INIT {NULL, 0, 0.0}

/** Allocates a tour of @p size cities in identity order, with length zero. */
aco_status aco_tour_init(aco_tour *tour, size_t size);

/** Releases the storage and resets the tour to ::ACO_TOUR_INIT. */
void aco_tour_free(aco_tour *tour);

/** Copies @p source into @p destination, resizing the latter if necessary. */
aco_status aco_tour_copy(aco_tour *destination, const aco_tour *source);

/**
 * Tells whether @p tour visits every city of @p instance exactly once.
 *
 * @return `false` for a size mismatch, a repeated or out-of-range city, or
 *         if the scratch memory needed for the check cannot be allocated.
 */
bool aco_tour_is_valid(const aco_instance *instance, const aco_tour *tour);

/**
 * Recomputes the length of @p tour on @p instance from scratch.
 *
 * The stored `length` field is ignored and left untouched. The tour size
 * must equal the instance size.
 */
double aco_tour_measure(const aco_instance *instance, const aco_tour *tour);

/**
 * Rotates the visiting order so that it starts at @p city.
 *
 * The cycle itself, and therefore its length, is unchanged; this only gives
 * tours a canonical textual form. Nothing happens if @p city is absent.
 */
void aco_tour_rotate_to(aco_tour *tour, size_t city);

/**
 * Writes the tour in TSPLIB `TOUR` format (cities numbered from one).
 *
 * @param name Value of the `NAME` field; `NULL` selects a default.
 */
aco_status aco_tour_write(const aco_tour *tour, const char *name, FILE *stream);

#ifdef __cplusplus
}
#endif

#endif /* ACO_TOUR_H */
