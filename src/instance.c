#include "aco/instance.h"
#include "aco/rng.h"

#include "internal.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

aco_status aco_instance_alloc(size_t size, bool with_points, aco_instance **out, aco_error *error)
{
    aco_instance *instance;

    if (size < ACO_MIN_CITIES || size > ACO_MAX_CITIES) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT,
                        "city count %zu is outside the supported range [%d, %d]", size,
                        ACO_MIN_CITIES, ACO_MAX_CITIES);
    }

    instance = calloc(1, sizeof *instance);
    if (instance == NULL) {
        return ACO_FAIL(error, ACO_ERR_OUT_OF_MEMORY, "out of memory");
    }
    instance->distances = calloc(size * size, sizeof *instance->distances);
    if (with_points) {
        instance->points = calloc(size, sizeof *instance->points);
    }
    if (instance->distances == NULL || (with_points && instance->points == NULL)) {
        aco_instance_destroy(instance);
        return ACO_FAIL(error, ACO_ERR_OUT_OF_MEMORY,
                        "out of memory while allocating a %zu-city instance", size);
    }

    instance->size = size;
    instance->metric = ACO_METRIC_EXPLICIT;
    aco_instance_set_name(instance, "unnamed");
    *out = instance;
    return ACO_OK;
}

double aco_metric_distance(aco_point a, aco_point b, aco_metric metric)
{
    const double distance = hypot(a.x - b.x, a.y - b.y);

    switch (metric) {
    case ACO_METRIC_EUCLIDEAN_ROUNDED:
        return floor(distance + 0.5);
    case ACO_METRIC_EUCLIDEAN_CEIL:
        return ceil(distance);
    case ACO_METRIC_EUCLIDEAN:
    case ACO_METRIC_EXPLICIT:
        break;
    }
    return distance;
}

aco_status aco_instance_fill_from_points(aco_instance *instance, aco_error *error)
{
    const size_t size = instance->size;

    for (size_t i = 0; i < size; ++i) {
        if (!isfinite(instance->points[i].x) || !isfinite(instance->points[i].y)) {
            return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT,
                            "city %zu has a coordinate that is not finite", i + 1);
        }
    }
    for (size_t i = 0; i < size; ++i) {
        instance->distances[i * size + i] = 0.0;
        for (size_t j = i + 1; j < size; ++j) {
            const double distance =
                aco_metric_distance(instance->points[i], instance->points[j], instance->metric);

            if (!isfinite(distance)) {
                return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT,
                                "cities %zu and %zu are too far apart to measure", i + 1, j + 1);
            }
            instance->distances[i * size + j] = distance;
            instance->distances[j * size + i] = distance;
        }
    }
    return ACO_OK;
}

aco_status aco_instance_create_from_matrix(const double *matrix, size_t size, aco_instance **out,
                                           aco_error *error)
{
    aco_instance *instance = NULL;
    aco_status status;

    if (matrix == NULL || out == NULL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "matrix and out must not be NULL");
    }
    status = aco_instance_alloc(size, false, &instance, error);
    if (status != ACO_OK) {
        return status;
    }

    for (size_t i = 0; i < size; ++i) {
        if (matrix[i * size + i] != 0.0) {
            aco_instance_destroy(instance);
            return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT,
                            "the distance from city %zu to itself must be zero", i + 1);
        }
        for (size_t j = i + 1; j < size; ++j) {
            const double forward = matrix[i * size + j];
            const double backward = matrix[j * size + i];

            if (!isfinite(forward) || !isfinite(backward) || forward < 0.0 || backward < 0.0) {
                aco_instance_destroy(instance);
                return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT,
                                "the distance between cities %zu and %zu must be finite and "
                                "non-negative",
                                i + 1, j + 1);
            }
            if (fabs(forward - backward) > ACO_SYMMETRY_TOLERANCE * fmax(1.0, fabs(forward))) {
                aco_instance_destroy(instance);
                return ACO_FAIL(error, ACO_ERR_UNSUPPORTED,
                                "the matrix is not symmetric: d(%zu, %zu) = %g but "
                                "d(%zu, %zu) = %g",
                                i + 1, j + 1, forward, j + 1, i + 1, backward);
            }
            instance->distances[i * size + j] = forward;
            instance->distances[j * size + i] = forward;
        }
    }

    *out = instance;
    return ACO_OK;
}

