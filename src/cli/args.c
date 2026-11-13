#include "args.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cli_parser_init(cli_parser *parser, int argc, char **argv, const cli_option *options,
                     size_t option_count)
{
    parser->argc = argc;
    parser->argv = argv;
    parser->index = 0;
    parser->options = options;
    parser->option_count = option_count;
    parser->options_ended = false;
    parser->value = NULL;
    parser->name = NULL;
    parser->message[0] = '\0';
}

static const cli_option *find_long(const cli_parser *parser, const char *name, size_t length)
{
    for (size_t i = 0; i < parser->option_count; ++i) {
        const char *candidate = parser->options[i].long_name;

        if (candidate != NULL && strlen(candidate) == length &&
            strncmp(candidate, name, length) == 0) {
            return &parser->options[i];
        }
    }
    return NULL;
}

static const cli_option *find_short(const cli_parser *parser, char name)
{
    for (size_t i = 0; i < parser->option_count; ++i) {
        if (parser->options[i].short_name == name) {
            return &parser->options[i];
        }
    }
    return NULL;
}

static int invalid(cli_parser *parser, const char *problem, const char *argument)
{
    (void)snprintf(parser->message, sizeof parser->message, "%s '%s'", problem, argument);
    return CLI_INVALID;
}

int cli_next(cli_parser *parser)
{
    const cli_option *option;
    const char *argument;
    const char *attached = NULL; /* value glued to the option, as in --name=value */

    parser->value = NULL;
    parser->name = NULL;
    if (parser->index >= parser->argc) {
        return CLI_DONE;
    }
    argument = parser->argv[parser->index++];

    if (!parser->options_ended && strcmp(argument, "--") == 0) {
        parser->options_ended = true;
        return cli_next(parser);
    }
    /* A lone "-" is conventionally a file name (standard input or output). */
    if (parser->options_ended || argument[0] != '-' || argument[1] == '\0') {
        parser->value = argument;
        return CLI_POSITIONAL;
    }

    if (argument[1] == '-') {
        const char *name = argument + 2;
        const char *equals = strchr(name, '=');

        option = find_long(parser, name, equals != NULL ? (size_t)(equals - name) : strlen(name));
        if (equals != NULL) {
            attached = equals + 1;
        }
    } else {
        option = find_short(parser, argument[1]);
        if (argument[2] != '\0') {
            attached = argument + 2;
        }
    }

    if (option == NULL) {
        return invalid(parser, "unknown option", argument);
    }
    parser->name = option->long_name;
    if (!option->takes_value) {
        if (attached != NULL) {
            return invalid(parser, "unexpected value in", argument);
        }
        return option->id;
    }
    if (attached != NULL) {
        parser->value = attached;
    } else if (parser->index < parser->argc) {
        parser->value = parser->argv[parser->index++];
    } else {
        return invalid(parser, "missing value for option", argument);
    }
    return option->id;
}

bool cli_parse_uint64(const char *text, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    /* strtoull() would silently accept a sign or leading blanks. */
    if (text == NULL || text[0] < '0' || text[0] > '9') {
        return false;
    }
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0') {
        return false;
    }
    *value = (uint64_t)parsed;
    return true;
}

bool cli_parse_size(const char *text, size_t *value)
{
    uint64_t parsed = 0;

    if (!cli_parse_uint64(text, &parsed)) {
        return false;
    }
#if SIZE_MAX < UINT64_MAX
    if (parsed > SIZE_MAX) {
        return false;
    }
#endif
    *value = (size_t)parsed;
    return true;
}

bool cli_parse_double(const char *text, double *value)
{
    char *end = NULL;
    double parsed;

    if (text == NULL || text[0] == '\0' || text[0] == ' ' || text[0] == '\t') {
        return false;
    }
    parsed = strtod(text, &end);
    if (*end != '\0' || !isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}
