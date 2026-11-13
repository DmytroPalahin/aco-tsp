/*
 * Reading instances.
 *
 * Two text formats are supported and told apart automatically:
 *
 *   - the "triangular" format of the original coursework: the number of
 *     cities followed by the strict upper triangle of the distance matrix;
 *   - a subset of TSPLIB 95: EUC_2D and CEIL_2D coordinates, and EXPLICIT
 *     matrices in any of the five common layouts.
 *
 * Parsing works on a memory buffer rather than on a FILE so that it can be
 * unit-tested and fuzzed without touching the file system.
 */
#include "aco/instance.h"

#include "internal.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Files larger than this are rejected instead of being read into memory. */
#define MAX_FILE_BYTES ((size_t)256 * 1024 * 1024)
/* Longest textual number accepted, in characters. */
#define MAX_NUMBER_LENGTH 63

typedef struct reader {
    const char *cursor;
    const char *end;
    size_t line;       /* line that contains `cursor`, counted from one */
    size_t token_line; /* line of the token or header read most recently */
    aco_error *error;
} reader;

/* ------------------------------------------------------------------------ */
/* Lexical helpers                                                          */
/* ------------------------------------------------------------------------ */

static aco_status parse_fail(const reader *in, aco_status status, const char *format, ...)
    ACO_PRINTF_LIKE(3, 4);

static aco_status parse_fail(const reader *in, aco_status status, const char *format, ...)
{
    char detail[ACO_ERROR_MESSAGE_SIZE - 32];
    va_list arguments;

    va_start(arguments, format);
    (void)vsnprintf(detail, sizeof detail, format, arguments);
    va_end(arguments);
    return ACO_FAIL(in->error, status, "line %zu: %s", in->token_line, detail);
}

