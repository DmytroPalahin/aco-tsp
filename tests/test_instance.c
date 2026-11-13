#include "aco/aco.h"

#include "test.h"

#include <stdbool.h>
#include <string.h>

/* Parses a NUL-terminated string. */
static aco_status parse(const char *text, aco_instance **instance, aco_error *error)
{
    return aco_instance_parse(text, strlen(text), instance, error);
}

/* Checks the invariants every instance must satisfy. */
static bool is_well_formed(const aco_instance *instance)
{
    const size_t size = aco_instance_size(instance);

    for (size_t i = 0; i < size; ++i) {
        if (aco_instance_distance(instance, i, i) != 0.0) {
            return false;
        }
        for (size_t j = 0; j < size; ++j) {
            const double forward = aco_instance_distance(instance, i, j);

            if (!(forward >= 0.0) || forward != aco_instance_distance(instance, j, i)) {
                return false;
            }
        }
    }
    return true;
}

/* Compares a matrix with expected values, entry by entry. */
static bool has_matrix(const aco_instance *instance, const double *expected, size_t size)
{
    if (aco_instance_size(instance) != size) {
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        for (size_t j = 0; j < size; ++j) {
            if (aco_instance_distance(instance, i, j) != expected[i * size + j]) {
                return false;
            }
        }
    }
    return true;
}

static bool same_matrix(const aco_instance *a, const aco_instance *b)
{
    return has_matrix(a, aco_instance_matrix(b), aco_instance_size(b));
}

