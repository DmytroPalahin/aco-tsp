/*
 * Ant System with optional elitism, greedy exploitation and 2-opt.
 *
 * One iteration:
 *
 *   1. every ant builds a tour, choosing the next city j from city i with
 *      probability proportional to tau(i, j)^alpha * eta(i, j)^beta, where
 *      tau is the pheromone on the edge and eta = 1 / d(i, j) its visibility;
 *   2. pheromone evaporates:     tau <- (1 - rho) * tau;
 *   3. every ant reinforces the edges of its tour by q / L, L being the
 *      length of that tour.
 *
 * The cost of an iteration is O(ants * n^2); with one ant per city, O(n^3).
 */
#include "aco/colony.h"
#include "aco/heuristics.h"
#include "aco/rng.h"

#include "internal.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Upper bound on the colony size, to keep the tour storage addressable. */
#define MAX_ANTS ((size_t)1000000)

struct aco_colony {
    const aco_instance *instance; /* borrowed: must outlive the colony */
    aco_params params;            /* with `ants` and `tau0` resolved */
    aco_rng rng;
    size_t size; /* number of cities */

    double *pheromone;  /* tau(i, j), size^2 entries */
    double *visibility; /* eta(i, j)^beta, size^2 entries, constant */
    double *attraction; /* tau^alpha * eta^beta, refreshed once per iteration */

    size_t *tours;      /* `ants` rows of `size` cities */
    double *lengths;    /* length of each ant's tour */
    size_t *candidates; /* scratch: the cities the current ant may still visit */

    aco_tour best;
    size_t iteration;      /* iterations performed so far */
    size_t best_iteration; /* iteration that produced `best` */
};

/* ------------------------------------------------------------------------ */
/* Parameters                                                               */
/* ------------------------------------------------------------------------ */

aco_params aco_params_default(void)
{
    const aco_params params = {
        .ants = 0,
        .iterations = 200,
        .alpha = 1.0,
        .beta = 3.0,
        .rho = 0.5,
        .q = 1.0,
        .tau0 = 0.0,
        .q0 = 0.0,
        .elitist_weight = 0.0,
        .local_search = ACO_LOCAL_SEARCH_NONE,
        .patience = 0,
        .seed = 1,
    };

    return params;
}

/* Both helpers are written so that a NaN fails the test. */
static bool within(double value, double low, double high)
{
    return value >= low && value <= high;
}

static bool finite_and_at_least(double value, double low)
{
    return value >= low && isfinite(value);
}

aco_status aco_params_validate(const aco_params *params, aco_error *error)
{
    if (params == NULL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "params must not be NULL");
    }
    if (params->iterations == 0) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "iterations must be positive");
    }
    if (params->ants > MAX_ANTS) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "ants must not exceed %zu (got %zu)",
                        MAX_ANTS, params->ants);
    }
    if (!finite_and_at_least(params->alpha, 0.0)) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT,
                        "alpha must be finite and non-negative (got %g)", params->alpha);
    }
    if (!finite_and_at_least(params->beta, 0.0)) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT,
                        "beta must be finite and non-negative (got %g)", params->beta);
    }
    if (!within(params->rho, 0.0, 1.0)) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "rho must lie in [0, 1] (got %g)",
                        params->rho);
    }
    if (!(params->q > 0.0) || !isfinite(params->q)) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "q must be finite and positive (got %g)",
                        params->q);
    }
    if (!finite_and_at_least(params->tau0, 0.0)) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT,
                        "tau0 must be finite and non-negative (got %g)", params->tau0);
    }
    if (!within(params->q0, 0.0, 1.0)) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "q0 must lie in [0, 1] (got %g)",
                        params->q0);
    }
    if (!finite_and_at_least(params->elitist_weight, 0.0)) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT,
                        "elitist_weight must be finite and non-negative (got %g)",
                        params->elitist_weight);
    }
    if (params->local_search != ACO_LOCAL_SEARCH_NONE &&
        params->local_search != ACO_LOCAL_SEARCH_TWO_OPT) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "unknown local search");
    }
    return ACO_OK;
}

/* ------------------------------------------------------------------------ */
/* Construction and destruction                                             */
/* ------------------------------------------------------------------------ */