/* The classic C locale definition, without the locale dependence of isspace(). */
static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char to_upper(char c)
{
    return (char)((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
}

/* Copies at most `capacity - 1` characters of a token, masking unprintable bytes. */
static void excerpt(const char *text, size_t length, char *buffer, size_t capacity)
{
    const size_t count = length < capacity - 1 ? length : capacity - 1;

    for (size_t i = 0; i < count; ++i) {
        const unsigned char c = (unsigned char)text[i];

        buffer[i] = (char)((c >= 0x20 && c < 0x7F) ? c : '?');
    }
    buffer[count] = '\0';
}

static void skip_space(reader *in)
{
    while (in->cursor < in->end && is_space(*in->cursor)) {
        if (*in->cursor == '\n') {
            ++in->line;
        }
        ++in->cursor;
    }
}

static size_t remaining_bytes(const reader *in)
{
    return (size_t)(in->end - in->cursor);
}

static bool next_token(reader *in, const char **token, size_t *length)
{
    skip_space(in);
    in->token_line = in->line;
    if (in->cursor == in->end) {
        return false;
    }
    *token = in->cursor;
    while (in->cursor < in->end && !is_space(*in->cursor)) {
        ++in->cursor;
    }
    *length = (size_t)(in->cursor - *token);
    return true;
}

static bool next_line(reader *in, const char **line, size_t *length)
{
    const char *start = in->cursor;

    if (start == in->end) {
        return false;
    }
    in->token_line = in->line;
    while (in->cursor < in->end && *in->cursor != '\n') {
        ++in->cursor;
    }
    *line = start;
    *length = (size_t)(in->cursor - start);
    if (in->cursor < in->end) {
        ++in->cursor;
        ++in->line;
    }
    return true;
}

static void trim(const char **text, size_t *length)
{
    while (*length > 0 && is_space((*text)[0])) {
        ++*text;
        --*length;
    }
    while (*length > 0 && is_space((*text)[*length - 1])) {
        --*length;
    }
}

/* ASCII case-insensitive comparison of a slice against an upper-case keyword. */
static bool text_equals(const char *text, size_t length, const char *keyword)
{
    size_t i = 0;

    while (i < length && keyword[i] != '\0') {
        if (to_upper(text[i]) != keyword[i]) {
            return false;
        }
        ++i;
    }
    return i == length && keyword[i] == '\0';
}

static bool text_ends_with(const char *text, size_t length, const char *suffix)
{
    const size_t suffix_length = strlen(suffix);

    return length >= suffix_length &&
           text_equals(text + (length - suffix_length), suffix_length, suffix);
}

/* Parses a decimal, unsigned integer made of digits only. */
static bool parse_count(const char *text, size_t length, size_t *value)
{
    size_t result = 0;

    if (length == 0) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (text[i] < '0' || text[i] > '9' || result > (SIZE_MAX - 9) / 10) {
            return false;
        }
        result = result * 10 + (size_t)(text[i] - '0');
    }
    *value = result;
    return true;
}

static aco_status read_count(reader *in, const char *what, size_t *value)
{
    const char *token = NULL;
    size_t length = 0;
    char shown[24];

    if (!next_token(in, &token, &length)) {
        return parse_fail(in, ACO_ERR_PARSE, "unexpected end of input, expected %s", what);
    }
    if (!parse_count(token, length, value)) {
        excerpt(token, length, shown, sizeof shown);
        return parse_fail(in, ACO_ERR_PARSE, "expected %s but found '%s'", what, shown);
    }
    return ACO_OK;
}

static aco_status read_number(reader *in, const char *what, double *value)
{
    const char *token = NULL;
    size_t length = 0;
    char text[MAX_NUMBER_LENGTH + 1];
    char shown[24];
    char *stop = NULL;

    if (!next_token(in, &token, &length)) {
        return parse_fail(in, ACO_ERR_PARSE, "unexpected end of input, expected %s", what);
    }
    excerpt(token, length, shown, sizeof shown);
    if (length > MAX_NUMBER_LENGTH) {
        return parse_fail(in, ACO_ERR_PARSE, "expected %s but found '%s...'", what, shown);
    }
    memcpy(text, token, length);
    text[length] = '\0';
    *value = strtod(text, &stop);
    if (stop != text + length || !isfinite(*value)) {
        return parse_fail(in, ACO_ERR_PARSE, "expected %s but found '%s'", what, shown);
    }
    return ACO_OK;
}

static aco_status read_distance(reader *in, double *value)
{
    const aco_status status = read_number(in, "a distance", value);

    if (status == ACO_OK && *value < 0.0) {
        return parse_fail(in, ACO_ERR_PARSE, "distances must not be negative (found %g)", *value);
    }
    return status;
}

static aco_status check_dimension(const reader *in, size_t size)
{
    if (size < ACO_MIN_CITIES || size > ACO_MAX_CITIES) {
        return parse_fail(in, ACO_ERR_PARSE,
                          "city count %zu is outside the supported range [%d, %d]", size,
                          ACO_MIN_CITIES, ACO_MAX_CITIES);
    }
    return ACO_OK;
}

/* ------------------------------------------------------------------------ */
/* Triangular format                                                        */
/* ------------------------------------------------------------------------ */

static aco_status parse_triangular(reader *in, aco_instance **out)
{
    aco_instance *instance = NULL;
    aco_status status;
    size_t size = 0;
    size_t pairs;

    status = read_count(in, "the number of cities", &size);
    if (status == ACO_OK) {
        status = check_dimension(in, size);
    }
    if (status != ACO_OK) {
        return status;
    }

    /* Every distance takes at least one byte: refuse absurd headers before allocating. */
    pairs = size * (size - 1) / 2;
    if (remaining_bytes(in) < pairs) {
        return parse_fail(in, ACO_ERR_PARSE,
                          "%zu cities need %zu distances, but the input is too short", size, pairs);
    }

    status = aco_instance_alloc(size, false, &instance, in->error);
    for (size_t i = 0; status == ACO_OK && i < size; ++i) {
        for (size_t j = i + 1; status == ACO_OK && j < size; ++j) {
            double distance = 0.0;

            status = read_distance(in, &distance);
            if (status == ACO_OK) {
                instance->distances[i * size + j] = distance;
                instance->distances[j * size + i] = distance;
            }
        }
    }
    if (status == ACO_OK) {
        skip_space(in);
        if (in->cursor != in->end) {
            in->token_line = in->line;
            status = parse_fail(in, ACO_ERR_PARSE,
                                "unexpected data after the %zu distances of a %zu-city instance",
                                pairs, size);
        }
    }

    if (status != ACO_OK) {
        aco_instance_destroy(instance);
        return status;
    }
    *out = instance;
    return ACO_OK;
}

/* ------------------------------------------------------------------------ */
/* TSPLIB format                                                            */
/* ------------------------------------------------------------------------ */

typedef enum weight_type {
    WEIGHT_TYPE_UNSET,
    WEIGHT_TYPE_EXPLICIT,
    WEIGHT_TYPE_EUC_2D,
    WEIGHT_TYPE_CEIL_2D
} weight_type;

typedef enum weight_format {
    WEIGHT_FORMAT_UNSET,
    WEIGHT_FORMAT_FULL_MATRIX,
    WEIGHT_FORMAT_UPPER_ROW,
    WEIGHT_FORMAT_LOWER_ROW,
    WEIGHT_FORMAT_UPPER_DIAG_ROW,
    WEIGHT_FORMAT_LOWER_DIAG_ROW
} weight_format;

typedef struct tsplib {
    size_t dimension; /* zero until DIMENSION has been read */
    weight_type type;
    weight_format format;
    char name[ACO_NAME_SIZE];
    aco_instance *instance; /* allocated by EDGE_WEIGHT_SECTION */
    aco_point *coords;      /* NODE_COORD_SECTION */
    aco_point *display;     /* DISPLAY_DATA_SECTION */
} tsplib;

static aco_status read_points(reader *in, const tsplib *state, const char *section, aco_point **out)
{
    const size_t size = state->dimension;
    aco_status status = ACO_OK;
    aco_point *points;
    unsigned char *seen;

    if (size == 0) {
        return parse_fail(in, ACO_ERR_PARSE, "DIMENSION must appear before %s", section);
    }
    if (*out != NULL) {
        return parse_fail(in, ACO_ERR_PARSE, "%s appears twice", section);
    }
    if (remaining_bytes(in) < size) {
        return parse_fail(in, ACO_ERR_PARSE, "%s is too short for %zu cities", section, size);
    }

    points = calloc(size, sizeof *points);
    seen = calloc(size, sizeof *seen);
    if (points == NULL || seen == NULL) {
        free(points);
        free(seen);
        return ACO_FAIL(in->error, ACO_ERR_OUT_OF_MEMORY, "out of memory");
    }

    for (size_t k = 0; status == ACO_OK && k < size; ++k) {
        size_t id = 0;

        status = read_count(in, "a city number", &id);
        if (status == ACO_OK && (id < 1 || id > size)) {
            status = parse_fail(in, ACO_ERR_PARSE, "city number %zu is outside 1..%zu", id, size);
        }
        if (status == ACO_OK && seen[id - 1] != 0) {
            status = parse_fail(in, ACO_ERR_PARSE, "city %zu is listed twice", id);
        }
        if (status == ACO_OK) {
            seen[id - 1] = 1;
            status = read_number(in, "an x coordinate", &points[id - 1].x);
        }
        if (status == ACO_OK) {
            status = read_number(in, "a y coordinate", &points[id - 1].y);
        }
    }

    free(seen);
    if (status != ACO_OK) {
        free(points);
        return status;
    }
    *out = points;
    return ACO_OK;
}

/* Half-open range of the columns stored on row `row` for a given layout. */
static void stored_columns(weight_format format, size_t row, size_t size, size_t *begin,
                           size_t *end)
{
    switch (format) {
    case WEIGHT_FORMAT_UPPER_ROW:
        *begin = row + 1;
        *end = size;
        return;
    case WEIGHT_FORMAT_LOWER_ROW:
        *begin = 0;
        *end = row;
        return;
    case WEIGHT_FORMAT_UPPER_DIAG_ROW:
        *begin = row;
        *end = size;
        return;
    case WEIGHT_FORMAT_LOWER_DIAG_ROW:
        *begin = 0;
        *end = row + 1;
        return;
    case WEIGHT_FORMAT_FULL_MATRIX:
    case WEIGHT_FORMAT_UNSET:
        break;
    }
    *begin = 0;
    *end = size;
}

static aco_status check_symmetry(const reader *in, const aco_instance *instance)
{
    const size_t size = instance->size;

    for (size_t i = 0; i < size; ++i) {
        for (size_t j = i + 1; j < size; ++j) {
            const double forward = instance->distances[i * size + j];
            const double backward = instance->distances[j * size + i];

            if (fabs(forward - backward) > ACO_SYMMETRY_TOLERANCE * fmax(1.0, fabs(forward))) {
                return parse_fail(in, ACO_ERR_UNSUPPORTED,
                                  "asymmetric instances are not supported: d(%zu, %zu) = %g but "
                                  "d(%zu, %zu) = %g",
                                  i + 1, j + 1, forward, j + 1, i + 1, backward);
            }
            instance->distances[j * size + i] = forward;
        }
    }
    return ACO_OK;
}

static aco_status read_weights(reader *in, tsplib *state)
{
    const size_t size = state->dimension;
    aco_status status;
    size_t entries;

    if (size == 0) {
        return parse_fail(in, ACO_ERR_PARSE, "DIMENSION must appear before EDGE_WEIGHT_SECTION");
    }
    if (state->format == WEIGHT_FORMAT_UNSET) {
        return parse_fail(in, ACO_ERR_PARSE,
                          "EDGE_WEIGHT_FORMAT must appear before EDGE_WEIGHT_SECTION");
    }
    if (state->instance != NULL) {
        return parse_fail(in, ACO_ERR_PARSE, "EDGE_WEIGHT_SECTION appears twice");
    }

    entries = 0;
    for (size_t row = 0; row < size; ++row) {
        size_t begin = 0;
        size_t end = 0;

        stored_columns(state->format, row, size, &begin, &end);
        entries += end - begin;
    }
    if (remaining_bytes(in) < entries) {
        return parse_fail(in, ACO_ERR_PARSE,
                          "EDGE_WEIGHT_SECTION is too short for %zu cities (%zu values expected)",
                          size, entries);
    }

    status = aco_instance_alloc(size, false, &state->instance, in->error);
    for (size_t row = 0; status == ACO_OK && row < size; ++row) {
        size_t begin = 0;
        size_t end = 0;

        stored_columns(state->format, row, size, &begin, &end);
        for (size_t column = begin; status == ACO_OK && column < end; ++column) {
            double weight = 0.0;

            if (row == column) {
                /* Files disagree on what the diagonal holds; it is always zero for us. */
                status = read_number(in, "a distance", &weight);
                continue;
            }
            status = read_distance(in, &weight);
            if (status == ACO_OK) {
                state->instance->distances[row * size + column] = weight;
                if (state->format != WEIGHT_FORMAT_FULL_MATRIX) {
                    state->instance->distances[column * size + row] = weight;
                }
            }
        }
    }
    if (status == ACO_OK && state->format == WEIGHT_FORMAT_FULL_MATRIX) {
        status = check_symmetry(in, state->instance);
    }
    return status;
}

static aco_status read_dimension(reader *in, tsplib *state, const char *value, size_t length)
{
    char shown[24];
    size_t size = 0;

    if (state->dimension != 0) {
        return parse_fail(in, ACO_ERR_PARSE, "DIMENSION appears twice");
    }
    if (!parse_count(value, length, &size)) {
        excerpt(value, length, shown, sizeof shown);
        return parse_fail(in, ACO_ERR_PARSE, "DIMENSION must be a positive integer, not '%s'",
                          shown);
    }
    if (check_dimension(in, size) != ACO_OK) {
        return ACO_ERR_PARSE;
    }
    state->dimension = size;
    return ACO_OK;
}

static aco_status read_weight_type(reader *in, tsplib *state, const char *value, size_t length)
{
    char shown[24];

    if (text_equals(value, length, "EXPLICIT")) {
        state->type = WEIGHT_TYPE_EXPLICIT;
    } else if (text_equals(value, length, "EUC_2D")) {
        state->type = WEIGHT_TYPE_EUC_2D;
    } else if (text_equals(value, length, "CEIL_2D")) {
        state->type = WEIGHT_TYPE_CEIL_2D;
    } else {
        excerpt(value, length, shown, sizeof shown);
        return parse_fail(in, ACO_ERR_UNSUPPORTED,
                          "EDGE_WEIGHT_TYPE '%s' is not supported (use EUC_2D, CEIL_2D or "
                          "EXPLICIT)",
                          shown);
    }
    return ACO_OK;
}

static aco_status read_weight_format(reader *in, tsplib *state, const char *value, size_t length)
{
    char shown[24];

    if (text_equals(value, length, "FULL_MATRIX")) {
        state->format = WEIGHT_FORMAT_FULL_MATRIX;
    } else if (text_equals(value, length, "UPPER_ROW")) {
        state->format = WEIGHT_FORMAT_UPPER_ROW;
    } else if (text_equals(value, length, "LOWER_ROW")) {
        state->format = WEIGHT_FORMAT_LOWER_ROW;
    } else if (text_equals(value, length, "UPPER_DIAG_ROW")) {
        state->format = WEIGHT_FORMAT_UPPER_DIAG_ROW;
    } else if (text_equals(value, length, "LOWER_DIAG_ROW")) {
        state->format = WEIGHT_FORMAT_LOWER_DIAG_ROW;
    } else {
        excerpt(value, length, shown, sizeof shown);
        return parse_fail(in, ACO_ERR_UNSUPPORTED, "EDGE_WEIGHT_FORMAT '%s' is not supported",
                          shown);
    }
    return ACO_OK;
}

static aco_status handle_keyword(reader *in, tsplib *state, const char *key, size_t key_length,
                                 const char *value, size_t value_length, bool is_field)
{
    char shown[40];

    if (text_equals(key, key_length, "NAME")) {
        excerpt(value, value_length, state->name, sizeof state->name);
        return ACO_OK;
    }
    if (text_equals(key, key_length, "TYPE")) {
        if (!text_equals(value, value_length, "TSP")) {
            excerpt(value, value_length, shown, sizeof shown);
            return parse_fail(in, ACO_ERR_UNSUPPORTED,
                              "problem TYPE '%s' is not supported (only symmetric TSP is)", shown);
        }
        return ACO_OK;
    }
    if (text_equals(key, key_length, "DIMENSION")) {
        return read_dimension(in, state, value, value_length);
    }
    if (text_equals(key, key_length, "EDGE_WEIGHT_TYPE")) {
        return read_weight_type(in, state, value, value_length);
    }
    if (text_equals(key, key_length, "EDGE_WEIGHT_FORMAT")) {
        return read_weight_format(in, state, value, value_length);
    }
    if (text_equals(key, key_length, "NODE_COORD_SECTION")) {
        return read_points(in, state, "NODE_COORD_SECTION", &state->coords);
    }
    if (text_equals(key, key_length, "DISPLAY_DATA_SECTION")) {
        return read_points(in, state, "DISPLAY_DATA_SECTION", &state->display);
    }
    if (text_equals(key, key_length, "EDGE_WEIGHT_SECTION")) {
        return read_weights(in, state);
    }

    excerpt(key, key_length, shown, sizeof shown);
    if (text_ends_with(key, key_length, "_SECTION")) {
        return parse_fail(in, ACO_ERR_UNSUPPORTED, "section '%s' is not supported", shown);
    }
    if (is_field) {
        /* COMMENT, NODE_COORD_TYPE, DISPLAY_DATA_TYPE and friends carry nothing we need. */
        return ACO_OK;
    }
    return parse_fail(in, ACO_ERR_PARSE, "'%s' is neither a city count nor a TSPLIB keyword",
                      shown);
}

static aco_status finish_tsplib(reader *in, tsplib *state)
{
    aco_status status = ACO_OK;

    in->token_line = in->line;
    if (state->dimension == 0) {
        return parse_fail(in, ACO_ERR_PARSE, "the DIMENSION field is missing");
    }

    switch (state->type) {
    case WEIGHT_TYPE_UNSET:
        return parse_fail(in, ACO_ERR_PARSE, "the EDGE_WEIGHT_TYPE field is missing");
    case WEIGHT_TYPE_EXPLICIT:
        if (state->instance == NULL) {
            return parse_fail(in, ACO_ERR_PARSE, "the EDGE_WEIGHT_SECTION is missing");
        }
        state->instance->metric = ACO_METRIC_EXPLICIT;
        state->instance->points = state->display;
        state->display = NULL;
        break;
    case WEIGHT_TYPE_EUC_2D:
    case WEIGHT_TYPE_CEIL_2D:
        if (state->coords == NULL) {
            return parse_fail(in, ACO_ERR_PARSE, "the NODE_COORD_SECTION is missing");
        }
        if (state->instance == NULL) {
            status = aco_instance_alloc(state->dimension, false, &state->instance, in->error);
            if (status != ACO_OK) {
                return status;
            }
        }
        state->instance->metric = state->type == WEIGHT_TYPE_EUC_2D ? ACO_METRIC_EUCLIDEAN_ROUNDED
                                                                    : ACO_METRIC_EUCLIDEAN_CEIL;
        state->instance->points = state->coords;
        state->coords = NULL;
        status = aco_instance_fill_from_points(state->instance, in->error);
        break;
    }

    if (status == ACO_OK && state->name[0] != '\0') {
        aco_instance_set_name(state->instance, state->name);
    }
    return status;
}

static aco_status parse_tsplib(reader *in, aco_instance **out)
{
    tsplib state = {0, WEIGHT_TYPE_UNSET, WEIGHT_FORMAT_UNSET, {'\0'}, NULL, NULL, NULL};
    aco_status status = ACO_OK;
    const char *line = NULL;
    size_t length = 0;
    bool finished = false;

    while (status == ACO_OK && !finished && next_line(in, &line, &length)) {
        const char *key = line;
        size_t key_length = length;
        const char *value = line + length;
        size_t value_length = 0;
        const char *colon;

        trim(&key, &key_length);
        if (key_length == 0) {
            continue;
        }
        colon = memchr(key, ':', key_length);
        if (colon != NULL) {
            value = colon + 1;
            value_length = key_length - (size_t)(value - key);
            key_length = (size_t)(colon - key);
            trim(&key, &key_length);
            trim(&value, &value_length);
        }
        if (text_equals(key, key_length, "EOF")) {
            finished = true;
        } else {
            status =
                handle_keyword(in, &state, key, key_length, value, value_length, colon != NULL);
        }
    }
    if (status == ACO_OK) {
        status = finish_tsplib(in, &state);
    }

    free(state.coords);
    free(state.display);
    if (status != ACO_OK) {
        aco_instance_destroy(state.instance);
        return status;
    }
    *out = state.instance;
    return ACO_OK;
}

/* ------------------------------------------------------------------------ */
/* Public entry points: parsing                                             */
/* ------------------------------------------------------------------------ */

aco_status aco_instance_parse(const char *text, size_t length, aco_instance **out, aco_error *error)
{
    reader in;

    if (text == NULL || out == NULL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "text and out must not be NULL");
    }
    /* Skip a UTF-8 byte-order mark, which some editors prepend. */
    if (length >= 3 && memcmp(text, "\xEF\xBB\xBF", 3) == 0) {
        text += 3;
        length -= 3;
    }

    in.cursor = text;
    in.end = text + length;
    in.line = 1;
    in.token_line = 1;
    in.error = error;

    skip_space(&in);
    if (in.cursor == in.end) {
        return parse_fail(&in, ACO_ERR_PARSE, "the input is empty");
    }
    if (*in.cursor >= '0' && *in.cursor <= '9') {
        return parse_triangular(&in, out);
    }
    return parse_tsplib(&in, out);
}

