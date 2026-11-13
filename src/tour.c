#include "aco/tour.h"

#include "internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

aco_status aco_tour_resize(aco_tour *tour, size_t size)
{
    size_t *cities;

    if (tour->cities != NULL && tour->size == size) {
        return ACO_OK;
    }
    if (size == 0) {
        aco_tour_free(tour);
        return ACO_OK;
    }
    cities = realloc(tour->cities, size * sizeof *cities);
    if (cities == NULL) {
        return ACO_ERR_OUT_OF_MEMORY;
    }
    tour->cities = cities;
    tour->size = size;
    return ACO_OK;
}

aco_status aco_tour_init(aco_tour *tour, size_t size)
{
    aco_status status;

    if (tour == NULL) {
        return ACO_ERR_INVALID_ARGUMENT;
    }
    tour->cities = NULL;
    tour->size = 0;
    tour->length = 0.0;

    status = aco_tour_resize(tour, size);
    if (status != ACO_OK) {
        return status;
    }
    for (size_t i = 0; i < size; ++i) {
        tour->cities[i] = i;
    }
    return ACO_OK;
}

void aco_tour_free(aco_tour *tour)
{
    if (tour == NULL) {
        return;
    }
    free(tour->cities);
    tour->cities = NULL;
    tour->size = 0;
    tour->length = 0.0;
}

aco_status aco_tour_copy(aco_tour *destination, const aco_tour *source)
{
    aco_status status;

    if (destination == NULL || source == NULL) {
        return ACO_ERR_INVALID_ARGUMENT;
    }
    if (destination == source) {
        return ACO_OK;
    }
    status = aco_tour_resize(destination, source->size);
    if (status != ACO_OK) {
        return status;
    }
    if (source->size > 0) {
        memcpy(destination->cities, source->cities, source->size * sizeof *source->cities);
    }
    destination->length = source->length;
    return ACO_OK;
}

bool aco_tour_is_valid(const aco_instance *instance, const aco_tour *tour)
{
    unsigned char *seen;
    bool valid = true;

    if (instance == NULL || tour == NULL || tour->cities == NULL || tour->size != instance->size) {
        return false;
    }
    seen = calloc(tour->size, sizeof *seen);
    if (seen == NULL) {
        return false;
    }
    for (size_t i = 0; valid && i < tour->size; ++i) {
        const size_t city = tour->cities[i];

        if (city >= tour->size || seen[city] != 0) {
            valid = false;
        } else {
            seen[city] = 1;
        }
    }
    free(seen);
    return valid;
}

double aco_cycle_length(const double *distances, size_t size, const size_t *cities)
{
    double length = 0.0;

    for (size_t i = 0; i + 1 < size; ++i) {
        length += distances[cities[i] * size + cities[i + 1]];
    }
    if (size > 1) {
        length += distances[cities[size - 1] * size + cities[0]];
    }
    return length;
}

double aco_tour_measure(const aco_instance *instance, const aco_tour *tour)
{
    assert(tour->size == instance->size);
    return aco_cycle_length(instance->distances, tour->size, tour->cities);
}

static void reverse(size_t *first, size_t *last)
{
    while (first < last) {
        const size_t swap = *first;

        *first++ = *last;
        *last-- = swap;
    }
}

void aco_tour_rotate_to(aco_tour *tour, size_t city)
{
    size_t position = 0;

    if (tour == NULL || tour->cities == NULL) {
        return;
    }
    while (position < tour->size && tour->cities[position] != city) {
        ++position;
    }
    if (position == 0 || position == tour->size) {
        return;
    }
    /* Rotating left by `position` is three reversals. */
    reverse(tour->cities, tour->cities + position - 1);
    reverse(tour->cities + position, tour->cities + tour->size - 1);
    reverse(tour->cities, tour->cities + tour->size - 1);
}

aco_status aco_tour_write(const aco_tour *tour, const char *name, FILE *stream)
{
    if (tour == NULL || stream == NULL || tour->cities == NULL) {
        return ACO_ERR_INVALID_ARGUMENT;
    }

    (void)fprintf(stream, "NAME : %s\n", name != NULL ? name : "tour");
    (void)fprintf(stream, "COMMENT : length %.12g, found by aco-tsp\n", tour->length);
    (void)fprintf(stream, "TYPE : TOUR\n");
    (void)fprintf(stream, "DIMENSION : %zu\n", tour->size);
    (void)fprintf(stream, "TOUR_SECTION\n");
    for (size_t i = 0; i < tour->size; ++i) {
        (void)fprintf(stream, "%zu\n", tour->cities[i] + 1);
    }
    (void)fprintf(stream, "-1\nEOF\n");

    return ferror(stream) ? ACO_ERR_IO : ACO_OK;
}
