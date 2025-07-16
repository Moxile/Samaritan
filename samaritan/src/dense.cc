
#include <gtest/gtest.h>

#include "nnue/dense.h"

using namespace samaritan;

TEST(TestDense, Propagate)
{
    constexpr std::size_t inSize = 3;
    constexpr std::size_t outSize = 2;

    Dense<inSize, outSize> dense;

    std::cout << dense.forward({1, 2, 3}) << std::endl;

    EXPECT_EQ(output[0], 24);
    EXPECT_EQ(output[1], 52);
}