/*
 * A deliberately tiny unit-test harness: no dependency, no registration
 * magic, portable to every C11 compiler.
 *
 *     static void test_addition(void)
 *     {
 *         EXPECT_EQ_SIZE(1 + 1, 2);
 *     }
 *
 *     int main(int argc, char **argv)
 *     {
 *         static const test_case cases[] = {TEST_CASE(test_addition)};
 *         return test_main(argc, argv, cases, sizeof cases / sizeof cases[0]);
 *     }
 *
 * EXPECT_* records a failure and carries on; REQUIRE* also returns from the
 * current test, for conditions the rest of the test depends on. Passing test
 * names on the command line runs only those tests.
 */
#ifndef ACO_TEST_H
#define ACO_TEST_H

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct test_case {
    const char *name;
    void (*run)(void);
} test_case;

#define TEST_CASE(function) {#function, function}

/* Failures recorded by the test that is currently running. */
static int test_failures;

static void test_fail(const char *file, int line, const char *message)
{
    ++test_failures;
    (void)fprintf(stderr, "    %s:%d: %s\n", file, line, message);
}

#define EXPECT(condition)                                                                          \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            test_fail(__FILE__, __LINE__, "expected: " #condition);                                \
        }                                                                                          \
    } while (0)

#define REQUIRE(condition)                                                                         \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            test_fail(__FILE__, __LINE__, "required: " #condition);                                \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define EXPECT_EQ_SIZE(actual, expected)                                                           \
    do {                                                                                           \
        const size_t test_actual = (actual);                                                       \
        const size_t test_expected = (expected);                                                   \
        if (test_actual != test_expected) {                                                        \
            char test_message[160];                                                                \
            (void)snprintf(test_message, sizeof test_message, "%s is %zu, expected %zu", #actual,  \
                           test_actual, test_expected);                                            \
            test_fail(__FILE__, __LINE__, test_message);                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_NEAR(actual, expected, tolerance)                                                   \
    do {                                                                                           \
        const double test_actual = (actual);                                                       \
        const double test_expected = (expected);                                                   \
        if (!(fabs(test_actual - test_expected) <= (tolerance))) {                                 \
            char test_message[160];                                                                \
            (void)snprintf(test_message, sizeof test_message, "%s is %.12g, expected %.12g",       \
                           #actual, test_actual, test_expected);                                   \
            test_fail(__FILE__, __LINE__, test_message);                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_STATUS(expression, expected)                                                        \
    do {                                                                                           \
        const aco_status test_actual = (expression);                                               \
        if (test_actual != (expected)) {                                                           \
            char test_message[200];                                                                \
            (void)snprintf(test_message, sizeof test_message, "%s returned \"%s\", expected %s",   \
                           #expression, aco_status_string(test_actual), #expected);                \
            test_fail(__FILE__, __LINE__, test_message);                                           \
        }                                                                                          \
    } while (0)

#define REQUIRE_OK(expression)                                                                     \
    do {                                                                                           \
        const aco_status test_actual = (expression);                                               \
        if (test_actual != ACO_OK) {                                                               \
            char test_message[200];                                                                \
            (void)snprintf(test_message, sizeof test_message, "%s failed: %s", #expression,        \
                           aco_status_string(test_actual));                                        \
            test_fail(__FILE__, __LINE__, test_message);                                           \
            return;                                                                                \
        }                                                                                          \
    } while (0)

static int test_is_selected(const char *name, int argc, char **argv)
{
    if (argc <= 1) {
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int test_main(int argc, char **argv, const test_case *cases, size_t count)
{
    size_t ran = 0;
    size_t failed = 0;

    for (size_t i = 0; i < count; ++i) {
        if (!test_is_selected(cases[i].name, argc, argv)) {
            continue;
        }
        test_failures = 0;
        cases[i].run();
        (void)printf("[%s] %s\n", test_failures == 0 ? " ok " : "FAIL", cases[i].name);
        ++ran;
        if (test_failures != 0) {
            ++failed;
        }
    }

    (void)printf("%zu test(s) run, %zu failed\n", ran, failed);
    if (ran == 0) {
        (void)fprintf(stderr, "no test matches the given names\n");
        return 2;
    }
    return failed == 0 ? 0 : 1;
}

#endif /* ACO_TEST_H */