static bool has_points(const aco_instance *instance, const aco_point *expected)
{
    const aco_point *points = aco_instance_points(instance);

    if (points == NULL) {
        return false;
    }
    for (size_t i = 0; i < aco_instance_size(instance); ++i) {
        if (points[i].x != expected[i].x || points[i].y != expected[i].y) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------------ */
/* Triangular format                                                        */
/* ------------------------------------------------------------------------ */

/* The example given in the 2023 assignment. */
static void test_parses_the_assignment_example(void)
{
    static const char text[] = "5\n"
                               "61 789 204 349\n"
                               "362 135 376\n"
                               "260 45\n"
                               "880\n";
    aco_instance *instance = NULL;

    REQUIRE_OK(parse(text, &instance, NULL));
    EXPECT_EQ_SIZE(aco_instance_size(instance), 5);
    EXPECT_NEAR(aco_instance_distance(instance, 0, 1), 61.0, 0.0);
    EXPECT_NEAR(aco_instance_distance(instance, 0, 4), 349.0, 0.0);
    EXPECT_NEAR(aco_instance_distance(instance, 1, 2), 362.0, 0.0);
    EXPECT_NEAR(aco_instance_distance(instance, 2, 4), 45.0, 0.0);
    EXPECT_NEAR(aco_instance_distance(instance, 4, 3), 880.0, 0.0);
    EXPECT(is_well_formed(instance));
    EXPECT(aco_instance_points(instance) == NULL);
    EXPECT(aco_instance_metric(instance) == ACO_METRIC_EXPLICIT);
    aco_instance_destroy(instance);
}

static void test_layout_and_line_endings_do_not_matter(void)
{
    /* Byte-order mark, CRLF line endings, everything on odd lines, no final newline. */
    static const char text[] = "\xEF\xBB\xBF  3\r\n1\r\n\r\n2\t3";
    aco_instance *instance = NULL;

    REQUIRE_OK(parse(text, &instance, NULL));
    EXPECT_EQ_SIZE(aco_instance_size(instance), 3);
    EXPECT_NEAR(aco_instance_distance(instance, 0, 2), 2.0, 0.0);
    EXPECT_NEAR(aco_instance_distance(instance, 2, 1), 3.0, 0.0);
    aco_instance_destroy(instance);
}

static void test_accepts_real_distances(void)
{
    aco_instance *instance = NULL;

    REQUIRE_OK(parse("3\n1.5 2.25\n3e1\n", &instance, NULL));
    EXPECT_NEAR(aco_instance_distance(instance, 0, 1), 1.5, 0.0);
    EXPECT_NEAR(aco_instance_distance(instance, 0, 2), 2.25, 0.0);
    EXPECT_NEAR(aco_instance_distance(instance, 1, 2), 30.0, 0.0);
    aco_instance_destroy(instance);
}

static void test_parse_does_not_need_a_terminator(void)
{
    /* Only the first nine bytes belong to the instance. */
    static const char buffer[] = {'3', '\n', '1', ' ', '2', '\n', '3', '\n', ' ', '9', '9'};
    aco_instance *instance = NULL;

    REQUIRE_OK(aco_instance_parse(buffer, 9, &instance, NULL));
    EXPECT_EQ_SIZE(aco_instance_size(instance), 3);
    aco_instance_destroy(instance);
}

static void test_rejects_malformed_triangular_input(void)
{
    static const char *const inputs[] = {
        "",                          /* empty */
        " \n\t\n",                   /* blank */
        "0\n",                       /* too few cities */
        "1\n",                       /* too few cities */
        "5001\n",                    /* too many cities */
        "99999999999999999999999\n", /* count overflows */
        "3.5\n1 2 3\n",              /* fractional count */
        "3\n1 2\n",                  /* a distance is missing */
        "3\n1 2 3 4\n",              /* a distance too many */
        "3\n1 -2 3\n",               /* negative distance */
        "3\n1 two 3\n",              /* not a number */
        "3\n1 2x 3\n",               /* trailing garbage in a number */
        "3\n1 nan 3\n",              /* not finite */
        "3\n1 inf 3\n",              /* not finite */
        "3\n1 1e999 3\n",            /* overflows a double */
        "4\n1 2 3\n4 5\n",           /* truncated */
        "3000\n1 2 3\n",             /* far too short for the announced size */
        /* A "number" longer than any real one. */
        "3\n1 2 1234567890123456789012345678901234567890123456789012345678901234567890\n",
    };

    for (size_t i = 0; i < sizeof inputs / sizeof inputs[0]; ++i) {
        aco_instance *instance = NULL;
        aco_error error = ACO_ERROR_INIT;

        EXPECT_STATUS(parse(inputs[i], &instance, &error), ACO_ERR_PARSE);
        EXPECT(instance == NULL);
        EXPECT(error.status == ACO_ERR_PARSE);
        EXPECT(error.message[0] != '\0');
        if (instance != NULL || error.status != ACO_ERR_PARSE) {
            (void)fprintf(stderr, "    while parsing input #%zu\n", i);
        }
        aco_instance_destroy(instance);
    }
}

static void test_parse_errors_name_the_line(void)
{
    aco_instance *instance = NULL;
    aco_error error = ACO_ERROR_INIT;

    EXPECT_STATUS(parse("3\n1 2\noops\n", &instance, &error), ACO_ERR_PARSE);
    EXPECT(strstr(error.message, "line 3") != NULL);
    EXPECT(strstr(error.message, "oops") != NULL);
}

static void test_rejects_null_arguments(void)
{
    aco_instance *instance = NULL;

    EXPECT_STATUS(aco_instance_parse(NULL, 0, &instance, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_parse("3", 1, NULL, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_load(NULL, &instance, NULL), ACO_ERR_INVALID_ARGUMENT);
}

/* ------------------------------------------------------------------------ */
/* TSPLIB format                                                            */
/* ------------------------------------------------------------------------ */

static void test_parses_tsplib_coordinates(void)
{
    static const char text[] = "NAME : rectangle\n"
                               "COMMENT : a 3 by 4 rectangle\n"
                               "TYPE : TSP\n"
                               "DIMENSION : 4\n"
                               "EDGE_WEIGHT_TYPE : EUC_2D\n"
                               "NODE_COORD_SECTION\n"
                               "2 3 0\n" /* city numbers need not be sorted */
                               "1 0 0\n"
                               "3 3.0 4.0\n"
                               "4 0 4e0\n"
                               "EOF\n";
    aco_instance *instance = NULL;
    const aco_point *points;

    REQUIRE_OK(parse(text, &instance, NULL));
    EXPECT(strcmp(aco_instance_name(instance), "rectangle") == 0);
    EXPECT_EQ_SIZE(aco_instance_size(instance), 4);
    EXPECT(aco_instance_metric(instance) == ACO_METRIC_EUCLIDEAN_ROUNDED);
    EXPECT_NEAR(aco_instance_distance(instance, 0, 1), 3.0, 0.0);
    EXPECT_NEAR(aco_instance_distance(instance, 1, 2), 4.0, 0.0);
    EXPECT_NEAR(aco_instance_distance(instance, 0, 2), 5.0, 0.0);
    EXPECT(is_well_formed(instance));

    points = aco_instance_points(instance);
    REQUIRE(points != NULL);
    EXPECT_NEAR(points[1].x, 3.0, 0.0);
    EXPECT_NEAR(points[2].y, 4.0, 0.0);
    aco_instance_destroy(instance);
}

static void test_tsplib_rounding_rules(void)
{
    /* The diagonal of the unit square is 1.414...: 1 when rounded, 2 when rounded up. */
    static const char rounded[] = "DIMENSION:2\nEDGE_WEIGHT_TYPE:EUC_2D\n"
                                  "NODE_COORD_SECTION\n1 0 0\n2 1 1\nEOF\n";
    static const char ceiling[] = "dimension: 2\nedge_weight_type: ceil_2d\n"
                                  "node_coord_section\n1 0 0\n2 1 1\n";
    aco_instance *instance = NULL;

    REQUIRE_OK(parse(rounded, &instance, NULL));
    EXPECT_NEAR(aco_instance_distance(instance, 0, 1), 1.0, 0.0);
    aco_instance_destroy(instance);

    instance = NULL;
    REQUIRE_OK(parse(ceiling, &instance, NULL)); /* keywords are case-insensitive */
    EXPECT_NEAR(aco_instance_distance(instance, 0, 1), 2.0, 0.0);
    EXPECT(aco_instance_metric(instance) == ACO_METRIC_EUCLIDEAN_CEIL);
    aco_instance_destroy(instance);
}

/*
 * One matrix, written in each of the five explicit layouts:
 *
 *     0 1 2 3
 *     1 0 4 5
 *     2 4 0 6
 *     3 5 6 0
 */
static void test_parses_every_explicit_layout(void)
{
    static const char *const layouts[][2] = {
        {"FULL_MATRIX", "0 1 2 3\n1 0 4 5\n2 4 0 6\n3 5 6 0\n"},
        {"UPPER_ROW", "1 2 3\n4 5\n6\n"},
        {"LOWER_ROW", "1\n2 4\n3 5 6\n"},
        {"UPPER_DIAG_ROW", "0 1 2 3\n0 4 5\n0 6\n0\n"},
        {"LOWER_DIAG_ROW", "0\n1 0\n2 4 0\n3 5 6 0\n"},
    };
    static const double expected[16] = {0, 1, 2, 3, 1, 0, 4, 5, 2, 4, 0, 6, 3, 5, 6, 0};

    for (size_t i = 0; i < sizeof layouts / sizeof layouts[0]; ++i) {
        char text[256];
        aco_instance *instance = NULL;
        aco_error error = ACO_ERROR_INIT;

        (void)snprintf(text, sizeof text,
                       "NAME: layouts\nTYPE: TSP\nDIMENSION: 4\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
                       "EDGE_WEIGHT_FORMAT: %s\nEDGE_WEIGHT_SECTION\n%sEOF\n",
                       layouts[i][0], layouts[i][1]);
        EXPECT_STATUS(parse(text, &instance, &error), ACO_OK);
        if (instance == NULL) {
            (void)fprintf(stderr, "    %s: %s\n", layouts[i][0], error.message);
            continue;
        }
        EXPECT(has_matrix(instance, expected, 4));
        EXPECT(aco_instance_points(instance) == NULL);
        aco_instance_destroy(instance);
    }
}

static void test_explicit_diagonal_is_ignored(void)
{
    /* Some TSPLIB files put a large sentinel on the diagonal. */
    static const char text[] = "DIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
                               "EDGE_WEIGHT_FORMAT: FULL_MATRIX\nEDGE_WEIGHT_SECTION\n"
                               "9999 7\n7 9999\nEOF\n";
    aco_instance *instance = NULL;

    REQUIRE_OK(parse(text, &instance, NULL));
    EXPECT(is_well_formed(instance));
    EXPECT_NEAR(aco_instance_distance(instance, 0, 1), 7.0, 0.0);
    aco_instance_destroy(instance);
}

static void test_display_data_provides_coordinates(void)
{
    static const char text[] = "DIMENSION: 3\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
                               "EDGE_WEIGHT_FORMAT: UPPER_ROW\nDISPLAY_DATA_TYPE: TWOD_DISPLAY\n"
                               "EDGE_WEIGHT_SECTION\n10 20\n30\n"
                               "DISPLAY_DATA_SECTION\n1 0.5 1.5\n2 2.5 3.5\n3 4.5 5.5\nEOF\n";
    aco_instance *instance = NULL;
    const aco_point *points;

    REQUIRE_OK(parse(text, &instance, NULL));
    EXPECT(aco_instance_metric(instance) == ACO_METRIC_EXPLICIT);
    EXPECT_NEAR(aco_instance_distance(instance, 0, 2), 20.0, 0.0);
    points = aco_instance_points(instance);
    REQUIRE(points != NULL);
    EXPECT_NEAR(points[2].x, 4.5, 0.0);
    EXPECT_NEAR(points[0].y, 1.5, 0.0);
    aco_instance_destroy(instance);
}

static void test_rejects_malformed_tsplib_input(void)
{
    static const struct {
        const char *text;
        aco_status expected;
    } inputs[] = {
        {"hello world\n", ACO_ERR_PARSE},
        {"TYPE: TSP\nEOF\n", ACO_ERR_PARSE},    /* no DIMENSION */
        {"DIMENSION: 3\nEOF\n", ACO_ERR_PARSE}, /* no EDGE_WEIGHT_TYPE */
        {"DIMENSION: three\n", ACO_ERR_PARSE},
        {"DIMENSION: 1\n", ACO_ERR_PARSE},
        {"DIMENSION: 3\nDIMENSION: 4\n", ACO_ERR_PARSE},
        {"DIMENSION: 3\nEDGE_WEIGHT_TYPE: EUC_2D\nEOF\n", ACO_ERR_PARSE}, /* no coordinates */
        {"EDGE_WEIGHT_TYPE: EUC_2D\nNODE_COORD_SECTION\n1 0 0\n", ACO_ERR_PARSE},
        {"DIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\nNODE_COORD_SECTION\n1 0 0\n", ACO_ERR_PARSE},
        {"DIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\nNODE_COORD_SECTION\n1 0 0\n1 1 1\n",
         ACO_ERR_PARSE}, /* city listed twice */
        {"DIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\nNODE_COORD_SECTION\n1 0 0\n3 1 1\n",
         ACO_ERR_PARSE}, /* city out of range */
        {"DIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\nNODE_COORD_SECTION\n1 0 0\n2 1 one\n",
         ACO_ERR_PARSE},
        {"DIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\nNODE_COORD_SECTION\n1 0 0\n2 1 1\n"
         "NODE_COORD_SECTION\n1 0 0\n2 1 1\n",
         ACO_ERR_PARSE},                                                    /* section repeated */
        {"DIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\nEOF\n", ACO_ERR_PARSE}, /* no weights */
        {"DIMENSION: 500\nEDGE_WEIGHT_TYPE: EUC_2D\nNODE_COORD_SECTION\n1 0 0\n",
         ACO_ERR_PARSE}, /* far too short for the announced size */
        {"DIMENSION: 2000\nEDGE_WEIGHT_TYPE: EXPLICIT\nEDGE_WEIGHT_FORMAT: FULL_MATRIX\n"
         "EDGE_WEIGHT_SECTION\n0 1\n1 0\n",
         ACO_ERR_PARSE}, /* far too short for the announced size */
        {"EDGE_WEIGHT_TYPE: EXPLICIT\nEDGE_WEIGHT_FORMAT: UPPER_ROW\nEDGE_WEIGHT_SECTION\n1\n",
         ACO_ERR_PARSE}, /* weights before DIMENSION */
        {"DIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\nEDGE_WEIGHT_FORMAT: UPPER_ROW\n"
         "EDGE_WEIGHT_SECTION\n5\nEDGE_WEIGHT_SECTION\n5\n",
         ACO_ERR_PARSE}, /* section repeated */
        {"DIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\nEDGE_WEIGHT_SECTION\n5\n", ACO_ERR_PARSE},
        {"DIMENSION: 3\nEDGE_WEIGHT_TYPE: EXPLICIT\nEDGE_WEIGHT_FORMAT: UPPER_ROW\n"
         "EDGE_WEIGHT_SECTION\n1 2\n",
         ACO_ERR_PARSE}, /* a weight is missing */
        {"DIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\nEDGE_WEIGHT_FORMAT: UPPER_ROW\n"
         "EDGE_WEIGHT_SECTION\n-5\n",
         ACO_ERR_PARSE},
        {"TYPE: ATSP\n", ACO_ERR_UNSUPPORTED},
        {"DIMENSION: 3\nEDGE_WEIGHT_TYPE: GEO\n", ACO_ERR_UNSUPPORTED},
        {"DIMENSION: 3\nEDGE_WEIGHT_TYPE: EXPLICIT\nEDGE_WEIGHT_FORMAT: UPPER_COL\n",
         ACO_ERR_UNSUPPORTED},
        {"DIMENSION: 3\nTOUR_SECTION\n1\n2\n3\n-1\n", ACO_ERR_UNSUPPORTED},
        {"DIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\nEDGE_WEIGHT_FORMAT: FULL_MATRIX\n"
         "EDGE_WEIGHT_SECTION\n0 1\n2 0\n",
         ACO_ERR_UNSUPPORTED}, /* asymmetric */
    };

    for (size_t i = 0; i < sizeof inputs / sizeof inputs[0]; ++i) {
        aco_instance *instance = NULL;
        aco_error error = ACO_ERROR_INIT;
        const aco_status status = parse(inputs[i].text, &instance, &error);

        EXPECT(status == inputs[i].expected);
        EXPECT(instance == NULL);
        EXPECT(error.status == status);
        if (status != inputs[i].expected || instance != NULL) {
            (void)fprintf(stderr, "    input #%zu gave \"%s\": %s\n", i, aco_status_string(status),
                          error.message);
        }
        aco_instance_destroy(instance);
    }
}

/* ------------------------------------------------------------------------ */
/* Construction from memory                                                 */
/* ------------------------------------------------------------------------ */

static void test_creates_from_matrix(void)
{
    static const double matrix[9] = {0, 2, 9, 2, 0, 6, 9, 6, 0};
    aco_instance *instance = NULL;

    REQUIRE_OK(aco_instance_create_from_matrix(matrix, 3, &instance, NULL));
    EXPECT_EQ_SIZE(aco_instance_size(instance), 3);
    EXPECT_NEAR(aco_instance_distance(instance, 2, 1), 6.0, 0.0);
    EXPECT(has_matrix(instance, matrix, 3));
    aco_instance_destroy(instance);
}

static void test_matrix_must_be_a_valid_metric_table(void)
{
    static const double asymmetric[4] = {0, 1, 2, 0};
    static const double negative[4] = {0, -1, -1, 0};
    static const double diagonal[4] = {1, 2, 2, 0};
    const double not_finite[4] = {0, HUGE_VAL, HUGE_VAL, 0};
    aco_instance *instance = NULL;
    aco_error error = ACO_ERROR_INIT;

    EXPECT_STATUS(aco_instance_create_from_matrix(asymmetric, 2, &instance, &error),
                  ACO_ERR_UNSUPPORTED);
    EXPECT(strstr(error.message, "symmetric") != NULL);
    EXPECT_STATUS(aco_instance_create_from_matrix(negative, 2, &instance, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_create_from_matrix(diagonal, 2, &instance, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_create_from_matrix(not_finite, 2, &instance, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_create_from_matrix(asymmetric, 1, &instance, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_create_from_matrix(NULL, 2, &instance, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    EXPECT(instance == NULL);
}

static void test_creates_from_points(void)
{
    static const aco_point points[3] = {{0.0, 0.0}, {1.0, 1.0}, {4.0, 5.0}};
    const aco_point broken[2] = {{0.0, 0.0}, {HUGE_VAL, 1.0}};
    static const aco_point too_far[2] = {{-1e308, -1e308}, {1e308, 1e308}};
    aco_instance *instance = NULL;

    REQUIRE_OK(aco_instance_create_from_points(points, 3, ACO_METRIC_EUCLIDEAN, &instance, NULL));
    EXPECT_NEAR(aco_instance_distance(instance, 0, 1), sqrt(2.0), 1e-15);
    EXPECT_NEAR(aco_instance_distance(instance, 1, 2), 5.0, 1e-15);
    EXPECT(is_well_formed(instance));
    REQUIRE(aco_instance_points(instance) != NULL);
    EXPECT_NEAR(aco_instance_points(instance)[2].x, 4.0, 0.0);
    aco_instance_destroy(instance);

    instance = NULL;
    EXPECT_STATUS(aco_instance_create_from_points(points, 3, ACO_METRIC_EXPLICIT, &instance, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_create_from_points(broken, 2, ACO_METRIC_EUCLIDEAN, &instance, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_create_from_points(NULL, 3, ACO_METRIC_EUCLIDEAN, &instance, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    /* Finite coordinates whose distance overflows a double. */
    EXPECT_STATUS(
        aco_instance_create_from_points(too_far, 2, ACO_METRIC_EUCLIDEAN, &instance, NULL),
        ACO_ERR_INVALID_ARGUMENT);
    EXPECT(instance == NULL);
}

static void test_random_instances_are_reproducible(void)
{
    aco_instance *first = NULL;
    aco_instance *again = NULL;
    aco_instance *other = NULL;
    const aco_point *points;

    REQUIRE_OK(aco_instance_create_random(40, 100, 11, &first, NULL));
    REQUIRE_OK(aco_instance_create_random(40, 100, 11, &again, NULL));
    REQUIRE_OK(aco_instance_create_random(40, 100, 12, &other, NULL));

    EXPECT(same_matrix(first, again));
    EXPECT(!same_matrix(first, other));
    EXPECT(is_well_formed(first));
    EXPECT(aco_instance_metric(first) == ACO_METRIC_EUCLIDEAN_ROUNDED);

    points = aco_instance_points(first);
    REQUIRE(points != NULL);
    for (size_t i = 0; i < 40; ++i) {
        EXPECT(points[i].x >= 0.0 && points[i].x <= 100.0 && points[i].x == floor(points[i].x));
        EXPECT(points[i].y >= 0.0 && points[i].y <= 100.0 && points[i].y == floor(points[i].y));
    }

    aco_instance_destroy(first);
    aco_instance_destroy(again);
    aco_instance_destroy(other);
}

static void test_random_instance_arguments_are_checked(void)
{
    aco_instance *instance = NULL;

    EXPECT_STATUS(aco_instance_create_random(1, 100, 1, &instance, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_create_random(ACO_MAX_CITIES + 1, 100, 1, &instance, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_create_random(10, 0, 1, &instance, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_create_random(10, 100, 1, NULL, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT(instance == NULL);
}

static void test_names_are_sanitised_and_truncated(void)
{
    aco_instance *instance = NULL;
    char long_name[2 * ACO_NAME_SIZE];

    REQUIRE_OK(aco_instance_create_random(3, 10, 1, &instance, NULL));

    aco_instance_set_name(instance, "two\nlines");
    EXPECT(strcmp(aco_instance_name(instance), "two_lines") == 0);

    memset(long_name, 'x', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = '\0';
    aco_instance_set_name(instance, long_name);
    EXPECT_EQ_SIZE(strlen(aco_instance_name(instance)), ACO_NAME_SIZE - 1);

    aco_instance_set_name(instance, NULL);
    EXPECT(strcmp(aco_instance_name(instance), "") == 0);
    aco_instance_destroy(instance);
}

/* ------------------------------------------------------------------------ */
/* Files                                                                    */
/* ------------------------------------------------------------------------ */

/* Writes `instance` to a scratch file and reads it back. */
static aco_status round_trip(const aco_instance *instance, aco_file_format format,
                             aco_instance **reloaded)
{
    static const char path[] = ACO_TEST_OUTPUT_DIR "/round_trip.tsp";
    FILE *stream = fopen(path, "w");
    aco_status status;

    if (stream == NULL) {
        return ACO_ERR_IO;
    }
    status = aco_instance_write(instance, format, stream, NULL);
    if (fclose(stream) != 0 && status == ACO_OK) {
        status = ACO_ERR_IO;
    }
    if (status == ACO_OK) {
        status = aco_instance_load(path, reloaded, NULL);
    }
    (void)remove(path);
    return status;
}

static void test_tsplib_round_trip_keeps_coordinates(void)
{
    aco_instance *original = NULL;
    aco_instance *reloaded = NULL;

    REQUIRE_OK(aco_instance_create_random(25, 500, 3, &original, NULL));
    REQUIRE_OK(round_trip(original, ACO_FILE_FORMAT_TSPLIB, &reloaded));

    EXPECT(same_matrix(original, reloaded));
    EXPECT(strcmp(aco_instance_name(reloaded), aco_instance_name(original)) == 0);
    EXPECT(aco_instance_metric(reloaded) == ACO_METRIC_EUCLIDEAN_ROUNDED);
    REQUIRE(aco_instance_points(reloaded) != NULL);
    EXPECT(has_points(reloaded, aco_instance_points(original)));

    aco_instance_destroy(original);
    aco_instance_destroy(reloaded);
}

static void test_triangular_round_trip_keeps_distances(void)
{
    aco_instance *original = NULL;
    aco_instance *reloaded = NULL;

    REQUIRE_OK(aco_instance_create_random(25, 500, 4, &original, NULL));
    REQUIRE_OK(round_trip(original, ACO_FILE_FORMAT_TRIANGULAR, &reloaded));

    EXPECT(same_matrix(original, reloaded));
    EXPECT(aco_instance_points(reloaded) == NULL);
    EXPECT(strcmp(aco_instance_name(reloaded), "round_trip") == 0); /* named after the file */

    aco_instance_destroy(original);
    aco_instance_destroy(reloaded);
}

static void test_round_trip_is_exact_for_real_distances(void)
{
    /* Exact Euclidean distances are irrational: they must survive the text form bit for bit. */
    static const aco_point points[4] = {{0.1, 0.2}, {3.3, 0.7}, {2.9, 4.1}, {0.6, 3.8}};
    aco_instance *original = NULL;
    aco_instance *as_tsplib = NULL;
    aco_instance *as_triangle = NULL;

    REQUIRE_OK(aco_instance_create_from_points(points, 4, ACO_METRIC_EUCLIDEAN, &original, NULL));
    REQUIRE_OK(round_trip(original, ACO_FILE_FORMAT_TSPLIB, &as_tsplib));
    REQUIRE_OK(round_trip(original, ACO_FILE_FORMAT_TRIANGULAR, &as_triangle));

    EXPECT(same_matrix(original, as_tsplib));
    EXPECT(same_matrix(original, as_triangle));
    /* TSPLIB has no keyword for unrounded distances: they travel as an explicit matrix. */
    EXPECT(aco_instance_metric(as_tsplib) == ACO_METRIC_EXPLICIT);
    REQUIRE(aco_instance_points(as_tsplib) != NULL);
    EXPECT(has_points(as_tsplib, points));

    aco_instance_destroy(original);
    aco_instance_destroy(as_tsplib);
    aco_instance_destroy(as_triangle);
}

static void test_write_checks_its_arguments(void)
{
    aco_instance *instance = NULL;

    REQUIRE_OK(aco_instance_create_random(3, 10, 1, &instance, NULL));
    EXPECT_STATUS(aco_instance_write(NULL, ACO_FILE_FORMAT_TSPLIB, stdout, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_instance_write(instance, ACO_FILE_FORMAT_TSPLIB, NULL, NULL),
                  ACO_ERR_INVALID_ARGUMENT);
    aco_instance_destroy(instance);
}

static void test_loads_the_course_instances(void)
{
    static const struct {
        const char *file;
        const char *name;
        size_t size;
    } instances[] = {
        {"tsp005.txt", "tsp005", 5},   {"tsp010.txt", "tsp010", 10}, {"tsp020.txt", "tsp020", 20},
        {"tsp030.txt", "tsp030", 30},  {"tsp040.txt", "tsp040", 40}, {"tsp050.txt", "tsp050", 50},
        {"tsp100.txt", "tsp100", 100},
    };

    for (size_t i = 0; i < sizeof instances / sizeof instances[0]; ++i) {
        char path[1024];
        aco_instance *instance = NULL;
        aco_error error = ACO_ERROR_INIT;

        (void)snprintf(path, sizeof path, "%s/course/%s", ACO_TEST_DATA_DIR, instances[i].file);
        EXPECT_STATUS(aco_instance_load(path, &instance, &error), ACO_OK);
        if (instance == NULL) {
            (void)fprintf(stderr, "    %s: %s\n", path, error.message);
            continue;
        }
        EXPECT_EQ_SIZE(aco_instance_size(instance), instances[i].size);
        EXPECT(strcmp(aco_instance_name(instance), instances[i].name) == 0);
        EXPECT(is_well_formed(instance));
        aco_instance_destroy(instance);
    }
}

static void test_missing_file_is_an_io_error(void)
{
    aco_instance *instance = NULL;
    aco_error error = ACO_ERROR_INIT;

    EXPECT_STATUS(aco_instance_load(ACO_TEST_OUTPUT_DIR "/no-such-file.tsp", &instance, &error),
                  ACO_ERR_IO);
    EXPECT(strstr(error.message, "no-such-file.tsp") != NULL);
    EXPECT(instance == NULL);
}

static void test_status_strings_are_distinct(void)
{
    static const aco_status statuses[] = {
        ACO_OK,        ACO_ERR_INVALID_ARGUMENT, ACO_ERR_OUT_OF_MEMORY, ACO_ERR_IO,
        ACO_ERR_PARSE, ACO_ERR_UNSUPPORTED,
    };
    const size_t count = sizeof statuses / sizeof statuses[0];

    for (size_t i = 0; i < count; ++i) {
        REQUIRE(aco_status_string(statuses[i]) != NULL);
        for (size_t j = i + 1; j < count; ++j) {
            EXPECT(strcmp(aco_status_string(statuses[i]), aco_status_string(statuses[j])) != 0);
        }
    }
    EXPECT(strcmp(aco_version(), ACO_VERSION_STRING) == 0);
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        TEST_CASE(test_parses_the_assignment_example),
        TEST_CASE(test_layout_and_line_endings_do_not_matter),
        TEST_CASE(test_accepts_real_distances),
        TEST_CASE(test_parse_does_not_need_a_terminator),
        TEST_CASE(test_rejects_malformed_triangular_input),
        TEST_CASE(test_parse_errors_name_the_line),
        TEST_CASE(test_rejects_null_arguments),
        TEST_CASE(test_parses_tsplib_coordinates),
        TEST_CASE(test_tsplib_rounding_rules),
        TEST_CASE(test_parses_every_explicit_layout),
        TEST_CASE(test_explicit_diagonal_is_ignored),
        TEST_CASE(test_display_data_provides_coordinates),
        TEST_CASE(test_rejects_malformed_tsplib_input),
        TEST_CASE(test_creates_from_matrix),
        TEST_CASE(test_matrix_must_be_a_valid_metric_table),
        TEST_CASE(test_creates_from_points),
        TEST_CASE(test_random_instances_are_reproducible),
        TEST_CASE(test_random_instance_arguments_are_checked),
        TEST_CASE(test_names_are_sanitised_and_truncated),
        TEST_CASE(test_tsplib_round_trip_keeps_coordinates),
        TEST_CASE(test_triangular_round_trip_keeps_distances),
        TEST_CASE(test_round_trip_is_exact_for_real_distances),
        TEST_CASE(test_write_checks_its_arguments),
        TEST_CASE(test_loads_the_course_instances),
        TEST_CASE(test_missing_file_is_an_io_error),
        TEST_CASE(test_status_strings_are_distinct),
    };

    return test_main(argc, argv, cases, sizeof cases / sizeof cases[0]);
}
