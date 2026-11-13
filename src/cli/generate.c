/*
 * `aco-tsp generate`: write a random Euclidean instance.
 */
#include "aco/aco.h"

#include "args.h"
#include "cli.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct generate_options {
    size_t cities;
    uint64_t extent;
    uint64_t seed;
    bool seed_given;
    aco_file_format format;
    const char *output_path; /* NULL means standard output */
    bool help;
} generate_options;

enum {
    OPT_FORMAT = 'f',
    OPT_HELP = 'h',
    OPT_CITIES = 'n',
    OPT_OUTPUT = 'o',
    OPT_SEED = 's',
    OPT_EXTENT = 256
};

static const cli_option generate_option_table[] = {
    {"cities", OPT_CITIES, 'n', true}, {"extent", OPT_EXTENT, '\0', true},
    {"seed", OPT_SEED, 's', true},     {"format", OPT_FORMAT, 'f', true},
    {"output", OPT_OUTPUT, 'o', true}, {"help", OPT_HELP, 'h', false},
};

void cli_generate_help(FILE *stream)
{
    (void)fputs("Usage: " CLI_PROGRAM " generate --cities N [options]\n"
                "\n"
                "Create a random instance: N cities drawn uniformly from the integer grid\n"
                "[0, extent] x [0, extent], with Euclidean distances rounded to the nearest\n"
                "integer. The same arguments always produce the same instance.\n"
                "\n"
                "Options:\n"
                "  -n, --cities N       number of cities, from 2 to 5000 (required)\n"
                "      --extent N       side of the square the cities lie in [1000]\n"
                "  -s, --seed N         random seed                          [from the clock]\n"
                "  -f, --format NAME    tsplib (default, keeps the coordinates) or triangular\n"
                "  -o, --output FILE    file to write                        [standard output]\n"
                "  -h, --help           show this help\n",
                stream);
}

static bool parse_format(const char *text, aco_file_format *format)
{
    if (strcmp(text, "tsplib") == 0) {
        *format = ACO_FILE_FORMAT_TSPLIB;
    } else if (strcmp(text, "triangular") == 0) {
        *format = ACO_FILE_FORMAT_TRIANGULAR;
    } else {
        return false;
    }
    return true;
}

static bool apply_option(generate_options *options, int id, const char *value)
{
    switch (id) {
    case OPT_CITIES:
        return cli_parse_size(value, &options->cities);
    case OPT_EXTENT:
        return cli_parse_uint64(value, &options->extent) && options->extent >= 1 &&
               options->extent <= UINT32_MAX;
    case OPT_SEED:
        options->seed_given = true;
        return cli_parse_uint64(value, &options->seed);
    case OPT_FORMAT:
        return parse_format(value, &options->format);
    case OPT_OUTPUT:
        options->output_path = value;
        return true;
    case OPT_HELP:
        options->help = true;
        return true;
    default:
        return false;
    }
}

static int parse_options(int argc, char **argv, generate_options *options)
{
    cli_parser parser;
    int id;

    memset(options, 0, sizeof *options);
    options->extent = 1000;
    options->format = ACO_FILE_FORMAT_TSPLIB;

    cli_parser_init(&parser, argc, argv, generate_option_table,
                    sizeof generate_option_table / sizeof generate_option_table[0]);
    while ((id = cli_next(&parser)) != CLI_DONE) {
        if (id == CLI_INVALID) {
            return cli_usage_error("generate", "%s", parser.message);
        }
        if (id == CLI_POSITIONAL) {
            return cli_usage_error("generate", "unexpected argument '%s'", parser.value);
        }
        if (!apply_option(options, id, parser.value)) {
            return cli_usage_error("generate", "invalid value '%s' for option '--%s'", parser.value,
                                   parser.name);
        }
    }

    if (!options->help && options->cities == 0) {
        return cli_usage_error("generate", "missing required option '--cities'");
    }
    return CLI_EXIT_OK;
}

/* Writes the instance to the requested file, or to standard output. */
static int write_instance(const aco_instance *instance, const generate_options *options)
{
    const char *path = options->output_path;
    FILE *stream = stdout;
    aco_status status;

    if (path != NULL) {
        stream = fopen(path, "w");
        if (stream == NULL) {
            return cli_error(CLI_EXIT_FAILURE, "cannot open '%s': %s", path, strerror(errno));
        }
    }
    status = aco_instance_write(instance, options->format, stream, NULL);
    if (path != NULL && fclose(stream) != 0) {
        status = ACO_ERR_IO;
    }
    if (status != ACO_OK) {
        return cli_error(CLI_EXIT_FAILURE, "cannot write '%s'",
                         path != NULL ? path : "standard output");
    }
    if (path != NULL) {
        (void)fprintf(stderr, "wrote %zu cities to %s (seed %llu)\n", options->cities, path,
                      (unsigned long long)options->seed);
    }
    return CLI_EXIT_OK;
}

int cli_generate(int argc, char **argv)
{
    generate_options options;
    aco_instance *instance = NULL;
    aco_error error = ACO_ERROR_INIT;
    aco_status status;
    int exit_status = parse_options(argc, argv, &options);

    if (exit_status != CLI_EXIT_OK) {
        return exit_status;
    }
    if (options.help) {
        cli_generate_help(stdout);
        return CLI_EXIT_OK;
    }
    if (!options.seed_given) {
        options.seed = (uint64_t)time(NULL) ^ ((uint64_t)clock() << 32);
    }

    status = aco_instance_create_random(options.cities, (uint32_t)options.extent, options.seed,
                                        &instance, &error);
    if (status != ACO_OK) {
        return cli_error(status == ACO_ERR_INVALID_ARGUMENT ? CLI_EXIT_USAGE : CLI_EXIT_FAILURE,
                         "%s", error.message);
    }

    exit_status = write_instance(instance, &options);
    aco_instance_destroy(instance);
    return exit_status;
}
