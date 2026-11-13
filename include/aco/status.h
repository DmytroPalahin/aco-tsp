/**
 * @file status.h
 * @brief Status codes and error reporting shared by the whole library.
 *
 * Every fallible function returns an ::aco_status. Functions that can fail
 * for reasons worth explaining (a malformed file, an out-of-range parameter)
 * also accept an optional ::aco_error that receives a human-readable message.
 */
#ifndef ACO_STATUS_H
#define ACO_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Outcome of a library call. */
typedef enum aco_status {
    ACO_OK = 0,               /**< The call succeeded. */
    ACO_ERR_INVALID_ARGUMENT, /**< An argument violates the documented contract. */
    ACO_ERR_OUT_OF_MEMORY,    /**< An allocation failed. */
    ACO_ERR_IO,               /**< A file could not be read or written. */
    ACO_ERR_PARSE,            /**< The input text is not a well-formed instance. */
    ACO_ERR_UNSUPPORTED       /**< The input is valid but outside what the library handles. */
} aco_status;

/** Capacity of aco_error::message, terminator included. */
#define ACO_ERROR_MESSAGE_SIZE 256

/**
 * Detailed description of a failure.
 *
 * The structure is written only when a call fails; a successful call leaves
 * it untouched. Passing `NULL` wherever an `aco_error *` is expected is
 * always allowed and simply discards the message.
 */
typedef struct aco_error {
    aco_status status;                    /**< Same value as the returned status. */
    char message[ACO_ERROR_MESSAGE_SIZE]; /**< NUL-terminated explanation. */
} aco_error;

/** Initialiser for an ::aco_error that reports no failure. */
#define ACO_ERROR_INIT {ACO_OK, {'\0'}}

/**
 * Returns a short, static description of @p status.
 *
 * @return A string that is never `NULL` and must not be freed.
 */
const char *aco_status_string(aco_status status);

#ifdef __cplusplus
}
#endif

#endif /* ACO_STATUS_H */