static aco_status read_file(const char *path, char **text, size_t *length, aco_error *error)
{
    FILE *stream = fopen(path, "rb");
    char *buffer = NULL;
    size_t capacity = 0;
    size_t used = 0;
    aco_status status = ACO_OK;

    if (stream == NULL) {
        return ACO_FAIL(error, ACO_ERR_IO, "cannot open '%s': %s", path, strerror(errno));
    }

    for (;;) {
        size_t wanted;
        size_t count;

        if (used == capacity) {
            const size_t grown = capacity == 0 ? (size_t)64 * 1024 : capacity * 2;
            char *bigger;

            if (grown > MAX_FILE_BYTES) {
                status = ACO_FAIL(error, ACO_ERR_UNSUPPORTED, "'%s' is larger than %zu MiB", path,
                                  MAX_FILE_BYTES / ((size_t)1024 * 1024));
                break;
            }
            bigger = realloc(buffer, grown);
            if (bigger == NULL) {
                status = ACO_FAIL(error, ACO_ERR_OUT_OF_MEMORY, "out of memory while reading '%s'",
                                  path);
                break;
            }
            buffer = bigger;
            capacity = grown;
        }
        wanted = capacity - used;
        count = fread(buffer + used, 1, wanted, stream);
        used += count;
        if (count < wanted) {
            break; /* end of file, or a read error that ferror() reports below */
        }
    }
    if (status == ACO_OK && ferror(stream)) {
        status = ACO_FAIL(error, ACO_ERR_IO, "cannot read '%s'", path);
    }
    (void)fclose(stream);

    if (status != ACO_OK) {
        free(buffer);
        return status;
    }
    *text = buffer;
    *length = used;
    return ACO_OK;
}

