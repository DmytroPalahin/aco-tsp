/*
 * `aco-tsp solve`: load an instance, run one of the solvers, report the tour.
 */
#include "aco/aco.h"

#include "args.h"
#include "cli.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef enum algorithm {
    ALGORITHM_ACO,
    ALGORITHM_NEAREST_NEIGHBOUR,
    ALGORITHM_TWO_OPT,
    ALGORITHM_EXACT
} algorithm;

typedef struct solve_options {
    const char *instance_path;
    algorithm algorithm;
    aco_params params;
    bool seed_given;
    const char *tour_path;  /* TSPLIB tour file, or NULL */
    const char *trace_path; /* per-iteration CSV, or NULL */
    const char *plot_path;  /* coordinates of the tour, or NULL */
    bool quiet;
    bool help;
} solve_options;

/* What a solver reports besides the tour itself. */
typedef struct solve_report {
    size_t iterations;     /* iterations performed (colony only) */
    size_t best_iteration; /* iteration that found the tour (colony only) */
    aco_params params;     /* parameters in effect (colony only) */
    double seconds;        /* processor time spent solving */
} solve_report;

enum {
    OPT_ALGORITHM = 'a',
    OPT_HELP = 'h',
    OPT_ITERATIONS = 'i',
    OPT_ANTS = 'm',
    OPT_TOUR = 'o',
    OPT_QUIET = 'q',
    OPT_SEED = 's',
    /* Options without a short form are numbered above the character range. */
    OPT_ALPHA = 256,
    OPT_BETA = 257,
    OPT_RHO = 258,
    OPT_DEPOSIT = 259,
    OPT_TAU0 = 260,
    OPT_Q0 = 261,
    OPT_ELITIST = 262,
    OPT_LOCAL_SEARCH = 263,
    OPT_PATIENCE = 264,
    OPT_TRACE = 265,
    OPT_PLOT = 266
};

static const cli_option solve_option_table[] = {
    {"algorithm", OPT_ALGORITHM, 'a', true},
    {"ants", OPT_ANTS, 'm', true},
    {"iterations", OPT_ITERATIONS, 'i', true},
    {"alpha", OPT_ALPHA, '\0', true},
    {"beta", OPT_BETA, '\0', true},
    {"rho", OPT_RHO, '\0', true},
    {"deposit", OPT_DEPOSIT, '\0', true},
    {"tau0", OPT_TAU0, '\0', true},
    {"q0", OPT_Q0, '\0', true},
    {"elitist", OPT_ELITIST, '\0', true},
    {"local-search", OPT_LOCAL_SEARCH, '\0', true},
    {"patience", OPT_PATIENCE, '\0', true},
    {"seed", OPT_SEED, 's', true},
    {"tour", OPT_TOUR, 'o', true},
    {"trace", OPT_TRACE, '\0', true},
    {"plot", OPT_PLOT, '\0', true},
    {"quiet", OPT_QUIET, 'q', false},
    {"help", OPT_HELP, 'h', false},
};

