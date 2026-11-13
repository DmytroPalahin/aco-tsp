/*
 * Driving the colony step by step: build an instance in memory, compare two
 * parameter settings and watch them converge.
 *
 *     ./convergence
 */
#include <aco/aco.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* What the observer needs to know about the run it is watching. */
typedef struct progress {
    const char *label;
} progress;

/* Called after every iteration; prints a line whenever the best tour improves. */
static bool report_progress(const aco_colony *colony, const aco_iteration *iteration, void *context)
{
    const progress *watched = context;

    (void)colony;
    if (iteration->improved) {
        (void)printf("  %-12s iteration %3zu: %7.0f (iteration mean %7.0f)\n", watched->label,
                     iteration->index, iteration->global_best, iteration->mean);
    }
    return true; /* keep going */
}

/* Runs one colony to completion; returns false (after reporting why) on failure. */
static bool run(const aco_instance *instance, const aco_params *params, const char *label,
                double *best)
{
    aco_colony *colony = NULL;
    aco_error error = ACO_ERROR_INIT;
    progress watched = {label};

    if (aco_colony_create(instance, params, &colony, &error) != ACO_OK) {
        (void)fprintf(stderr, "error: %s\n", error.message);
        return false;
    }
    if (aco_colony_run(colony, report_progress, &watched) != ACO_OK) {
        (void)fprintf(stderr, "error: the colony stopped unexpectedly\n");
        aco_colony_destroy(colony);
        return false;
    }

    *best = aco_colony_best(colony)->length;
    (void)printf("  %-12s stopped after %zu iterations, best tour %.0f\n\n", label,
                 aco_colony_iterations(colony), *best);
    aco_colony_destroy(colony);
    return true;
}

int main(void)
{
    aco_instance *instance = NULL;
    aco_tour greedy = ACO_TOUR_INIT;
    aco_error error = ACO_ERROR_INIT;
    aco_params params = aco_params_default();
    double plain = 0.0;
    double hybrid = 0.0;
    int exit_status = EXIT_FAILURE;

    /* 75 cities on a 1000 x 1000 grid; the seed makes the instance reproducible. */
    if (aco_instance_create_random(75, 1000, 2023, &instance, &error) != ACO_OK ||
        aco_nearest_neighbour(instance, 0, &greedy) != ACO_OK) {
        (void)fprintf(stderr, "error: %s\n", error.message);
        return EXIT_FAILURE;
    }
    (void)printf("%zu cities, nearest-neighbour tour: %.0f\n\n", aco_instance_size(instance),
                 greedy.length);

    params.seed = 1;
    params.iterations = 150;
    params.patience = 50; /* give up after 50 iterations without progress */
    if (run(instance, &params, "ant system", &plain)) {
        params.local_search = ACO_LOCAL_SEARCH_TWO_OPT;
        if (run(instance, &params, "with 2-opt", &hybrid)) {
            (void)printf("2-opt shortened the tour by %.1f %%\n", 100.0 * (plain - hybrid) / plain);
            exit_status = EXIT_SUCCESS;
        }
    }

    aco_tour_free(&greedy);
    aco_instance_destroy(instance);
    return exit_status;
}
