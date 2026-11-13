/*
 * A minimal command-line option parser.
 *
 * getopt_long() is not part of ISO C and is missing from MSVC, so the tool
 * carries its own. Supported spellings: `--name value`, `--name=value`,
 * `-n value`, `-nvalue`, and `--` to end the options.
 */
#ifndef ACO_CLI_ARGS_H
#define ACO_CLI_ARGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Description of one option. */
typedef struct cli_option {
    const char *long_name; /* without the leading dashes */
    int id;                /* returned by cli_next(); must be positive */
    char short_name;       /* '\0' if the option has no short form */
    bool takes_value;
} cli_option;

/* Results of cli_next() other than an option id. */
enum {
    CLI_DONE = -1,      /* no argument left */
    CLI_INVALID = -2,   /* unknown or malformed option; see cli_parser.message */
    CLI_POSITIONAL = -3 /* a positional argument; see cli_parser.value */
};

typedef struct cli_parser {
    int argc;
    char **argv;
    int index;
    const cli_option *options;
    size_t option_count;
    bool options_ended; /* set once `--` has been seen */
    const char *value;  /* option value or positional argument, when applicable */
    const char *name;   /* long name of the option returned last, for diagnostics */
    char message[128];  /* explanation when cli_next() returns CLI_INVALID */
} cli_parser;

/* Prepares to iterate over argv[0 .. argc). */
void cli_parser_init(cli_parser *parser, int argc, char **argv, const cli_option *options,
                     size_t option_count);

/* Returns the id of the next option, or one of the CLI_* results. */
int cli_next(cli_parser *parser);

/*
 * Strict conversions: the whole string must be a number in range. They return
 * false, leaving *value untouched, on anything else.
 */
bool cli_parse_size(const char *text, size_t *value);
bool cli_parse_uint64(const char *text, uint64_t *value);
bool cli_parse_double(const char *text, double *value);

#endif /* ACO_CLI_ARGS_H */
