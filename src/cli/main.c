/*
 * aco-tsp: command-line front end of the library.
 *
 *     aco-tsp solve <instance> [options]
 *     aco-tsp generate --cities N [options]
 */
#include "aco/version.h"

#include "cli.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int cli_error(int status, const char *format, ...)
{
    va_list arguments;

    (void)fprintf(stderr, CLI_PROGRAM ": ");
    va_start(arguments, format);
    (void)vfprintf(stderr, format, arguments);
    va_end(arguments);
    (void)fputc('\n', stderr);
    return status;
}

int cli_usage_error(const char *command, const char *format, ...)
{
    va_list arguments;

    (void)fprintf(stderr, CLI_PROGRAM ": ");
    va_start(arguments, format);
    (void)vfprintf(stderr, format, arguments);
    va_end(arguments);
    if (command != NULL) {
        (void)fprintf(stderr, "\nTry '" CLI_PROGRAM " %s --help' for more information.\n", command);
    } else {
        (void)fprintf(stderr, "\nTry '" CLI_PROGRAM " --help' for more information.\n");
    }
    return CLI_EXIT_USAGE;
}

static void print_help(FILE *stream)
{
    (void)fputs("Usage: " CLI_PROGRAM " <command> [options]\n"
                "\n"
                "Solve the symmetric travelling salesman problem with ant colony optimisation.\n"
                "\n"
                "Commands:\n"
                "  solve <instance>   Find a short tour of an instance\n"
                "  generate           Create a random Euclidean instance\n"
                "  help [command]     Show this help, or the help of a command\n"
                "  version            Show the version\n"
                "\n"
                "Examples:\n"
                "  " CLI_PROGRAM " solve data/course/tsp050.txt --seed 1\n"
                "  " CLI_PROGRAM " generate --cities 100 --seed 7 --output cities.tsp\n"
                "  " CLI_PROGRAM " solve cities.tsp --local-search 2opt --plot tour.dat\n",
                stream);
}

static int help(int argc, char **argv)
{
    if (argc == 0) {
        print_help(stdout);
    } else if (strcmp(argv[0], "solve") == 0) {
        cli_solve_help(stdout);
    } else if (strcmp(argv[0], "generate") == 0) {
        cli_generate_help(stdout);
    } else {
        return cli_usage_error(NULL, "unknown command '%s'", argv[0]);
    }
    return CLI_EXIT_OK;
}

int main(int argc, char **argv)
{
    const char *command;

    if (argc < 2) {
        print_help(stderr);
        return CLI_EXIT_USAGE;
    }
    command = argv[1];

    if (strcmp(command, "solve") == 0) {
        return cli_solve(argc - 2, argv + 2);
    }
    if (strcmp(command, "generate") == 0) {
        return cli_generate(argc - 2, argv + 2);
    }
    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 ||
        strcmp(command, "-h") == 0) {
        return help(argc - 2, argv + 2);
    }
    if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0 ||
        strcmp(command, "-V") == 0) {
        (void)printf(CLI_PROGRAM " %s\n", aco_version());
        return CLI_EXIT_OK;
    }
    return cli_usage_error(NULL, "unknown command '%s'", command);
}
