/**
 * @file aco.h
 * @brief Umbrella header: includes the whole public API.
 *
 * @mainpage aco-tsp
 *
 * A small C11 library and command-line tool that solve the symmetric
 * travelling salesman problem with ant colony optimisation.
 *
 * | Header                | Purpose                                          |
 * |-----------------------|--------------------------------------------------|
 * | @ref status.h         | Status codes and error messages                  |
 * | @ref rng.h            | Reproducible pseudo-random numbers               |
 * | @ref instance.h       | Problem instances: creation, parsing, writing    |
 * | @ref tour.h           | Tours and their length                           |
 * | @ref heuristics.h     | Nearest neighbour, 2-opt, exact Held-Karp solver |
 * | @ref colony.h         | The ant colony optimiser                         |
 *
 * Minimal use:
 *
 * @code{.c}
 * aco_instance *instance = NULL;
 * aco_tour tour = ACO_TOUR_INIT;
 * aco_error error = ACO_ERROR_INIT;
 *
 * if (aco_instance_load("cities.tsp", &instance, &error) == ACO_OK &&
 *     aco_solve(instance, NULL, &tour, &error) == ACO_OK) {
 *     printf("best tour: %g\n", tour.length);
 * } else {
 *     fprintf(stderr, "error: %s\n", error.message);
 * }
 * aco_tour_free(&tour);
 * aco_instance_destroy(instance);
 * @endcode
 */
#ifndef ACO_ACO_H
#define ACO_ACO_H

#include "aco/colony.h"
#include "aco/heuristics.h"
#include "aco/instance.h"
#include "aco/rng.h"
#include "aco/status.h"
#include "aco/tour.h"
#include "aco/version.h"

#endif /* ACO_ACO_H */
