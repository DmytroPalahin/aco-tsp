/*
 * Shared declarations of the `aco-tsp` command-line tool.
 */
#ifndef ACO_CLI_CLI_H
#define ACO_CLI_CLI_H

#include <stdio.h>

#define CLI_PROGRAM "aco-tsp"

/* Exit statuses, following the common Unix convention. */
enum {
    CLI_EXIT_OK = 0,
    CLI_EXIT_FAILURE = 1, /* the request was understood but could not be carried out */
    CLI_EXIT_USAGE = 2    /* the command line itself is wrong */
};

/* Each command receives its own arguments, the command name excluded. */
int cli_solve(int argc, char **argv);
int cli_generate(int argc, char **argv);

void cli_solve_help(FILE *stream);
void cli_generate_help(FILE *stream);

/* Prints "aco-tsp: <message>" to standard error; returns `status` for chaining. */
int cli_error(int status, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/* Reports a command-line mistake and points to the help of `command`. */
int cli_usage_error(const char *command, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

#endif /* ACO_CLI_CLI_H */