void cli_solve_help(FILE *stream)
{
    (void)fputs(
        "Usage: " CLI_PROGRAM " solve <instance> [options]\n"
        "\n"
        "Find a short closed tour through every city of <instance>, which may be in the\n"
        "triangular format (city count, then the upper triangle of the distance matrix)\n"
        "or in TSPLIB format (EUC_2D, CEIL_2D or EXPLICIT).\n"
        "\n"
        "Algorithm:\n"
        "  -a, --algorithm NAME    aco (default), nearest-neighbour, two-opt or exact\n"
        "\n"
        "Colony (aco only):\n"
        "  -m, --ants N            number of ants                [one per city]\n"
        "  -i, --iterations N      number of iterations          [200]\n"
        "      --alpha X           weight of the pheromone       [1]\n"
        "      --beta X            weight of the visibility      [3]\n"
        "      --rho X             evaporation rate, in [0, 1]   [0.5]\n"
        "      --deposit X         pheromone budget Q of one ant [1]\n"
        "      --tau0 X            initial pheromone             [ants * Q / greedy length]\n"
        "      --q0 X              chance of a greedy move       [0]\n"
        "      --elitist X         weight of the best-so-far tour [0]\n"
        "      --local-search NAME none (default) or 2opt\n"
        "      --patience N        stop after N iterations without progress [off]\n"
        "  -s, --seed N            random seed                   [from the clock]\n"
        "\n"
        "Output:\n"
        "  -o, --tour FILE         write the tour in TSPLIB format\n"
        "      --trace FILE        write per-iteration statistics as CSV (aco only)\n"
        "      --plot FILE         write the tour as x y lines for gnuplot\n"
        "  -q, --quiet             print only the tour length\n"
        "  -h, --help              show this help\n"
        "\n"
        "The baselines nearest-neighbour and two-opt try every city as starting point\n"
        "and keep the best tour. The exact solver is limited to 20 cities.\n",
        stream);
}

/* ------------------------------------------------------------------------ */
/* Option parsing                                                           */
/* ------------------------------------------------------------------------ */

static bool parse_algorithm(const char *text, algorithm *value)
{
    if (strcmp(text, "aco") == 0) {
        *value = ALGORITHM_ACO;
    } else if (strcmp(text, "nearest-neighbour") == 0 || strcmp(text, "nearest-neighbor") == 0 ||
               strcmp(text, "nn") == 0) {
        *value = ALGORITHM_NEAREST_NEIGHBOUR;
    } else if (strcmp(text, "two-opt") == 0 || strcmp(text, "2opt") == 0) {
        *value = ALGORITHM_TWO_OPT;
    } else if (strcmp(text, "exact") == 0) {
        *value = ALGORITHM_EXACT;
    } else {
        return false;
    }
    return true;
}

static bool parse_local_search(const char *text, aco_local_search *value)
{
    if (strcmp(text, "none") == 0) {
        *value = ACO_LOCAL_SEARCH_NONE;
    } else if (strcmp(text, "2opt") == 0 || strcmp(text, "two-opt") == 0) {
        *value = ACO_LOCAL_SEARCH_TWO_OPT;
    } else {
        return false;
    }
    return true;
}

/* Stores the value of one option; returns false if the value is malformed. */
static bool apply_option(solve_options *options, int id, const char *value)
{
    aco_params *params = &options->params;

    switch (id) {
    case OPT_ALGORITHM:
        return parse_algorithm(value, &options->algorithm);
    case OPT_ANTS:
        return cli_parse_size(value, &params->ants) && params->ants > 0;
    case OPT_ITERATIONS:
        return cli_parse_size(value, &params->iterations);
    case OPT_ALPHA:
        return cli_parse_double(value, &params->alpha);
    case OPT_BETA:
        return cli_parse_double(value, &params->beta);
    case OPT_RHO:
        return cli_parse_double(value, &params->rho);
    case OPT_DEPOSIT:
        return cli_parse_double(value, &params->q);
    case OPT_TAU0:
        return cli_parse_double(value, &params->tau0) && params->tau0 > 0.0;
    case OPT_Q0:
        return cli_parse_double(value, &params->q0);
    case OPT_ELITIST:
        return cli_parse_double(value, &params->elitist_weight);
    case OPT_LOCAL_SEARCH:
        return parse_local_search(value, &params->local_search);
    case OPT_PATIENCE:
        return cli_parse_size(value, &params->patience);
    case OPT_SEED:
        options->seed_given = true;
        return cli_parse_uint64(value, &params->seed);
    case OPT_TOUR:
        options->tour_path = value;
        return true;
    case OPT_TRACE:
        options->trace_path = value;
        return true;
    case OPT_PLOT:
        options->plot_path = value;
        return true;
    case OPT_QUIET:
        options->quiet = true;
        return true;
    case OPT_HELP:
        options->help = true;
        return true;
    default:
        return false;
    }
}

