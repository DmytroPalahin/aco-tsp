/*
 * libFuzzer harness for the instance parser, the only part of the library
 * that reads untrusted input.
 *
 * Whatever the bytes are, parsing must either fail cleanly or produce an
 * instance that satisfies the documented invariants and survives a round
 * trip through the solver's entry points.
 *
 *     cmake --preset fuzz && cmake --build --preset fuzz
 *     build/fuzz/fuzz/fuzz_instance_parse -max_len=2048 fuzz/corpus
 */
#include "aco/aco.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* Keeps a single execution fast: larger instances are parsed but not solved. */
#define MAX_SOLVED_CITIES 48

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    aco_instance *instance = NULL;
    aco_error error = ACO_ERROR_INIT;
    const aco_status status = aco_instance_parse((const char *)data, size, &instance, &error);
    size_t cities;

    if (status != ACO_OK) {
        /* A failure must be reported consistently and leave nothing behind. */
        if (instance != NULL || error.status != status || error.message[0] == '\0') {
            abort();
        }
        return 0;
    }

    cities = aco_instance_size(instance);
    if (cities < ACO_MIN_CITIES || cities > ACO_MAX_CITIES) {
        abort();
    }
    for (size_t i = 0; i < cities; ++i) {
        if (aco_instance_distance(instance, i, i) != 0.0) {
            abort();
        }
        for (size_t j = i + 1; j < cities; ++j) {
            const double forward = aco_instance_distance(instance, i, j);

            if (!(forward >= 0.0) || forward != aco_instance_distance(instance, j, i)) {
                abort();
            }
        }
    }

    if (cities <= MAX_SOLVED_CITIES) {
        aco_params params = aco_params_default();
        aco_tour tour = ACO_TOUR_INIT;

        params.iterations = 2;
        params.local_search = ACO_LOCAL_SEARCH_TWO_OPT;
        if (aco_solve(instance, &params, &tour, NULL) != ACO_OK ||
            !aco_tour_is_valid(instance, &tour)) {
            abort();
        }
        aco_tour_free(&tour);
    }

    aco_instance_destroy(instance);
    return 0;
}
