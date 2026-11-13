/**
 * @file colony.h
 * @brief The ant colony itself: parameters, simulation and results.
 *
 * The solver implements Ant System (Dorigo, Maniezzo and Colorni, 1996) and
 * three classic, individually switchable refinements:
 *
 * - an *elitist* reinforcement of the best tour found so far;
 * - the *pseudo-random proportional* rule of Ant Colony System, which lets an
 *   ant take the most attractive edge outright with probability `q0`;
 * - a *2-opt local search* applied to every tour before pheromone is laid.
 *
 * With the default parameters none of them is active and the algorithm is
 * plain Ant System.
 *
 * A colony is deterministic: the same instance, parameters and seed give the
 * same sequence of iterations on a given platform.
 */
#ifndef ACO_COLONY_H
#define ACO_COLONY_H

#include "aco/instance.h"
#include "aco/status.h"
#include "aco/tour.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Tour improvement applied to each ant after construction. */
typedef enum aco_local_search {
    ACO_LOCAL_SEARCH_NONE = 0, /**< Keep the tours exactly as the ants built them. */
    ACO_LOCAL_SEARCH_TWO_OPT   /**< Run 2-opt to a local optimum on every tour. */
} aco_local_search;

/**
 * Tunable parameters. Obtain sensible values from aco_params_default() and
 * override the fields of interest.
 */
typedef struct aco_params {
    /** Number of ants; `0` means one ant per city. */
    size_t ants;
    /** Maximum number of iterations; must be positive. */
    size_t iterations;
    /** Exponent of the pheromone trail in the transition rule; >= 0. */
    double alpha;
    /** Exponent of the visibility (inverse distance) in the transition rule; >= 0. */
    double beta;
    /** Fraction of pheromone that evaporates each iteration; in [0, 1]. */
    double rho;
    /** Pheromone budget of one ant: a tour of length L receives q / L per edge; > 0. */
    double q;
    /** Initial pheromone per edge; `0` selects ants * q / L_nn automatically. */
    double tau0;
    /** Probability of taking the most attractive edge greedily; in [0, 1]. */
    double q0;
    /** Extra deposits on the best-so-far tour, in units of one ant; >= 0. */
    double elitist_weight;
    /** Local search applied to every constructed tour. */
    aco_local_search local_search;
    /** Stop after this many iterations without improvement; `0` disables it. */
    size_t patience;
    /** Seed of the pseudo-random generator. */
    uint64_t seed;
} aco_params;

/** Returns the default parameters (plain Ant System). */
aco_params aco_params_default(void);

/** Checks every field of @p params against its documented range. */
aco_status aco_params_validate(const aco_params *params, aco_error *error);

/** Statistics of one iteration, as handed to an ::aco_observer. */
typedef struct aco_iteration {
    size_t index;       /**< Iteration number, starting at one. */
    double best;        /**< Shortest tour built during this iteration. */
    double mean;        /**< Mean length of the tours built during this iteration. */
    double worst;       /**< Longest tour built during this iteration. */
    double global_best; /**< Shortest tour found since the colony was created. */
    bool improved;      /**< Whether this iteration improved `global_best`. */
} aco_iteration;

/** Opaque colony handle. */
typedef struct aco_colony aco_colony;

/**
 * Callback invoked by aco_colony_run() after every iteration.
 *
 * @return `true` to continue, `false` to stop the run early.
 */
typedef bool (*aco_observer)(const aco_colony *colony, const aco_iteration *iteration,
                             void *context);

/**
 * Creates a colony for @p instance.
 *
 * The colony keeps a pointer to @p instance, which must therefore outlive it.
 *
 * @param params Parameters to use; `NULL` selects aco_params_default().
 * @param[out] out Receives the colony; release it with aco_colony_destroy().
 */
aco_status aco_colony_create(const aco_instance *instance, const aco_params *params,
                             aco_colony **out, aco_error *error);

/** Releases a colony. `NULL` is accepted and ignored. */
void aco_colony_destroy(aco_colony *colony);

/**
 * Performs exactly one iteration: every ant builds a tour, then pheromone
 * evaporates and is reinforced.
 *
 * @param[out] iteration If not `NULL`, receives the statistics of the iteration.
 */
aco_status aco_colony_step(aco_colony *colony, aco_iteration *iteration);

/**
 * Iterates until the iteration budget is spent, `patience` iterations pass
 * without improvement, or @p observer asks to stop.
 *
 * @param observer Optional per-iteration callback; may be `NULL`.
 * @param context  Passed through to @p observer untouched.
 */
aco_status aco_colony_run(aco_colony *colony, aco_observer observer, void *context);

/**
 * Best tour found so far. Before the first iteration its length is infinite.
 * The pointer stays valid until the colony is destroyed.
 */
const aco_tour *aco_colony_best(const aco_colony *colony);

/** Number of iterations performed so far. */
size_t aco_colony_iterations(const aco_colony *colony);

/** Iteration during which the current best tour was found (0 if none yet). */
size_t aco_colony_best_iteration(const aco_colony *colony);

/** Pheromone currently laid on the edge between two cities. */
double aco_colony_pheromone(const aco_colony *colony, size_t from, size_t to);

/** Parameters in effect, with the automatic values (`ants`, `tau0`) resolved. */
const aco_params *aco_colony_params(const aco_colony *colony);

/**
 * Convenience wrapper: creates a colony, runs it to completion and copies
 * the best tour into @p tour.
 *
 * @param[in,out] tour Initialised tour; resized to the instance size.
 */
aco_status aco_solve(const aco_instance *instance, const aco_params *params, aco_tour *tour,
                     aco_error *error);

#ifdef __cplusplus
}
#endif

#endif /* ACO_COLONY_H */