/*
 * eta^beta for every edge. Only ratios between visibilities matter to the
 * transition rule, so distances are measured in units of the shortest
 * positive one: every value then lies in [0, 1] and cannot overflow, whatever
 * beta is. Coincident cities (distance zero) get the maximum visibility.
 */
static void compute_visibility(aco_colony *colony)
{
    const size_t size = colony->size;
    const double *distances = colony->instance->distances;
    double shortest = HUGE_VAL;

    for (size_t cell = 0; cell < size * size; ++cell) {
        if (distances[cell] > 0.0 && distances[cell] < shortest) {
            shortest = distances[cell];
        }
    }
    for (size_t i = 0; i < size; ++i) {
        for (size_t j = 0; j < size; ++j) {
            const double distance = distances[i * size + j];
            double visibility = 0.0;

            if (i != j) {
                visibility = distance > 0.0 ? pow(shortest / distance, colony->params.beta) : 1.0;
            }
            colony->visibility[i * size + j] = visibility;
        }
    }
}

/*
 * Initial pheromone suggested by Dorigo and Stuetzle for Ant System: what one
 * iteration would lay on an edge if every ant followed the nearest-neighbour
 * tour. Starting close to the level the algorithm settles at avoids both a
 * long warm-up and an early lock-in on the first tours.
 */
static aco_status automatic_tau0(aco_colony *colony, double *tau0)
{
    const aco_status status = aco_nearest_neighbour(colony->instance, 0, &colony->best);

    if (status != ACO_OK) {
        return status;
    }
    *tau0 = (double)colony->params.ants * colony->params.q / colony->best.length;
    if (!(*tau0 > 0.0) || !isfinite(*tau0)) {
        *tau0 = 1.0; /* Degenerate instance: every distance is zero. */
    }
    return ACO_OK;
}

aco_status aco_colony_create(const aco_instance *instance, const aco_params *params,
                             aco_colony **out, aco_error *error)
{
    aco_colony *colony;
    aco_params resolved;
    aco_status status;
    size_t size;

    if (instance == NULL || out == NULL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "instance and out must not be NULL");
    }
    resolved = (params != NULL) ? *params : aco_params_default();
    status = aco_params_validate(&resolved, error);
    if (status != ACO_OK) {
        return status;
    }
    size = instance->size;
    if (resolved.ants == 0) {
        resolved.ants = size;
    }
    if (resolved.ants > SIZE_MAX / sizeof(size_t) / size) {
        return ACO_FAIL(error, ACO_ERR_OUT_OF_MEMORY, "%zu ants on %zu cities do not fit in memory",
                        resolved.ants, size);
    }

    colony = calloc(1, sizeof *colony);
    if (colony == NULL) {
        return ACO_FAIL(error, ACO_ERR_OUT_OF_MEMORY, "out of memory");
    }
    colony->instance = instance;
    colony->params = resolved;
    colony->size = size;
    colony->pheromone = malloc(size * size * sizeof *colony->pheromone);
    colony->visibility = malloc(size * size * sizeof *colony->visibility);
    colony->attraction = malloc(size * size * sizeof *colony->attraction);
    colony->tours = malloc(resolved.ants * size * sizeof *colony->tours);
    colony->lengths = malloc(resolved.ants * sizeof *colony->lengths);
    colony->candidates = malloc(size * sizeof *colony->candidates);
    status = aco_tour_init(&colony->best, size);

    if (status != ACO_OK || colony->pheromone == NULL || colony->visibility == NULL ||
        colony->attraction == NULL || colony->tours == NULL || colony->lengths == NULL ||
        colony->candidates == NULL) {
        aco_colony_destroy(colony);
        return ACO_FAIL(error, ACO_ERR_OUT_OF_MEMORY,
                        "out of memory while creating a colony of %zu ants on %zu cities",
                        resolved.ants, size);
    }

    if (colony->params.tau0 == 0.0) {
        status = automatic_tau0(colony, &colony->params.tau0);
        if (status != ACO_OK) {
            aco_colony_destroy(colony);
            return ACO_FAIL(error, status, "cannot initialise the pheromone trails");
        }
    }
    for (size_t cell = 0; cell < size * size; ++cell) {
        colony->pheromone[cell] = colony->params.tau0;
    }
    compute_visibility(colony);
    aco_rng_seed(&colony->rng, colony->params.seed);
    colony->best.length = HUGE_VAL;

    *out = colony;
    return ACO_OK;
}

