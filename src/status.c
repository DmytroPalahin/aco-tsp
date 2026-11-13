#include "aco/status.h"
#include "aco/version.h"

#include "internal.h"

#include <stdarg.h>
#include <stdio.h>

const char *aco_status_string(aco_status status)
{
    switch (status) {
    case ACO_OK:
        return "success";
    case ACO_ERR_INVALID_ARGUMENT:
        return "invalid argument";
    case ACO_ERR_OUT_OF_MEMORY:
        return "out of memory";
    case ACO_ERR_IO:
        return "input/output error";
    case ACO_ERR_PARSE:
        return "malformed input";
    case ACO_ERR_UNSUPPORTED:
        return "unsupported input";
    }
    return "unknown status";
}

const char *aco_version(void)
{
    return ACO_VERSION_STRING;
}

void aco_error_set(aco_error *error, aco_status status, const char *format, ...)
{
    va_list arguments;

    if (error == NULL) {
        return;
    }
    error->status = status;
    va_start(arguments, format);
    (void)vsnprintf(error->message, sizeof error->message, format, arguments);
    va_end(arguments);
}
