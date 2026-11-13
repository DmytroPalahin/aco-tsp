#include "aco/aco.h"

#include "fixtures.h"
#include "test.h"

#include <stdbool.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Parameters                                                               */
/* ------------------------------------------------------------------------ */

static void test_default_parameters_are_valid(void)
{
    const aco_params params = aco_params_default();

    EXPECT_STATUS(aco_params_validate(&params, NULL), ACO_OK);
    EXPECT_EQ_SIZE(params.ants, 0); /* one per city */
    EXPECT(params.iterations > 0);
    EXPECT(params.local_search == ACO_LOCAL_SEARCH_NONE);
    EXPECT_NEAR(params.q0, 0.0, 0.0); /* plain Ant System */
    EXPECT_NEAR(params.elitist_weight, 0.0, 0.0);
}

static void test_out_of_range_parameters_are_rejected(void)
{
    const double not_a_number = nan("");
    aco_params cases[16];
    size_t count = 0;

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        cases[i] = aco_params_default();
    }
    cases[count++].iterations = 0;
    cases[count++].alpha = -0.5;
    cases[count++].alpha = not_a_number;
    cases[count++].beta = -1.0;
    cases[count++].beta = HUGE_VAL;
    cases[count++].rho = -0.1;
    cases[count++].rho = 1.5;
    cases[count++].rho = not_a_number;
    cases[count++].q = 0.0;
    cases[count++].q = -2.0;
    cases[count++].tau0 = -1.0;
    cases[count++].q0 = 1.01;
    cases[count++].q0 = -0.01;
    cases[count++].elitist_weight = -1.0;
    cases[count++].local_search = (aco_local_search)42;
    cases[count++].ants = (size_t)-1;

    for (size_t i = 0; i < count; ++i) {
        aco_error error = ACO_ERROR_INIT;

        EXPECT_STATUS(aco_params_validate(&cases[i], &error), ACO_ERR_INVALID_ARGUMENT);
        EXPECT(error.message[0] != '\0');
        if (error.status != ACO_ERR_INVALID_ARGUMENT) {
            (void)fprintf(stderr, "    case #%zu was accepted\n", i);
        }
    }
    EXPECT_STATUS(aco_params_validate(NULL, NULL), ACO_ERR_INVALID_ARGUMENT);
}

static void test_boundary_parameters_are_accepted(void)
{
    aco_params params = aco_params_default();

    params.alpha = 0.0;
    params.beta = 0.0;
    params.rho = 0.0;
    params.q0 = 1.0;
    EXPECT_STATUS(aco_params_validate(&params, NULL), ACO_OK);
    params.rho = 1.0;
    params.q0 = 0.0;
    EXPECT_STATUS(aco_params_validate(&params, NULL), ACO_OK);
}

/* ------------------------------------------------------------------------ */
/* Life cycle                                                               */
/* ------------------------------------------------------------------------ */

static void test_create_resolves_automatic_values(void)
{
    aco_instance *instance = NULL;
    aco_colony *colony = NULL;
    aco_tour greedy = ACO_TOUR_INIT;
    const aco_params *params;

    REQUIRE_OK(aco_instance_create_random(20, 100, 1, &instance, NULL));
    REQUIRE_OK(aco_colony_create(instance, NULL, &colony, NULL));
    REQUIRE_OK(aco_nearest_neighbour(instance, 0, &greedy));

    params = aco_colony_params(colony);
    EXPECT_EQ_SIZE(params->ants, 20);
    EXPECT_NEAR(params->tau0, 20.0 / greedy.length, 1e-12);
    EXPECT_NEAR(aco_colony_pheromone(colony, 3, 7), params->tau0, 0.0);

    EXPECT_EQ_SIZE(aco_colony_iterations(colony), 0);
    EXPECT_EQ_SIZE(aco_colony_best_iteration(colony), 0);
    EXPECT(isinf(aco_colony_best(colony)->length));

    aco_tour_free(&greedy);
    aco_colony_destroy(colony);
    aco_instance_destroy(instance);
}