static int parse_options(int argc, char **argv, solve_options *options)
{
    cli_parser parser;
    aco_error error = ACO_ERROR_INIT;
    int id;

    memset(options, 0, sizeof *options);
    options->algorithm = ALGORITHM_ACO;
    options->params = aco_params_default();

    cli_parser_init(&parser, argc, argv, solve_option_table,
                    sizeof solve_option_table / sizeof solve_option_table[0]);
    while ((id = cli_next(&parser)) != CLI_DONE) {
        if (id == CLI_INVALID) {
            return cli_usage_error("solve", "%s", parser.message);
        }
        if (id == CLI_POSITIONAL) {
            if (options->instance_path != NULL) {
                return cli_usage_error("solve", "unexpected argument '%s'", parser.value);
            }
            options->instance_path = parser.value;
        } else if (!apply_option(options, id, parser.value)) {
            return cli_usage_error("solve", "invalid value '%s' for option '--%s'", parser.value,
                                   parser.name);
        }
    }

    if (options->help) {
        return CLI_EXIT_OK;
    }
    if (options->instance_path == NULL) {
        return cli_usage_error("solve", "missing instance file");
    }
    if (aco_params_validate(&options->params, &error) != ACO_OK) {
        return cli_usage_error("solve", "%s", error.message);
    }
    if (options->trace_path != NULL && options->algorithm != ALGORITHM_ACO) {
        return cli_usage_error("solve", "--trace is only available with the aco algorithm");
    }
    return CLI_EXIT_OK;
}

/* ------------------------------------------------------------------------ */
/* Solvers                                                                  */
/* ------------------------------------------------------------------------ */

static bool write_trace_row(const aco_colony *colony, const aco_iteration *iteration, void *context)
{
    FILE *trace = context;

    (void)colony;
    (void)fprintf(trace, "%zu,%.10g,%.10g,%.10g,%.10g\n", iteration->index, iteration->best,
                  iteration->mean, iteration->worst, iteration->global_best);
    return true;
}

static aco_status run_colony(const aco_instance *instance, const solve_options *options,
                             FILE *trace, aco_tour *tour, solve_report *report, aco_error *error)
{
    aco_colony *colony = NULL;
    aco_status status = aco_colony_create(instance, &options->params, &colony, error);

    if (status != ACO_OK) {
        return status;
    }
    if (trace != NULL) {
        (void)fputs("iteration,iteration_best,iteration_mean,iteration_worst,global_best\n", trace);
    }
    status = aco_colony_run(colony, trace != NULL ? write_trace_row : NULL, trace);
    if (status == ACO_OK) {
        status = aco_tour_copy(tour, aco_colony_best(colony));
    }
    if (status == ACO_OK) {
        report->iterations = aco_colony_iterations(colony);
        report->best_iteration = aco_colony_best_iteration(colony);
        report->params = *aco_colony_params(colony);
    } else {
        error->status = status;
        (void)snprintf(error->message, sizeof error->message, "%s", aco_status_string(status));
    }
    aco_colony_destroy(colony);
    return status;
}

/* Nearest neighbour from every city, optionally followed by 2-opt; keeps the best. */
static aco_status run_greedy(const aco_instance *instance, bool improve, aco_tour *tour,
                             aco_error *error)
{
    aco_tour candidate = ACO_TOUR_INIT;
    aco_status status = ACO_OK;
    bool found = false;

    for (size_t start = 0; status == ACO_OK && start < aco_instance_size(instance); ++start) {
        status = aco_nearest_neighbour(instance, start, &candidate);
        if (status == ACO_OK && improve) {
            status = aco_two_opt(instance, &candidate, NULL);
        }
        if (status == ACO_OK && (!found || candidate.length < tour->length)) {
            status = aco_tour_copy(tour, &candidate);
            found = true;
        }
    }
    aco_tour_free(&candidate);

    if (status != ACO_OK) {
        error->status = status;
        (void)snprintf(error->message, sizeof error->message, "%s", aco_status_string(status));
    }
    return status;
}

