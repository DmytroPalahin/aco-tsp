#include "aco/aco.h"

#include "test.h"

/* First outputs of xoshiro256** from the state {1, 2, 3, 4} (reference implementation). */
static void test_matches_reference_vector(void)
{
    static const uint64_t expected[] = {
        UINT64_C(11520),
        UINT64_C(0),
        UINT64_C(1509978240),
        UINT64_C(1215971899390074240),
        UINT64_C(1216172134540287360),
        UINT64_C(607988272756665600),
        UINT64_C(16172922978634559625),
        UINT64_C(8476171486693032832),
        UINT64_C(10595114339597558777),
        UINT64_C(2904607092377533576),
    };
    aco_rng rng = {{1, 2, 3, 4}};

    for (size_t i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
        EXPECT(aco_rng_next(&rng) == expected[i]);
    }
}

/* SplitMix64 from seed 0 is a published test vector; it defines our seeding. */
static void test_seeding_uses_splitmix64(void)
{
    aco_rng rng;

    aco_rng_seed(&rng, 0);
    EXPECT(rng.state[0] == UINT64_C(0xE220A8397B1DCDAF));
    EXPECT(rng.state[1] == UINT64_C(0x6E789E6AA1B965F4));
    EXPECT(rng.state[2] == UINT64_C(0x06C45D188009454F));
    EXPECT(rng.state[3] == UINT64_C(0xF88BB8A8724C81EC));
}

static void test_same_seed_same_sequence(void)
{
    aco_rng first;
    aco_rng second;

    aco_rng_seed(&first, 2023);
    aco_rng_seed(&second, 2023);
    for (int i = 0; i < 1000; ++i) {
        EXPECT(aco_rng_next(&first) == aco_rng_next(&second));
    }
}

static void test_different_seeds_differ(void)
{
    aco_rng first;
    aco_rng second;
    int equal = 0;

    aco_rng_seed(&first, 1);
    aco_rng_seed(&second, 2);
    for (int i = 0; i < 100; ++i) {
        equal += aco_rng_next(&first) == aco_rng_next(&second);
    }
    EXPECT(equal == 0);
}

static void test_uniform_stays_in_unit_interval(void)
{
    aco_rng rng;
    double sum = 0.0;
    const int samples = 100000;

    aco_rng_seed(&rng, 7);
    for (int i = 0; i < samples; ++i) {
        const double value = aco_rng_uniform(&rng);

        EXPECT(value >= 0.0 && value < 1.0);
        sum += value;
    }
    /* The standard error of the mean is about 0.0009 here. */
    EXPECT_NEAR(sum / samples, 0.5, 0.01);
}

static void test_below_respects_bound(void)
{
    aco_rng rng;
    size_t counts[7] = {0};
    const size_t samples = 70000;

    aco_rng_seed(&rng, 99);
    for (size_t i = 0; i < samples; ++i) {
        const size_t value = aco_rng_below(&rng, 7);

        REQUIRE(value < 7);
        ++counts[value];
    }
    /* Each bucket expects 10000 hits with a standard deviation near 93. */
    for (size_t bucket = 0; bucket < 7; ++bucket) {
        EXPECT(counts[bucket] > 9500 && counts[bucket] < 10500);
    }
}

static void test_below_handles_edge_bounds(void)
{
    aco_rng rng;

    aco_rng_seed(&rng, 5);
    EXPECT_EQ_SIZE(aco_rng_below(&rng, 0), 0);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ_SIZE(aco_rng_below(&rng, 1), 0);
    }
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        TEST_CASE(test_matches_reference_vector),       TEST_CASE(test_seeding_uses_splitmix64),
        TEST_CASE(test_same_seed_same_sequence),        TEST_CASE(test_different_seeds_differ),
        TEST_CASE(test_uniform_stays_in_unit_interval), TEST_CASE(test_below_respects_bound),
        TEST_CASE(test_below_handles_edge_bounds),
    };

    return test_main(argc, argv, cases, sizeof cases / sizeof cases[0]);
}
