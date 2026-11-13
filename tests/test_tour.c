#include "aco/aco.h"

#include "test.h"

#include <stdbool.h>
#include <string.h>

/* Corners of a 3 by 4 rectangle, in order around it. */
static aco_instance *rectangle(void)
{
    static const aco_point corners[4] = {{0.0, 0.0}, {3.0, 0.0}, {3.0, 4.0}, {0.0, 4.0}};
    aco_instance *instance = NULL;

    (void)aco_instance_create_from_points(corners, 4, ACO_METRIC_EUCLIDEAN, &instance, NULL);
    return instance;
}

static void set_order(aco_tour *tour, const size_t *cities)
{
    memcpy(tour->cities, cities, tour->size * sizeof *cities);
}

static bool has_order(const aco_tour *tour, const size_t *cities)
{
    return memcmp(tour->cities, cities, tour->size * sizeof *cities) == 0;
}

static void test_init_gives_the_identity_tour(void)
{
    aco_tour tour;

    REQUIRE_OK(aco_tour_init(&tour, 6));
    EXPECT_EQ_SIZE(tour.size, 6);
    EXPECT_NEAR(tour.length, 0.0, 0.0);
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ_SIZE(tour.cities[i], i);
    }
    aco_tour_free(&tour);
    EXPECT(tour.cities == NULL);
    EXPECT_EQ_SIZE(tour.size, 0);

    aco_tour_free(&tour); /* freeing twice is harmless */
    aco_tour_free(NULL);
    EXPECT_STATUS(aco_tour_init(NULL, 3), ACO_ERR_INVALID_ARGUMENT);
}

static void test_copy_resizes_the_destination(void)
{
    static const size_t order[5] = {4, 2, 0, 1, 3};
    aco_tour source;
    aco_tour destination = ACO_TOUR_INIT;

    REQUIRE_OK(aco_tour_init(&source, 5));
    set_order(&source, order);
    source.length = 12.5;

    REQUIRE_OK(aco_tour_copy(&destination, &source)); /* from empty */
    EXPECT_EQ_SIZE(destination.size, 5);
    EXPECT(has_order(&destination, order));
    EXPECT_NEAR(destination.length, 12.5, 0.0);
    EXPECT(destination.cities != source.cities);

    REQUIRE_OK(aco_tour_copy(&destination, &destination)); /* onto itself */
    EXPECT(has_order(&destination, order));

    EXPECT_STATUS(aco_tour_copy(NULL, &source), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_tour_copy(&destination, NULL), ACO_ERR_INVALID_ARGUMENT);
    aco_tour_free(&source);
    aco_tour_free(&destination);
}

static void test_validity_requires_a_permutation(void)
{
    static const size_t repeated[4] = {0, 1, 1, 3};
    static const size_t out_of_range[4] = {0, 1, 2, 4};
    aco_instance *instance = rectangle();
    aco_tour tour;
    aco_tour shorter;
    aco_tour empty = ACO_TOUR_INIT;

    REQUIRE(instance != NULL);
    REQUIRE_OK(aco_tour_init(&tour, 4));
    REQUIRE_OK(aco_tour_init(&shorter, 3));

    EXPECT(aco_tour_is_valid(instance, &tour));
    set_order(&tour, repeated);
    EXPECT(!aco_tour_is_valid(instance, &tour));
    set_order(&tour, out_of_range);
    EXPECT(!aco_tour_is_valid(instance, &tour));
    EXPECT(!aco_tour_is_valid(instance, &shorter));
    EXPECT(!aco_tour_is_valid(instance, &empty));
    EXPECT(!aco_tour_is_valid(instance, NULL));
    EXPECT(!aco_tour_is_valid(NULL, &tour));

    aco_tour_free(&tour);
    aco_tour_free(&shorter);
    aco_instance_destroy(instance);
}

static void test_measure_closes_the_cycle(void)
{
    static const size_t crossing[4] = {0, 2, 1, 3};
    aco_instance *instance = rectangle();
    aco_tour tour;

    REQUIRE(instance != NULL);
    REQUIRE_OK(aco_tour_init(&tour, 4));

    EXPECT_NEAR(aco_tour_measure(instance, &tour), 14.0, 1e-12); /* the perimeter */
    set_order(&tour, crossing);
    EXPECT_NEAR(aco_tour_measure(instance, &tour), 18.0, 1e-12); /* both diagonals */
    EXPECT_NEAR(tour.length, 0.0, 0.0);                          /* measuring does not store */

    aco_tour_free(&tour);
    aco_instance_destroy(instance);
}

static void test_rotation_keeps_the_cycle(void)
{
    static const size_t original[5] = {3, 1, 4, 0, 2};
    static const size_t from_zero[5] = {0, 2, 3, 1, 4};
    static const size_t from_two[5] = {2, 3, 1, 4, 0};
    aco_tour tour;

    REQUIRE_OK(aco_tour_init(&tour, 5));
    set_order(&tour, original);

    aco_tour_rotate_to(&tour, 3); /* already first */
    EXPECT(has_order(&tour, original));
    aco_tour_rotate_to(&tour, 9); /* absent */
    EXPECT(has_order(&tour, original));
    aco_tour_rotate_to(&tour, 0);
    EXPECT(has_order(&tour, from_zero));
    aco_tour_rotate_to(&tour, 2); /* the second position */
    EXPECT(has_order(&tour, from_two));
    aco_tour_rotate_to(NULL, 0);

    aco_tour_free(&tour);
}

static void test_write_uses_the_tsplib_tour_format(void)
{
    static const char path[] = ACO_TEST_OUTPUT_DIR "/written.tour";
    static const size_t order[3] = {2, 0, 1};
    char content[512] = {'\0'};
    aco_tour tour;
    FILE *stream;
    size_t length;

    REQUIRE_OK(aco_tour_init(&tour, 3));
    set_order(&tour, order);
    tour.length = 42.0;

    stream = fopen(path, "w");
    REQUIRE(stream != NULL);
    EXPECT_STATUS(aco_tour_write(&tour, "triangle", stream), ACO_OK);
    EXPECT(fclose(stream) == 0);

    stream = fopen(path, "r");
    REQUIRE(stream != NULL);
    length = fread(content, 1, sizeof content - 1, stream);
    content[length] = '\0';
    (void)fclose(stream);
    (void)remove(path);

    EXPECT(strstr(content, "NAME : triangle\n") != NULL);
    EXPECT(strstr(content, "TYPE : TOUR\n") != NULL);
    EXPECT(strstr(content, "DIMENSION : 3\n") != NULL);
    EXPECT(strstr(content, "length 42") != NULL);
    /* Cities are numbered from one in files. */
    EXPECT(strstr(content, "TOUR_SECTION\n3\n1\n2\n-1\nEOF\n") != NULL);

    EXPECT_STATUS(aco_tour_write(NULL, "x", stdout), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_tour_write(&tour, "x", NULL), ACO_ERR_INVALID_ARGUMENT);
    aco_tour_free(&tour);
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        TEST_CASE(test_init_gives_the_identity_tour),
        TEST_CASE(test_copy_resizes_the_destination),
        TEST_CASE(test_validity_requires_a_permutation),
        TEST_CASE(test_measure_closes_the_cycle),
        TEST_CASE(test_rotation_keeps_the_cycle),
        TEST_CASE(test_write_uses_the_tsplib_tour_format),
    };

    return test_main(argc, argv, cases, sizeof cases / sizeof cases[0]);
}