void aco_colony_destroy(aco_colony *colony)
{
    if (colony == NULL) {
        return;
    }
    free(colony->pheromone);
    free(colony->visibility);
    free(colony->attraction);
    free(colony->tours);
    free(colony->lengths);
    free(colony->candidates);
    aco_tour_free(&colony->best);
    free(colony);
}

/* ------------------------------------------------------------------------ */
/* One iteration                                                            */
/* ------------------------------------------------------------------------ */

static void refresh_attraction(aco_colony *colony)
{
    const size_t cells = colony->size * colony->size;
    const double alpha = colony->params.alpha;

    if (alpha == 1.0) {
        for (size_t cell = 0; cell < cells; ++cell) {
            colony->attraction[cell] = colony->pheromone[cell] * colony->visibility[cell];
        }
    } else {
        for (size_t cell = 0; cell < cells; ++cell) {
            colony->attraction[cell] =
                pow(colony->pheromone[cell], alpha) * colony->visibility[cell];
        }
    }
}

/*
 * Picks the next city of an ant standing in `current`. The cities it may
 * still visit are candidates[0 .. remaining); the result indexes that array.
 */
static size_t choose_candidate(aco_colony *colony, size_t current, size_t remaining)
{
    const double *row = colony->attraction + current * colony->size;
    const size_t *candidates = colony->candidates;
    double total = 0.0;

    if (remaining == 1) {
        return 0;
    }

    /* Exploitation (Ant Colony System): take the most attractive edge outright. */
    if (colony->params.q0 > 0.0 && aco_rng_uniform(&colony->rng) < colony->params.q0) {
        size_t best = 0;

        for (size_t k = 1; k < remaining; ++k) {
            if (row[candidates[k]] > row[candidates[best]]) {
                best = k;
            }
        }
        return best;
    }

    /* Exploration: roulette wheel, each candidate owning a slice of [0, total). */
    for (size_t k = 0; k < remaining; ++k) {
        total += row[candidates[k]];
    }
    if (total > 0.0 && isfinite(total)) {
        const double target = aco_rng_uniform(&colony->rng) * total;
        double cumulative = 0.0;

        for (size_t k = 0; k + 1 < remaining; ++k) {
            cumulative += row[candidates[k]];
            if (target < cumulative) {
                return k;
            }
        }
        return remaining - 1;
    }

    /* Every weight underflowed to zero: no information left, choose uniformly. */
    return aco_rng_below(&colony->rng, remaining);
}

static void construct_tour(aco_colony *colony, size_t start, size_t *tour)
{
    const size_t size = colony->size;
    size_t *candidates = colony->candidates;
    size_t remaining = 0;
    size_t current = start;

    for (size_t city = 0; city < size; ++city) {
        if (city != start) {
            candidates[remaining++] = city;
        }
    }

    tour[0] = start;
    for (size_t step = 1; step < size; ++step) {
        const size_t pick = choose_candidate(colony, current, remaining);

        current = candidates[pick];
        candidates[pick] = candidates[--remaining];
        tour[step] = current;
    }
}

static void deposit(aco_colony *colony, const size_t *tour, double amount)
{
    const size_t size = colony->size;

    for (size_t i = 0; i < size; ++i) {
        const size_t from = tour[i];
        const size_t to = tour[(i + 1 == size) ? 0 : i + 1];

        colony->pheromone[from * size + to] += amount;
        colony->pheromone[to * size + from] += amount;
    }
}

/* A zero-length tour only exists when all cities coincide; any amount works then. */
static double deposit_amount(double budget, double length)
{
    return length > 0.0 ? budget / length : budget;
}

static void update_pheromone(aco_colony *colony)
{
    const size_t size = colony->size;
    const double persistence = 1.0 - colony->params.rho;

    for (size_t cell = 0; cell < size * size; ++cell) {
        colony->pheromone[cell] *= persistence;
    }
    for (size_t ant = 0; ant < colony->params.ants; ++ant) {
        deposit(colony, colony->tours + ant * size,
                deposit_amount(colony->params.q, colony->lengths[ant]));
    }
    if (colony->params.elitist_weight > 0.0) {
        deposit(
            colony, colony->best.cities,
            deposit_amount(colony->params.elitist_weight * colony->params.q, colony->best.length));
    }
}

