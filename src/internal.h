/*
 * Declarations shared between the translation units of the library.
 * Nothing in this header is part of the public API.
 */
#ifndef ACO_INTERNAL_H
#define ACO_INTERNAL_H

#include "aco/instance.h"
#include "aco/status.h"
#include "aco/tour.h"

#include <stdbool.h>
#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
#define ACO_PRINTF_LIKE(format_index, first_argument)                                              \
    __attribute__((format(printf, format_index, first_argument)))
#else
#define ACO_PRINTF_LIKE(format_index, first_argument)
#endif

/* Largest asymmetry tolerated between d(i, j) and d(j, i), relative to their size. */
#define ACO_SYMMETRY_TOLERANCE 1e-9

struct aco_instance {
    size_t size;       /* number of cities */
    double *distances; /* size * size entries, row-major */
    aco_point *points; /* size entries, or NULL when coordinates are unknown */
    aco_metric metric;
    char name[ACO_NAME_SIZE];
};

/* Describes a failure in `error`, which may be NULL. */
void aco_error_set(aco_error *error, aco_status status, const char *format, ...)
    ACO_PRINTF_LIKE(3, 4);

/*
 * Records a failure and evaluates to its status, so that a failing function
 * can simply `return ACO_FAIL(error, status, "format", ...)`.
 *
 * This is a macro rather than a function so that the returned status stays
 * visible to compilers and static analysers. `status` is evaluated twice.
 */
#define ACO_FAIL(error, status, ...) (aco_error_set((error), (status), __VA_ARGS__), (status))

/* Allocates an instance whose matrix (and points, if requested) is all zero. */
aco_status aco_instance_alloc(size_t size, bool with_points, aco_instance **out, aco_error *error);

/* Distance between two points under a coordinate-based metric. */
double aco_metric_distance(aco_point a, aco_point b, aco_metric metric);

/* Fills the matrix of `instance` from its points, using its metric. */
aco_status aco_instance_fill_from_points(aco_instance *instance, aco_error *error);

/* Makes `tour` hold exactly `size` cities; the contents are unspecified. */
aco_status aco_tour_resize(aco_tour *tour, size_t size);

/* Length of the closed cycle `cities[0..size)` on a row-major matrix. */
double aco_cycle_length(const double *distances, size_t size, const size_t *cities);

/* 2-opt on a raw cycle; returns the number of improving exchanges applied. */
size_t aco_two_opt_cycle(const double *distances, size_t size, size_t *cities);

#endif /* ACO_INTERNAL_H */