static void test_create_checks_its_arguments(void)
{
    aco_instance *instance = NULL;
    aco_colony *colony = NULL;
    aco_params params = aco_params_default();
    aco_error error = ACO_ERROR_INIT;

    REQUIRE_OK(aco_instance_create_random(5, 100, 1, &instance, NULL));
    params.rho = 2.0;

    EXPECT_STATUS(aco_colony_create(instance, &params, &colony, &error), ACO_ERR_INVALID_ARGUMENT);
    EXPECT(strstr(error.message, "rho") != NULL);
    EXPECT_STATUS(aco_colony_create(NULL, NULL, &colony, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_colony_create(instance, NULL, NULL, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT(colony == NULL);

    EXPECT_STATUS(aco_colony_step(NULL, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_colony_run(NULL, NULL, NULL), ACO_ERR_INVALID_ARGUMENT);
    aco_colony_destroy(NULL);
    aco_instance_destroy(instance);
}

/* ------------------------------------------------------------------------ */
/* One iteration                                                            */
/* ------------------------------------------------------------------------ */

static void test_step_reports_consistent_statistics(void)
{
    aco_instance *instance = NULL;
    aco_colony *colony = NULL;
    double previous_best = HUGE_VAL;

    REQUIRE_OK(aco_instance_create_random(30, 1000, 2, &instance, NULL));
    REQUIRE_OK(aco_colony_create(instance, NULL, &colony, NULL));

    for (size_t step = 1; step <= 25; ++step) {
        aco_iteration iteration;
        const aco_tour *best;

        REQUIRE_OK(aco_colony_step(colony, &iteration));
        best = aco_colony_best(colony);

        EXPECT_EQ_SIZE(iteration.index, step);
        EXPECT_EQ_SIZE(aco_colony_iterations(colony), step);
        EXPECT(iteration.best <= iteration.mean && iteration.mean <= iteration.worst);
        EXPECT(iteration.global_best <= iteration.best);
        EXPECT(iteration.global_best <= previous_best); /* never gets worse */
        EXPECT(iteration.improved == (iteration.global_best < previous_best));
        EXPECT(iteration.improved == (aco_colony_best_iteration(colony) == step));

        EXPECT(aco_tour_is_valid(instance, best));
        EXPECT_NEAR(best->length, iteration.global_best, 0.0);
        EXPECT_NEAR(best->length, aco_tour_measure(instance, best), 1e-9);
        previous_best = iteration.global_best;
    }

    REQUIRE_OK(aco_colony_step(colony, NULL)); /* statistics are optional */
    aco_colony_destroy(colony);
    aco_instance_destroy(instance);
}

/*
 * On three cities every tour is the same triangle, so one iteration can be
 * checked by hand: tau <- (1 - rho) * tau0 + ants * q / L (+ e * q / L).
 */
static void test_pheromone_update_matches_the_formula(void)
{
    static const double triangle[9] = {0, 3, 4, 3, 0, 5, 4, 5, 0};
    const double length = 12.0;
    aco_instance *instance = NULL;
    aco_colony *colony = NULL;
    aco_params params = aco_params_default();

    REQUIRE_OK(aco_instance_create_from_matrix(triangle, 3, &instance, NULL));
    params.tau0 = 1.0;
    params.rho = 0.25;
    params.q = 6.0;

    REQUIRE_OK(aco_colony_create(instance, &params, &colony, NULL));
    REQUIRE_OK(aco_colony_step(colony, NULL));
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            if (i != j) {
                EXPECT_NEAR(aco_colony_pheromone(colony, i, j), 0.75 + 3.0 * 6.0 / length, 1e-12);
            }
        }
    }
    aco_colony_destroy(colony);

    colony = NULL;
    params.elitist_weight = 2.0;
    REQUIRE_OK(aco_colony_create(instance, &params, &colony, NULL));
    REQUIRE_OK(aco_colony_step(colony, NULL));
    EXPECT_NEAR(aco_colony_pheromone(colony, 0, 1), 0.75 + (3.0 + 2.0) * 6.0 / length, 1e-12);
    EXPECT_NEAR(aco_colony_pheromone(colony, 2, 1), 0.75 + (3.0 + 2.0) * 6.0 / length, 1e-12);

    aco_colony_destroy(colony);
    aco_instance_destroy(instance);
}

static void test_pheromone_stays_symmetric_and_positive(void)
{
    aco_instance *instance = NULL;
    aco_colony *colony = NULL;
    aco_params params = aco_params_default();

    REQUIRE_OK(aco_instance_create_random(25, 500, 8, &instance, NULL));
    params.iterations = 60;
    params.elitist_weight = 3.0;
    REQUIRE_OK(aco_colony_create(instance, &params, &colony, NULL));
    REQUIRE_OK(aco_colony_run(colony, NULL, NULL));

    for (size_t i = 0; i < 25; ++i) {
        for (size_t j = i + 1; j < 25; ++j) {
            const double forward = aco_colony_pheromone(colony, i, j);

            EXPECT(forward > 0.0 && isfinite(forward));
            EXPECT_NEAR(forward, aco_colony_pheromone(colony, j, i), 0.0);
        }
    }
    aco_colony_destroy(colony);
    aco_instance_destroy(instance);
}

/* ------------------------------------------------------------------------ */
/* Determinism                                                              */
/* ------------------------------------------------------------------------ */

static void test_same_seed_gives_the_same_run(void)
{
    aco_instance *instance = NULL;
    aco_colony *first = NULL;
    aco_colony *second = NULL;
    aco_params params = aco_params_default();

    REQUIRE_OK(aco_instance_create_random(30, 1000, 4, &instance, NULL));
    params.seed = 12345;
    REQUIRE_OK(aco_colony_create(instance, &params, &first, NULL));
    REQUIRE_OK(aco_colony_create(instance, &params, &second, NULL));

    for (int step = 0; step < 30; ++step) {
        aco_iteration a;
        aco_iteration b;

        REQUIRE_OK(aco_colony_step(first, &a));
        REQUIRE_OK(aco_colony_step(second, &b));
        EXPECT_NEAR(a.best, b.best, 0.0);
        EXPECT_NEAR(a.mean, b.mean, 0.0);
        EXPECT_NEAR(a.worst, b.worst, 0.0);
    }
    EXPECT(memcmp(aco_colony_best(first)->cities, aco_colony_best(second)->cities,
                  30 * sizeof(size_t)) == 0);

    aco_colony_destroy(first);
    aco_colony_destroy(second);
    aco_instance_destroy(instance);
}

static void test_different_seeds_give_different_runs(void)
{
    aco_instance *instance = NULL;
    aco_colony *first = NULL;
    aco_colony *second = NULL;
    aco_params params = aco_params_default();
    bool differ = false;

    REQUIRE_OK(aco_instance_create_random(30, 1000, 4, &instance, NULL));
    params.seed = 1;
    REQUIRE_OK(aco_colony_create(instance, &params, &first, NULL));
    params.seed = 2;
    REQUIRE_OK(aco_colony_create(instance, &params, &second, NULL));

    for (int step = 0; step < 10; ++step) {
        aco_iteration a;
        aco_iteration b;

        REQUIRE_OK(aco_colony_step(first, &a));
        REQUIRE_OK(aco_colony_step(second, &b));
        differ = differ || a.mean != b.mean;
    }
    EXPECT(differ);

    aco_colony_destroy(first);
    aco_colony_destroy(second);
    aco_instance_destroy(instance);
}

/* With q0 = 1 and one ant per city nothing is left to chance. */
static void test_pure_exploitation_ignores_the_seed(void)
{
    aco_instance *instance = NULL;
    aco_params params = aco_params_default();
    aco_tour first = ACO_TOUR_INIT;
    aco_tour second = ACO_TOUR_INIT;

    REQUIRE_OK(aco_instance_create_random(20, 1000, 6, &instance, NULL));
    params.q0 = 1.0;
    params.iterations = 15;
    params.seed = 1;
    REQUIRE_OK(aco_solve(instance, &params, &first, NULL));
    params.seed = 987654321;
    REQUIRE_OK(aco_solve(instance, &params, &second, NULL));

    EXPECT(aco_tour_is_valid(instance, &first));
    EXPECT_NEAR(first.length, second.length, 0.0);
    EXPECT(memcmp(first.cities, second.cities, 20 * sizeof(size_t)) == 0);

    aco_tour_free(&first);
    aco_tour_free(&second);
    aco_instance_destroy(instance);
}

/* ------------------------------------------------------------------------ */
/* Solution quality                                                         */
/* ------------------------------------------------------------------------ */

/*
 * Small instances, where the optimum can be proven. The search is stochastic,
 * so the bounds leave a wide margin over what is observed: across 150 seeds,
 * plain Ant System never ended more than 2 % above the optimum on 8 cities,
 * and the hybrid with 2-opt found the optimum every single time.
 */
static void expect_near_optimal(size_t cities, aco_local_search local_search, double tolerance)
{
    for (uint64_t seed = 1; seed <= 8; ++seed) {
        aco_instance *instance = NULL;
        aco_tour optimal = ACO_TOUR_INIT;
        aco_tour found = ACO_TOUR_INIT;
        aco_params params = aco_params_default();

        REQUIRE_OK(aco_instance_create_random(cities, 100, seed, &instance, NULL));
        REQUIRE_OK(aco_exact(instance, &optimal, NULL));
        params.seed = seed;
        params.local_search = local_search;
        REQUIRE_OK(aco_solve(instance, &params, &found, NULL));

        EXPECT(aco_tour_is_valid(instance, &found));
        EXPECT(found.length >= optimal.length - 1e-9); /* nothing beats the optimum */
        EXPECT(found.length <= optimal.length * (1.0 + tolerance));

        aco_tour_free(&optimal);
        aco_tour_free(&found);
        aco_instance_destroy(instance);
    }
}

static void test_ant_system_is_near_optimal_on_small_instances(void)
{
    expect_near_optimal(8, ACO_LOCAL_SEARCH_NONE, 0.10);
}

static void test_hybrid_is_near_optimal_on_small_instances(void)
{
    expect_near_optimal(12, ACO_LOCAL_SEARCH_TWO_OPT, 0.02);
}

/* Medium instances: the search must clearly beat the tours it starts from. */
static void test_improves_on_its_first_iteration(void)
{
    aco_instance *instance = NULL;
    aco_colony *colony = NULL;
    aco_params params = aco_params_default();
    aco_iteration first;

    REQUIRE_OK(aco_instance_create_random(40, 1000, 21, &instance, NULL));
    params.iterations = 100;
    REQUIRE_OK(aco_colony_create(instance, &params, &colony, NULL));
    REQUIRE_OK(aco_colony_step(colony, &first));
    REQUIRE_OK(aco_colony_run(colony, NULL, NULL));

    EXPECT_EQ_SIZE(aco_colony_iterations(colony), 100);
    EXPECT(aco_colony_best(colony)->length < first.mean);
    EXPECT(aco_colony_best(colony)->length <= first.best);

    aco_colony_destroy(colony);
    aco_instance_destroy(instance);
}

/* 2-opt alone solves convex instances, so the hybrid must too, from iteration one. */
static void test_local_search_solves_convex_instances(void)
{
    double optimum = 0.0;
    aco_instance *instance = fixture_circle(30, 100.0, &optimum);
    aco_params params = aco_params_default();
    aco_tour tour = ACO_TOUR_INIT;

    REQUIRE(instance != NULL);
    params.local_search = ACO_LOCAL_SEARCH_TWO_OPT;
    params.iterations = 3;
    REQUIRE_OK(aco_solve(instance, &params, &tour, NULL));

    EXPECT(aco_tour_is_valid(instance, &tour));
    EXPECT_NEAR(tour.length, optimum, 1e-9);

    aco_tour_free(&tour);
    aco_instance_destroy(instance);
}

/* ------------------------------------------------------------------------ */
/* Stopping rules                                                           */
/* ------------------------------------------------------------------------ */

static void test_run_spends_the_iteration_budget(void)
{
    aco_instance *instance = NULL;
    aco_colony *colony = NULL;
    aco_params params = aco_params_default();

    REQUIRE_OK(aco_instance_create_random(12, 100, 3, &instance, NULL));
    params.iterations = 17;
    REQUIRE_OK(aco_colony_create(instance, &params, &colony, NULL));

    REQUIRE_OK(aco_colony_run(colony, NULL, NULL));
    EXPECT_EQ_SIZE(aco_colony_iterations(colony), 17);
    REQUIRE_OK(aco_colony_run(colony, NULL, NULL)); /* nothing left to do */
    EXPECT_EQ_SIZE(aco_colony_iterations(colony), 17);

    aco_colony_destroy(colony);
    aco_instance_destroy(instance);
}

static void test_patience_stops_a_stagnating_run(void)
{
    static const double triangle[9] = {0, 3, 4, 3, 0, 5, 4, 5, 0};
    aco_instance *instance = NULL;
    aco_colony *colony = NULL;
    aco_params params = aco_params_default();

    /* A triangle has a single tour: the first iteration finds it, nothing improves it. */
    REQUIRE_OK(aco_instance_create_from_matrix(triangle, 3, &instance, NULL));
    params.iterations = 1000;
    params.patience = 5;
    REQUIRE_OK(aco_colony_create(instance, &params, &colony, NULL));
    REQUIRE_OK(aco_colony_run(colony, NULL, NULL));

    EXPECT_EQ_SIZE(aco_colony_best_iteration(colony), 1);
    EXPECT_EQ_SIZE(aco_colony_iterations(colony), 6);

    aco_colony_destroy(colony);
    aco_instance_destroy(instance);
}

typedef struct observation {
    size_t calls;
    size_t stop_after;
    size_t last_index;
} observation;

static bool count_and_stop(const aco_colony *colony, const aco_iteration *iteration, void *context)
{
    observation *seen = context;

    ++seen->calls;
    seen->last_index = iteration->index;
    EXPECT_EQ_SIZE(aco_colony_iterations(colony), iteration->index);
    return seen->calls < seen->stop_after;
}

static void test_observer_can_stop_the_run(void)
{
    aco_instance *instance = NULL;
    aco_colony *colony = NULL;
    observation seen = {0, 4, 0};

    REQUIRE_OK(aco_instance_create_random(12, 100, 3, &instance, NULL));
    REQUIRE_OK(aco_colony_create(instance, NULL, &colony, NULL));
    REQUIRE_OK(aco_colony_run(colony, count_and_stop, &seen));

    EXPECT_EQ_SIZE(seen.calls, 4);
    EXPECT_EQ_SIZE(seen.last_index, 4);
    EXPECT_EQ_SIZE(aco_colony_iterations(colony), 4);

    aco_colony_destroy(colony);
    aco_instance_destroy(instance);
}

/* ------------------------------------------------------------------------ */
/* Robustness                                                               */
/* ------------------------------------------------------------------------ */

/* Runs a short search and checks that it produced a real tour. */
static void expect_a_valid_run(const aco_instance *instance, const aco_params *params)
{
    aco_tour tour = ACO_TOUR_INIT;
    aco_error error = ACO_ERROR_INIT;
    const aco_status status = aco_solve(instance, params, &tour, &error);

    EXPECT(status == ACO_OK);
    if (status != ACO_OK) {
        (void)fprintf(stderr, "    %s\n", error.message);
    } else {
        EXPECT(aco_tour_is_valid(instance, &tour));
        EXPECT(isfinite(tour.length) && tour.length >= 0.0);
        EXPECT_NEAR(tour.length, aco_tour_measure(instance, &tour), 1e-9);
    }
    aco_tour_free(&tour);
}

static void test_any_number_of_ants_works(void)
{
    aco_instance *instance = NULL;
    aco_params params = aco_params_default();

    REQUIRE_OK(aco_instance_create_random(15, 100, 9, &instance, NULL));
    params.iterations = 10;
    params.ants = 1;
    expect_a_valid_run(instance, &params);
    params.ants = 7;
    expect_a_valid_run(instance, &params);
    params.ants = 64;
    expect_a_valid_run(instance, &params);
    aco_instance_destroy(instance);
}

static void test_extreme_parameters_do_not_break_the_search(void)
{
    aco_instance *instance = NULL;
    aco_params params;

    REQUIRE_OK(aco_instance_create_random(20, 1000, 10, &instance, NULL));

    params = aco_params_default();
    params.iterations = 20;
    params.alpha = 0.0; /* pheromone ignored */
    params.beta = 0.0;  /* distance ignored: a random search */
    expect_a_valid_run(instance, &params);

    params.alpha = 5.0;
    params.beta = 80.0; /* far cities underflow to weight zero */
    expect_a_valid_run(instance, &params);

    params = aco_params_default();
    params.iterations = 20;
    params.rho = 0.0; /* pheromone never evaporates */
    expect_a_valid_run(instance, &params);
    params.rho = 1.0; /* pheromone never persists */
    expect_a_valid_run(instance, &params);

    params = aco_params_default();
    params.iterations = 20;
    params.q0 = 0.9;
    params.elitist_weight = 20.0;
    params.local_search = ACO_LOCAL_SEARCH_TWO_OPT;
    expect_a_valid_run(instance, &params);

    params = aco_params_default();
    params.iterations = 400;
    params.alpha = 8.0; /* trails on unused edges decay towards zero */
    params.rho = 0.9;
    expect_a_valid_run(instance, &params);

    aco_instance_destroy(instance);
}

static void test_degenerate_instances_are_handled(void)
{
    static const double all_zero[16] = {0};
    static const aco_point with_duplicates[6] = {{0, 0}, {5, 5}, {0, 0}, {5, 5}, {9, 1}, {0, 0}};
    static const double two_cities[4] = {0, 7, 7, 0};
    aco_instance *instance = NULL;
    aco_params params = aco_params_default();
    aco_tour tour = ACO_TOUR_INIT;

    params.iterations = 10;

    REQUIRE_OK(aco_instance_create_from_matrix(all_zero, 4, &instance, NULL));
    expect_a_valid_run(instance, &params);
    aco_instance_destroy(instance);

    instance = NULL;
    REQUIRE_OK(
        aco_instance_create_from_points(with_duplicates, 6, ACO_METRIC_EUCLIDEAN, &instance, NULL));
    expect_a_valid_run(instance, &params);
    params.local_search = ACO_LOCAL_SEARCH_TWO_OPT;
    expect_a_valid_run(instance, &params);
    aco_instance_destroy(instance);

    instance = NULL;
    REQUIRE_OK(aco_instance_create_from_matrix(two_cities, 2, &instance, NULL));
    REQUIRE_OK(aco_solve(instance, &params, &tour, NULL));
    EXPECT_NEAR(tour.length, 14.0, 0.0); /* there and back */
    aco_instance_destroy(instance);
    aco_tour_free(&tour);
}

static void test_solve_checks_its_arguments(void)
{
    aco_instance *instance = NULL;
    aco_tour tour = ACO_TOUR_INIT;
    aco_params params = aco_params_default();
    aco_error error = ACO_ERROR_INIT;

    REQUIRE_OK(aco_instance_create_random(5, 100, 1, &instance, NULL));
    params.q = -1.0;

    EXPECT_STATUS(aco_solve(instance, NULL, NULL, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_solve(NULL, NULL, &tour, NULL), ACO_ERR_INVALID_ARGUMENT);
    EXPECT_STATUS(aco_solve(instance, &params, &tour, &error), ACO_ERR_INVALID_ARGUMENT);
    EXPECT(error.status == ACO_ERR_INVALID_ARGUMENT);
    EXPECT(tour.cities == NULL);

    aco_instance_destroy(instance);
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        TEST_CASE(test_default_parameters_are_valid),
        TEST_CASE(test_out_of_range_parameters_are_rejected),
        TEST_CASE(test_boundary_parameters_are_accepted),
        TEST_CASE(test_create_resolves_automatic_values),
        TEST_CASE(test_create_checks_its_arguments),
        TEST_CASE(test_step_reports_consistent_statistics),
        TEST_CASE(test_pheromone_update_matches_the_formula),
        TEST_CASE(test_pheromone_stays_symmetric_and_positive),
        TEST_CASE(test_same_seed_gives_the_same_run),
        TEST_CASE(test_different_seeds_give_different_runs),
        TEST_CASE(test_pure_exploitation_ignores_the_seed),
        TEST_CASE(test_ant_system_is_near_optimal_on_small_instances),
        TEST_CASE(test_hybrid_is_near_optimal_on_small_instances),
        TEST_CASE(test_improves_on_its_first_iteration),
        TEST_CASE(test_local_search_solves_convex_instances),
        TEST_CASE(test_run_spends_the_iteration_budget),
        TEST_CASE(test_patience_stops_a_stagnating_run),
        TEST_CASE(test_observer_can_stop_the_run),
        TEST_CASE(test_any_number_of_ants_works),
        TEST_CASE(test_extreme_parameters_do_not_break_the_search),
        TEST_CASE(test_degenerate_instances_are_handled),
        TEST_CASE(test_solve_checks_its_arguments),
    };

    return test_main(argc, argv, cases, sizeof cases / sizeof cases[0]);
}