/* Names an instance after its file: "data/course/tsp100.txt" becomes "tsp100". */
static void name_after_path(aco_instance *instance, const char *path)
{
    const char *base = path;
    const char *dot;
    char name[ACO_NAME_SIZE];
    size_t length;

    for (const char *c = path; *c != '\0'; ++c) {
        if (*c == '/' || *c == '\\') {
            base = c + 1;
        }
    }
    dot = strrchr(base, '.');
    length = (dot != NULL && dot != base) ? (size_t)(dot - base) : strlen(base);
    if (length == 0) {
        return;
    }
    excerpt(base, length, name, sizeof name);
    aco_instance_set_name(instance, name);
}

aco_status aco_instance_load(const char *path, aco_instance **out, aco_error *error)
{
    char *text = NULL;
    size_t length = 0;
    aco_instance *instance = NULL;
    aco_status status;

    if (path == NULL || out == NULL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "path and out must not be NULL");
    }
    status = read_file(path, &text, &length, error);
    if (status != ACO_OK) {
        return status;
    }
    status = aco_instance_parse(text, length, &instance, error);
    free(text);
    if (status != ACO_OK) {
        return status;
    }
    assert(instance != NULL);

    /* The triangular format carries no name: fall back on the file name. */
    if (strcmp(instance->name, "unnamed") == 0) {
        name_after_path(instance, path);
    }
    *out = instance;
    return ACO_OK;
}
