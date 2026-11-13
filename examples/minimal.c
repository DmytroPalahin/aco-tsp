/*
 * The smallest useful program: load an instance, solve it, print the tour.
 *
 *     ./minimal data/course/tsp050.txt
 */
#include <aco/aco.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    aco_instance *instance = NULL;
    aco_tour tour = ACO_TOUR_INIT;
    aco_error error = ACO_ERROR_INIT;
    int exit_status = EXIT_FAILURE;

    if (argc != 2) {
        (void)fprintf(stderr, "usage: %s <instance>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Passing NULL for the parameters selects the defaults: plain Ant System. */
    if (aco_instance_load(argv[1], &instance, &error) == ACO_OK &&
        aco_solve(instance, NULL, &tour, &error) == ACO_OK) {
        (void)printf("%s: tour of length %g\n", aco_instance_name(instance), tour.length);
        for (size_t i = 0; i < tour.size; ++i) {
            (void)printf("%zu ", tour.cities[i] + 1);
        }
        (void)printf("\n");
        exit_status = EXIT_SUCCESS;
    } else {
        (void)fprintf(stderr, "error: %s\n", error.message);
    }

    aco_tour_free(&tour);
    aco_instance_destroy(instance);
    return exit_status;
}
