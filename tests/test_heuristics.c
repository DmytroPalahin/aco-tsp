#include "aco/aco.h"

#include "fixtures.h"
#include "test.h"

#include <stdbool.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Fixtures                                                                 */
/* ------------------------------------------------------------------------ */

/* Cities on a straight line at x = 0, 1, 3, 6 and 10. */
static aco_instance *line(void)
{
    static const aco_point points[5] = {{0, 0}, {1, 0}, {3, 0}, {6, 0}, {10, 0}};
    aco_instance *instance = NULL;

    (void)aco_instance_create_from_points(points, 5, ACO_METRIC_EUCLIDEAN, &instance, NULL);
    return instance;
}

/* Length of the best tour, by trying every permutation that starts at city 0. */
static void enumerate(const aco_instance *instance, size_t *order, size_t depth, double so_far,
                      double *best)
{
    const size_t size = aco_instance_size(instance);

    if (depth == size) {
        const double total = so_far + aco_instance_distance(instance, order[size - 1], order[0]);

        if (total < *best) {
            *best = total;
        }
        return;
    }
    for (size_t i = depth; i < size; ++i) {
        size_t swap = order[depth];

        order[depth] = order[i];
        order[i] = swap;
        enumerate(instance, order, depth + 1,
                  so_far + aco_instance_distance(instance, order[depth - 1], order[depth]), best);
        swap = order[depth];
        order[depth] = order[i];
        order[i] = swap;
    }
}

static double brute_force(const aco_instance *instance)
{
    size_t order[ACO_EXACT_MAX_CITIES];
    double best = HUGE_VAL;

    for (size_t i = 0; i < ACO_EXACT_MAX_CITIES; ++i) {
        order[i] = i;
    }
    enumerate(instance, order, 1, 0.0, &best);
    return best;
}

/* True if no exchange of two edges shortens the tour. */
static bool is_two_optimal(const aco_instance *instance, const aco_tour *tour)
{
    const size_t size = tour->size;

    for (size_t i = 0; i < size; ++i) {
        for (size_t j = i + 2; j < size; ++j) {
            const size_t a = tour->cities[i];
            const size_t b = tour->cities[i + 1];
            const size_t c = tour->cities[j];
            const size_t d = tour->cities[(j + 1) % size];
            const double removed =
                aco_instance_distance(instance, a, b) + aco_instance_distance(instance, c, d);
            const double added =
                aco_instance_distance(instance, a, c) + aco_instance_distance(instance, b, d);

            if (a != d && added < removed - 1e-9 * removed) {
                return false;
            }
        }
    }
    return true;
}

/* ------------------------------------------------------------------------ */
/* Nearest neighbour                                                        */
/* ------------------------------------------------------------------------ */

static void test_nearest_neighbour_follows_the_closest_city(void)
{
    static const size_t from_the_end[5] = {0, 1, 2, 3, 4};
    static const size_t from_the_middle[5] = {2, 1, 0, 3, 4};
    aco_instance *instance = line();
    aco_tour tour = ACO_TOUR_INIT;

    REQUIRE(instance != NULL);

    REQUIRE_OK(aco_nearest_neighbour(instance, 0, &tour));
    EXPECT(memcmp(tour.cities, from_the_end, sizeof from_the_end) == 0);
    EXPECT_NEAR(tour.length, 20.0, 1e-12);

    /* From x = 3: left to 1 (distance 2), on to 0, then all the way right. */
    REQUIRE_OK(aco_nearest_neighbour(instance, 2, &tour));
    EXPECT(memcmp(tour.cities, from_the_middle, sizeof from_the_middle) == 0);
    EXPECT_NEAR(tour.length, 2.0 + 1.0 + 6.0 + 4.0 + 7.0, 1e-12);
    EXPECT(aco_tour_is_valid(instance, &tour));

    aco_tour_free(&tour);
    aco_instance_destroy(instance);
}

static void test_nearest_neighbour_breaks_ties_by_index(void)
{
    static const double equidistant[16] = {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0};
    static const size_t expected[4] = {2, 0, 1, 3};
    aco_instance *instance = NULL;
    aco_tour tour = ACO_TOUR_INIT;

    REQUIRE_OK(aco_instance_create_from_matrix(equidistant, 4, &instance, NULL));
    REQUIRE_OK(aco_nearest_neighbour(instance, 2, &tour));
    EXPECT(memcmp(tour.cities, expected, sizeof expected) == 0);

    aco_tour_free(&tour);
    aco_instance_destroy(instance);
}

static void test_nearest_neighbour_checks_its_arguments(void)
{
    aco_instance *instance = line();
    aco_tour tour = ACO_TOUR_INIT;

    REQUIRE(instance != NULL);
    EXPECT_STATUS(aco_nearest_neighbour(instance, 5, &tour), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_nearest_neighbour(NULL, 0, &tour), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_nearest_neighbour(instance, 0, NULL), ACO_ERR_INVALID_ARGUMENT);
    aco_instance_destroy(instance);
}