aco_status aco_instance_create_from_points(const aco_point *points, size_t size, aco_metric metric,
                                           aco_instance **out, aco_error *error)
{
    aco_instance *instance = NULL;
    aco_status status;

    if (points == NULL || out == NULL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "points and out must not be NULL");
    }
    if (metric != ACO_METRIC_EUCLIDEAN && metric != ACO_METRIC_EUCLIDEAN_ROUNDED &&
        metric != ACO_METRIC_EUCLIDEAN_CEIL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT,
                        "a coordinate-based metric is required to build an instance from points");
    }
    status = aco_instance_alloc(size, true, &instance, error);
    if (status != ACO_OK) {
        return status;
    }

    memcpy(instance->points, points, size * sizeof *points);
    instance->metric = metric;
    status = aco_instance_fill_from_points(instance, error);
    if (status != ACO_OK) {
        aco_instance_destroy(instance);
        return status;
    }

    *out = instance;
    return ACO_OK;
}

aco_status aco_instance_create_random(size_t size, uint32_t extent, uint64_t seed,
                                      aco_instance **out, aco_error *error)
{
    aco_instance *instance = NULL;
    aco_status status;
    aco_rng rng;
    char name[ACO_NAME_SIZE];

    if (out == NULL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "out must not be NULL");
    }
    if (extent == 0) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "the extent must be positive");
    }
    status = aco_instance_alloc(size, true, &instance, error);
    if (status != ACO_OK) {
        return status;
    }

    aco_rng_seed(&rng, seed);
    for (size_t i = 0; i < size; ++i) {
        instance->points[i].x = (double)aco_rng_below(&rng, (size_t)extent + 1);
        instance->points[i].y = (double)aco_rng_below(&rng, (size_t)extent + 1);
    }
    instance->metric = ACO_METRIC_EUCLIDEAN_ROUNDED;
    status = aco_instance_fill_from_points(instance, error);
    if (status != ACO_OK) {
        aco_instance_destroy(instance);
        return status;
    }

    (void)snprintf(name, sizeof name, "random%zu", size);
    aco_instance_set_name(instance, name);
    *out = instance;
    return ACO_OK;
}

void aco_instance_destroy(aco_instance *instance)
{
    if (instance == NULL) {
        return;
    }
    free(instance->distances);
    free(instance->points);
    free(instance);
}

size_t aco_instance_size(const aco_instance *instance)
{
    return instance->size;
}

double aco_instance_distance(const aco_instance *instance, size_t from, size_t to)
{
    assert(from < instance->size && to < instance->size);
    return instance->distances[from * instance->size + to];
}

const double *aco_instance_matrix(const aco_instance *instance)
{
    return instance->distances;
}

const aco_point *aco_instance_points(const aco_instance *instance)
{
    return instance->points;
}

aco_metric aco_instance_metric(const aco_instance *instance)
{
    return instance->metric;
}

const char *aco_instance_name(const aco_instance *instance)
{
    return instance->name;
}

void aco_instance_set_name(aco_instance *instance, const char *name)
{
    size_t length = 0;

    if (name == NULL) {
        name = "";
    }
    while (length < sizeof instance->name - 1 && name[length] != '\0') {
        const unsigned char c = (unsigned char)name[length];

        /* Names end up in single-line file headers: keep them printable. */
        instance->name[length] = (char)((c >= 0x20 && c != 0x7F) ? c : '_');
        ++length;
    }
    instance->name[length] = '\0';
}
