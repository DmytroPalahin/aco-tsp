/*
 * Writing instances, in the triangular format or as TSPLIB.
 */
#include "aco/instance.h"

#include "internal.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

/* Integers are printed as such; anything else with full round-trip precision. */
static void write_number(FILE *stream, double value)
{
    if (value == floor(value) && fabs(value) < 1e15) {
        (void)fprintf(stream, "%.0f", value);
    } else {
        (void)fprintf(stream, "%.17g", value);
    }
}

static void write_upper_rows(const aco_instance *instance, FILE *stream)
{
    const size_t size = instance->size;

    for (size_t i = 0; i + 1 < size; ++i) {
        for (size_t j = i + 1; j < size; ++j) {
            if (j > i + 1) {
                (void)fputc(' ', stream);
            }
            write_number(stream, instance->distances[i * size + j]);
        }
        (void)fputc('\n', stream);
    }
}

static void write_points(const aco_instance *instance, FILE *stream)
{
    for (size_t i = 0; i < instance->size; ++i) {
        (void)fprintf(stream, "%zu ", i + 1);
        write_number(stream, instance->points[i].x);
        (void)fputc(' ', stream);
        write_number(stream, instance->points[i].y);
        (void)fputc('\n', stream);
    }
}

static void write_tsplib(const aco_instance *instance, FILE *stream)
{
    const bool has_points = instance->points != NULL;
    const bool by_coordinates = has_points && (instance->metric == ACO_METRIC_EUCLIDEAN_ROUNDED ||
                                               instance->metric == ACO_METRIC_EUCLIDEAN_CEIL);

    (void)fprintf(stream, "NAME : %s\n", instance->name);
    (void)fprintf(stream, "COMMENT : written by aco-tsp\n");
    (void)fprintf(stream, "TYPE : TSP\n");
    (void)fprintf(stream, "DIMENSION : %zu\n", instance->size);

    if (by_coordinates) {
        (void)fprintf(stream, "EDGE_WEIGHT_TYPE : %s\n",
                      instance->metric == ACO_METRIC_EUCLIDEAN_ROUNDED ? "EUC_2D" : "CEIL_2D");
        (void)fprintf(stream, "NODE_COORD_SECTION\n");
        write_points(instance, stream);
    } else {
        (void)fprintf(stream, "EDGE_WEIGHT_TYPE : EXPLICIT\n");
        (void)fprintf(stream, "EDGE_WEIGHT_FORMAT : UPPER_ROW\n");
        if (has_points) {
            (void)fprintf(stream, "DISPLAY_DATA_TYPE : TWOD_DISPLAY\n");
        }
        (void)fprintf(stream, "EDGE_WEIGHT_SECTION\n");
        write_upper_rows(instance, stream);
        if (has_points) {
            (void)fprintf(stream, "DISPLAY_DATA_SECTION\n");
            write_points(instance, stream);
        }
    }
    (void)fprintf(stream, "EOF\n");
}

aco_status aco_instance_write(const aco_instance *instance, aco_file_format format, FILE *stream,
                              aco_error *error)
{
    if (instance == NULL || stream == NULL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "instance and stream must not be NULL");
    }

    switch (format) {
    case ACO_FILE_FORMAT_TRIANGULAR:
        (void)fprintf(stream, "%zu\n", instance->size);
        write_upper_rows(instance, stream);
        break;
    case ACO_FILE_FORMAT_TSPLIB:
        write_tsplib(instance, stream);
        break;
    default:
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "unknown file format");
    }

    if (ferror(stream)) {
        return ACO_FAIL(error, ACO_ERR_IO, "cannot write the instance");
    }
    return ACO_OK;
}