/* ------------------------------------------------------------------------ */
/* 2-opt                                                                    */
/* ------------------------------------------------------------------------ */

static void test_two_opt_removes_a_crossing(void)
{
    static const aco_point corners[4] = {{0, 0}, {3, 0}, {3, 4}, {0, 4}};
    static const size_t crossing[4] = {0, 2, 1, 3};
    aco_instance *instance = NULL;
    aco_tour tour;
    size_t moves = 0;

    REQUIRE_OK(aco_instance_create_from_points(corners, 4, ACO_METRIC_EUCLIDEAN, &instance, NULL));
    REQUIRE_OK(aco_tour_init(&tour, 4));
    memcpy(tour.cities, crossing, sizeof crossing);

    REQUIRE_OK(aco_two_opt(instance, &tour, &moves));
    EXPECT_EQ_SIZE(moves, 1);
    EXPECT_NEAR(tour.length, 14.0, 1e-12);
    EXPECT(aco_tour_is_valid(instance, &tour));

    REQUIRE_OK(aco_two_opt(instance, &tour, &moves)); /* already optimal */
    EXPECT_EQ_SIZE(moves, 0);

    aco_tour_free(&tour);
    aco_instance_destroy(instance);
}

static void test_two_opt_reaches_a_local_optimum(void)
{
    aco_rng rng;

    aco_rng_seed(&rng, 2024);
    for (uint64_t seed = 1; seed <= 20; ++seed) {
        aco_instance *instance = NULL;
        aco_tour tour;
        double before;

        REQUIRE_OK(aco_instance_create_random(35, 1000, seed, &instance, NULL));
        REQUIRE_OK(aco_tour_init(&tour, 35));
        fixture_shuffle(&tour, &rng);
        before = aco_tour_measure(instance, &tour);

        REQUIRE_OK(aco_two_opt(instance, &tour, NULL));
        EXPECT(aco_tour_is_valid(instance, &tour));
        EXPECT(tour.length <= before);
        EXPECT_NEAR(tour.length, aco_tour_measure(instance, &tour), 1e-9);
        EXPECT(is_two_optimal(instance, &tour));

        aco_tour_free(&tour);
        aco_instance_destroy(instance);
    }
}

/*
 * A tour without crossing edges through points in convex position can only
 * be the convex hull, so 2-opt must find the optimum from any starting tour.
 */
static void test_two_opt_solves_convex_instances(void)
{
    aco_rng rng;
    double optimum = 0.0;
    aco_instance *instance = fixture_circle(30, 100.0, &optimum);
    aco_tour tour;

    REQUIRE(instance != NULL);
    REQUIRE_OK(aco_tour_init(&tour, 30));
    aco_rng_seed(&rng, 5);
    for (int trial = 0; trial < 10; ++trial) {
        fixture_shuffle(&tour, &rng);
        REQUIRE_OK(aco_two_opt(instance, &tour, NULL));
        EXPECT_NEAR(tour.length, optimum, 1e-9);
    }
    aco_tour_free(&tour);
    aco_instance_destroy(instance);
}