static aco_status run(const aco_instance *instance, const solve_options *options, FILE *trace,
                      aco_tour *tour, solve_report *report, aco_error *error)
{
    const clock_t started = clock();
    aco_status status = ACO_OK;

    switch (options->algorithm) {
    case ALGORITHM_ACO:
        status = run_colony(instance, options, trace, tour, report, error);
        break;
    case ALGORITHM_NEAREST_NEIGHBOUR:
        status = run_greedy(instance, false, tour, error);
        break;
    case ALGORITHM_TWO_OPT:
        status = run_greedy(instance, true, tour, error);
        break;
    case ALGORITHM_EXACT:
        status = aco_exact(instance, tour, error);
        break;
    }
    report->seconds = (double)(clock() - started) / (double)CLOCKS_PER_SEC;
    return status;
}

/* ------------------------------------------------------------------------ */
/* Reporting                                                                */
/* ------------------------------------------------------------------------ */

static const char *algorithm_name(algorithm value)
{
    switch (value) {
    case ALGORITHM_ACO:
        return "ant colony optimisation";
    case ALGORITHM_NEAREST_NEIGHBOUR:
        return "nearest neighbour (best of all starts)";
    case ALGORITHM_TWO_OPT:
        return "nearest neighbour + 2-opt (best of all starts)";
    case ALGORITHM_EXACT:
        return "exact (Held-Karp)";
    }
    return "unknown";
}

static void print_tour(const aco_tour *tour)
{
    static const char label[] = "tour        ";
    size_t column = sizeof label - 1;

    (void)fputs(label, stdout);
    for (size_t i = 0; i < tour->size; ++i) {
        char number[24];
        const int width = snprintf(number, sizeof number, "%zu", tour->cities[i] + 1);

        if (i > 0 && column + (size_t)width + 1 > 78) {
            (void)printf("\n%*s", (int)(sizeof label - 1), "");
            column = sizeof label - 1;
        }
        (void)printf(" %s", number);
        column += (size_t)width + 1;
    }
    (void)fputc('\n', stdout);
}

static void print_summary(const aco_instance *instance, const solve_options *options,
                          const aco_tour *tour, const solve_report *report)
{
    const aco_params *params = &report->params;

    (void)printf("instance     %s (%zu cities)\n", aco_instance_name(instance),
                 aco_instance_size(instance));
    (void)printf("algorithm    %s\n", algorithm_name(options->algorithm));
    if (options->algorithm == ALGORITHM_ACO) {
        (void)printf("parameters   ants=%zu iterations=%zu alpha=%g beta=%g rho=%g deposit=%g\n",
                     params->ants, params->iterations, params->alpha, params->beta, params->rho,
                     params->q);
        (void)printf("             tau0=%.4g q0=%g elitist=%g local-search=%s\n", params->tau0,
                     params->q0, params->elitist_weight,
                     params->local_search == ACO_LOCAL_SEARCH_TWO_OPT ? "2opt" : "none");
        (void)printf("seed         %llu\n", (unsigned long long)params->seed);
        (void)printf("iterations   %zu (best tour found at iteration %zu)\n", report->iterations,
                     report->best_iteration);
    }
    (void)printf("time         %.3f s\n", report->seconds);
    (void)printf("length       %.10g\n", tour->length);
    print_tour(tour);
}

static int write_tour_file(const char *path, const aco_instance *instance, const aco_tour *tour)
{
    FILE *stream = fopen(path, "w");
    aco_status status;

    if (stream == NULL) {
        return cli_error(CLI_EXIT_FAILURE, "cannot open '%s': %s", path, strerror(errno));
    }
    status = aco_tour_write(tour, aco_instance_name(instance), stream);
    if (fclose(stream) != 0 || status != ACO_OK) {
        return cli_error(CLI_EXIT_FAILURE, "cannot write '%s'", path);
    }
    return CLI_EXIT_OK;
}