aco_status aco_colony_step(aco_colony *colony, aco_iteration *iteration)
{
    size_t size;
    size_t ants;
    size_t best_ant = 0;
    const double *distances;
    double *lengths;
    double best = HUGE_VAL;
    double worst = 0.0;
    double sum = 0.0;
    bool improved = false;

    if (colony == NULL) {
        return ACO_ERR_INVALID_ARGUMENT;
    }
    size = colony->size;
    ants = colony->params.ants;
    distances = colony->instance->distances;
    lengths = colony->lengths;

    refresh_attraction(colony);
    for (size_t ant = 0; ant < ants; ++ant) {
        size_t *tour = colony->tours + ant * size;
        /* One ant per city when the numbers match, as in the original Ant System. */
        const size_t start = (ants == size) ? ant : aco_rng_below(&colony->rng, size);

        construct_tour(colony, start, tour);
        if (colony->params.local_search == ACO_LOCAL_SEARCH_TWO_OPT) {
            (void)aco_two_opt_cycle(distances, size, tour);
        }
        lengths[ant] = aco_cycle_length(distances, size, tour);

        sum += lengths[ant];
        if (lengths[ant] > worst) {
            worst = lengths[ant];
        }
        if (lengths[ant] < best) {
            best = lengths[ant];
            best_ant = ant;
        }
    }

    ++colony->iteration;
    if (best < colony->best.length) {
        memcpy(colony->best.cities, colony->tours + best_ant * size, size * sizeof(size_t));
        colony->best.length = best;
        colony->best_iteration = colony->iteration;
        improved = true;
    }
    update_pheromone(colony);

    if (iteration != NULL) {
        iteration->index = colony->iteration;
        iteration->best = best;
        iteration->mean = sum / (double)ants;
        iteration->worst = worst;
        iteration->global_best = colony->best.length;
        iteration->improved = improved;
    }
    return ACO_OK;
}

aco_status aco_colony_run(aco_colony *colony, aco_observer observer, void *context)
{
    if (colony == NULL) {
        return ACO_ERR_INVALID_ARGUMENT;
    }

    while (colony->iteration < colony->params.iterations) {
        aco_iteration iteration;
        const aco_status status = aco_colony_step(colony, &iteration);

        if (status != ACO_OK) {
            return status;
        }
        if (observer != NULL && !observer(colony, &iteration, context)) {
            break;
        }
        if (colony->params.patience > 0 &&
            colony->iteration - colony->best_iteration >= colony->params.patience) {
            break;
        }
    }
    return ACO_OK;
}

/* ------------------------------------------------------------------------ */
/* Accessors and convenience                                                */
/* ------------------------------------------------------------------------ */

const aco_tour *aco_colony_best(const aco_colony *colony)
{
    return &colony->best;
}

size_t aco_colony_iterations(const aco_colony *colony)
{
    return colony->iteration;
}

size_t aco_colony_best_iteration(const aco_colony *colony)
{
    return colony->best_iteration;
}

double aco_colony_pheromone(const aco_colony *colony, size_t from, size_t to)
{
    assert(from < colony->size && to < colony->size);
    return colony->pheromone[from * colony->size + to];
}

const aco_params *aco_colony_params(const aco_colony *colony)
{
    return &colony->params;
}

aco_status aco_solve(const aco_instance *instance, const aco_params *params, aco_tour *tour,
                     aco_error *error)
{
    aco_colony *colony = NULL;
    aco_status status;

    if (tour == NULL) {
        return ACO_FAIL(error, ACO_ERR_INVALID_ARGUMENT, "tour must not be NULL");
    }
    status = aco_colony_create(instance, params, &colony, error);
    if (status != ACO_OK) {
        return status;
    }
    status = aco_colony_run(colony, NULL, NULL);
    if (status == ACO_OK) {
        status = aco_tour_copy(tour, &colony->best);
    }
    if (status != ACO_OK) {
        (void)ACO_FAIL(error, status, "%s", aco_status_string(status));
    }
    aco_colony_destroy(colony);
    return status;
}