static void test_two_opt_handles_tiny_and_mismatched_tours(void)
{
    static const double triangle[9] = {0, 3, 4, 3, 0, 5, 4, 5, 0};
    aco_instance *instance = NULL;
    aco_tour tour;
    aco_tour wrong_size;
    size_t moves = 99;

    REQUIRE_OK(aco_instance_create_from_matrix(triangle, 3, &instance, NULL));
    REQUIRE_OK(aco_tour_init(&tour, 3));
    REQUIRE_OK(aco_tour_init(&wrong_size, 4));

    REQUIRE_OK(aco_two_opt(instance, &tour, &moves));
    EXPECT_EQ_SIZE(moves, 0);
    EXPECT_NEAR(tour.length, 12.0, 0.0);

    EXPECT_STATUS(aco_two_opt(instance, &wrong_size, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_two_opt(NULL, &tour, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_two_opt(instance, NULL, NULL), ACO_ERR_INVALID_ARGUMENT);

    aco_tour_free(&tour);
    aco_tour_free(&wrong_size);
    aco_instance_destroy(instance);
}

/* ------------------------------------------------------------------------ */
/* Exact solver                                                             */
/* ------------------------------------------------------------------------ */

static void test_exact_matches_brute_force(void)
{
    for (size_t size = 2; size <= 9; ++size) {
        for (uint64_t seed = 1; seed <= 5; ++seed) {
            aco_instance *instance = NULL;
            aco_tour tour = ACO_TOUR_INIT;

            REQUIRE_OK(aco_instance_create_random(size, 100, seed * 31 + size, &instance, NULL));
            REQUIRE_OK(aco_exact(instance, &tour, NULL));

            EXPECT(aco_tour_is_valid(instance, &tour));
            EXPECT_NEAR(tour.length, brute_force(instance), 1e-9);
            EXPECT_NEAR(tour.length, aco_tour_measure(instance, &tour), 1e-9);
            EXPECT_EQ_SIZE(tour.cities[0], 0);

            aco_tour_free(&tour);
            aco_instance_destroy(instance);
        }
    }
}

static void test_exact_solves_the_course_instances(void)
{
    aco_instance *instance = NULL;
    aco_tour tour = ACO_TOUR_INIT;

    REQUIRE_OK(aco_instance_load(ACO_TEST_DATA_DIR "/course/tsp005.txt", &instance, NULL));
    REQUIRE_OK(aco_exact(instance, &tour, NULL));
    EXPECT_NEAR(tour.length, brute_force(instance), 0.0);
    EXPECT_NEAR(tour.length, 64.0, 0.0);
    aco_instance_destroy(instance);

    /* Every distance in tsp010 is at least 1, and the tour 1-2-...-10-1 only uses ones. */
    instance = NULL;
    REQUIRE_OK(aco_instance_load(ACO_TEST_DATA_DIR "/course/tsp010.txt", &instance, NULL));
    REQUIRE_OK(aco_exact(instance, &tour, NULL));
    EXPECT_NEAR(tour.length, 10.0, 0.0);
    aco_instance_destroy(instance);
    aco_tour_free(&tour);
}

static void test_exact_finds_the_convex_hull(void)
{
    double optimum = 0.0;
    aco_instance *instance = fixture_circle(12, 50.0, &optimum);
    aco_tour tour = ACO_TOUR_INIT;

    REQUIRE(instance != NULL);
    REQUIRE_OK(aco_exact(instance, &tour, NULL));
    EXPECT_NEAR(tour.length, optimum, 1e-9);
    aco_tour_free(&tour);
    aco_instance_destroy(instance);
}

static void test_exact_refuses_large_instances(void)
{
    aco_instance *instance = NULL;
    aco_tour tour = ACO_TOUR_INIT;
    aco_error error = ACO_ERROR_INIT;

    REQUIRE_OK(aco_instance_create_random(ACO_EXACT_MAX_CITIES + 1, 100, 1, &instance, NULL));
    EXPECT_STATUS(aco_exact(instance, &tour, &error), ACO_ERR_UNSUPPORTED);
    EXPECT(strstr(error.message, "at most") != NULL);
    EXPECT_STATUS(aco_exact(NULL, &tour, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_exact(instance, NULL, NULL), ACO_ERR_INVALID_ARGUMENT);

    aco_tour_free(&tour);
    aco_instance_destroy(instance);
}

/* The heuristics can never beat the optimum, and 2-opt can only help. */
static void test_heuristics_are_ordered_as_expected(void)
{
    for (uint64_t seed = 1; seed <= 10; ++seed) {
        aco_instance *instance = NULL;
        aco_tour optimal = ACO_TOUR_INIT;
        aco_tour greedy = ACO_TOUR_INIT;
        double greedy_length;

        REQUIRE_OK(aco_instance_create_random(11, 200, seed, &instance, NULL));
        REQUIRE_OK(aco_exact(instance, &optimal, NULL));
        REQUIRE_OK(aco_nearest_neighbour(instance, 0, &greedy));
        greedy_length = greedy.length;
        REQUIRE_OK(aco_two_opt(instance, &greedy, NULL));

        EXPECT(optimal.length <= greedy.length + 1e-9);
        EXPECT(greedy.length <= greedy_length + 1e-9);

        aco_tour_free(&optimal);
        aco_tour_free(&greedy);
        aco_instance_destroy(instance);
    }
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        TEST_CASE(test_nearest_neighbour_follows_the_closest_city),
        TEST_CASE(test_nearest_neighbour_breaks_ties_by_index),
        TEST_CASE(test_nearest_neighbour_checks_its_arguments),
        TEST_CASE(test_two_opt_removes_a_crossing),
        TEST_CASE(test_two_opt_reaches_a_local_optimum),
        TEST_CASE(test_two_opt_solves_convex_instances),
        TEST_CASE(test_two_opt_handles_tiny_and_mismatched_tours),
        TEST_CASE(test_exact_matches_brute_force),
        TEST_CASE(test_exact_solves_the_course_instances),
        TEST_CASE(test_exact_finds_the_convex_hull),
        TEST_CASE(test_exact_refuses_large_instances),
        TEST_CASE(test_heuristics_are_ordered_as_expected),
    };

    return test_main(argc, argv, cases, sizeof cases / sizeof cases[0]);
}