/* One "x y" line per city in visiting order, the first city repeated to close the loop. */
static int write_plot_file(const char *path, const aco_instance *instance, const aco_tour *tour)
{
    const aco_point *points = aco_instance_points(instance);
    FILE *stream = fopen(path, "w");
    bool failed;

    if (stream == NULL) {
        return cli_error(CLI_EXIT_FAILURE, "cannot open '%s': %s", path, strerror(errno));
    }
    (void)fprintf(stream, "# x y city   (%s, length %.10g)\n", aco_instance_name(instance),
                  tour->length);
    for (size_t i = 0; i <= tour->size; ++i) {
        const size_t city = tour->cities[i % tour->size];

        (void)fprintf(stream, "%.10g %.10g %zu\n", points[city].x, points[city].y, city + 1);
    }
    failed = ferror(stream) != 0;
    if (fclose(stream) != 0 || failed) {
        return cli_error(CLI_EXIT_FAILURE, "cannot write '%s'", path);
    }
    return CLI_EXIT_OK;
}

/* ------------------------------------------------------------------------ */
/* Command                                                                  */
/* ------------------------------------------------------------------------ */

int cli_solve(int argc, char **argv)
{
    solve_options options;
    solve_report report = {0};
    aco_instance *instance = NULL;
    aco_tour tour = ACO_TOUR_INIT;
    aco_error error = ACO_ERROR_INIT;
    FILE *trace = NULL;
    int exit_status = parse_options(argc, argv, &options);

    if (exit_status != CLI_EXIT_OK) {
        return exit_status;
    }
    if (options.help) {
        cli_solve_help(stdout);
        return CLI_EXIT_OK;
    }
    if (!options.seed_given) {
        /* Mixing in the processor clock separates runs started within the same second. */
        options.params.seed = (uint64_t)time(NULL) ^ ((uint64_t)clock() << 32);
    }

    if (aco_instance_load(options.instance_path, &instance, &error) != ACO_OK) {
        /* I/O errors already name the file; parse errors only give a line number. */
        if (error.status == ACO_ERR_IO) {
            return cli_error(CLI_EXIT_FAILURE, "%s", error.message);
        }
        return cli_error(CLI_EXIT_FAILURE, "%s: %s", options.instance_path, error.message);
    }
    if (options.plot_path != NULL && aco_instance_points(instance) == NULL) {
        exit_status = cli_error(CLI_EXIT_FAILURE,
                                "--plot needs city coordinates, which '%s' does not provide",
                                options.instance_path);
        goto cleanup;
    }
    if (options.trace_path != NULL) {
        trace = fopen(options.trace_path, "w");
        if (trace == NULL) {
            exit_status = cli_error(CLI_EXIT_FAILURE, "cannot open '%s': %s", options.trace_path,
                                    strerror(errno));
            goto cleanup;
        }
    }

    if (run(instance, &options, trace, &tour, &report, &error) != ACO_OK) {
        exit_status = cli_error(CLI_EXIT_FAILURE, "%s", error.message);
        goto cleanup;
    }
    aco_tour_rotate_to(&tour, 0);

    if (options.quiet) {
        (void)printf("%.10g\n", tour.length);
    } else {
        print_summary(instance, &options, &tour, &report);
    }
    if (options.tour_path != NULL) {
        exit_status = write_tour_file(options.tour_path, instance, &tour);
    }
    if (exit_status == CLI_EXIT_OK && options.plot_path != NULL) {
        exit_status = write_plot_file(options.plot_path, instance, &tour);
    }

cleanup:
    if (trace != NULL && fclose(trace) != 0 && exit_status == CLI_EXIT_OK) {
        exit_status = cli_error(CLI_EXIT_FAILURE, "cannot write '%s'", options.trace_path);
    }
    aco_tour_free(&tour);
    aco_instance_destroy(instance);
    return exit_status;
}
