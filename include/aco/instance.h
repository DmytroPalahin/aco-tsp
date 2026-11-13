/**
 * @file instance.h
 * @brief Symmetric travelling-salesman instances: construction, I/O, access.
 *
 * An ::aco_instance is an immutable, validated distance matrix: square,
 * symmetric, zero on the diagonal, finite and non-negative everywhere. It may
 * additionally remember the planar coordinates it was derived from, which is
 * what makes plotting a tour possible.
 *
 * Cities are numbered from zero in the API. Files number them from one.
 */
#ifndef ACO_INSTANCE_H
#define ACO_INSTANCE_H

#include "aco/status.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Smallest instance the library accepts. */
#define ACO_MIN_CITIES 2
/** Largest instance the library accepts (the matrix alone is then 200 MB). */
#define ACO_MAX_CITIES 5000
/** Capacity of an instance name, terminator included. */
#define ACO_NAME_SIZE 64

/** Opaque instance handle. */
typedef struct aco_instance aco_instance;

/** A city position in the plane. */
typedef struct aco_point {
    double x;
    double y;
} aco_point;

/** How the distance matrix relates to the city coordinates. */
typedef enum aco_metric {
    ACO_METRIC_EXPLICIT,          /**< Distances were supplied directly. */
    ACO_METRIC_EUCLIDEAN,         /**< Exact Euclidean distance. */
    ACO_METRIC_EUCLIDEAN_ROUNDED, /**< Rounded to nearest integer (TSPLIB `EUC_2D`). */
    ACO_METRIC_EUCLIDEAN_CEIL     /**< Rounded up (TSPLIB `CEIL_2D`). */
} aco_metric;

/** On-disk representations understood by aco_instance_write(). */
typedef enum aco_file_format {
    /** City count followed by the strict upper triangle, row by row. */
    ACO_FILE_FORMAT_TRIANGULAR,
    /** TSPLIB 95 (`EUC_2D`, `CEIL_2D` or `EXPLICIT`/`UPPER_ROW`). */
    ACO_FILE_FORMAT_TSPLIB
} aco_file_format;

/**
 * Builds an instance from a full, row-major @p size x @p size matrix.
 *
 * The matrix is copied. It must be symmetric with a zero diagonal and contain
 * only finite, non-negative values.
 *
 * @param[out] out Receives the new instance; release it with
 *                 aco_instance_destroy().
 */
aco_status aco_instance_create_from_matrix(const double *matrix, size_t size, aco_instance **out,
                                           aco_error *error);

/**
 * Builds an instance from planar coordinates.
 *
 * @param metric Any metric except ::ACO_METRIC_EXPLICIT.
 */
aco_status aco_instance_create_from_points(const aco_point *points, size_t size, aco_metric metric,
                                           aco_instance **out, aco_error *error);

/**
 * Builds a random instance: @p size cities drawn uniformly from the integer
 * grid [0, @p extent]^2, with ::ACO_METRIC_EUCLIDEAN_ROUNDED distances.
 *
 * The result depends only on the arguments, never on the platform.
 */
aco_status aco_instance_create_random(size_t size, uint32_t extent, uint64_t seed,
                                      aco_instance **out, aco_error *error);

/**
 * Parses an instance from memory. The format is detected automatically:
 * input that starts with a number is read as the triangular format, anything
 * else as TSPLIB.
 *
 * @param text   Buffer to parse; it does not need to be NUL-terminated.
 * @param length Number of bytes in @p text.
 */
aco_status aco_instance_parse(const char *text, size_t length, aco_instance **out,
                              aco_error *error);

/** Reads the file at @p path and parses it with aco_instance_parse(). */
aco_status aco_instance_load(const char *path, aco_instance **out, aco_error *error);

/** Serialises @p instance to @p stream in the requested @p format. */
aco_status aco_instance_write(const aco_instance *instance, aco_file_format format, FILE *stream,
                              aco_error *error);

/** Releases an instance. `NULL` is accepted and ignored. */
void aco_instance_destroy(aco_instance *instance);

/** Number of cities. */
size_t aco_instance_size(const aco_instance *instance);

/** Distance between two cities. Both indices must be below the size. */
double aco_instance_distance(const aco_instance *instance, size_t from, size_t to);

/** Row-major view of the whole matrix, valid as long as the instance lives. */
const double *aco_instance_matrix(const aco_instance *instance);

/** City coordinates, or `NULL` when the instance has none. */
const aco_point *aco_instance_points(const aco_instance *instance);

/** Relationship between the coordinates and the distances. */
aco_metric aco_instance_metric(const aco_instance *instance);

/** Instance name; never `NULL`. */
const char *aco_instance_name(const aco_instance *instance);

/**
 * Renames the instance. Names longer than ::ACO_NAME_SIZE - 1 characters are
 * truncated and control characters are replaced by underscores.
 */
void aco_instance_set_name(aco_instance *instance, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* ACO_INSTANCE_H */
