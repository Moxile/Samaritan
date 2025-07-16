#include <gtest/gtest.h>
#include "nnue/dense.h"

TEST(TestDense, Forward)
{
    constexpr std::size_t inSize = 3;
    constexpr std::size_t outSize = 2;

    DenseLayer<float> dense{inSize, outSize};
    
    // Example initialization
    dense.setBiases({10, 20, 0});
    dense.setWeights({1, 2, 3, 4, 5, 6});

    std::vector<float> input({1, 2});
    std::vector<float> output = dense.forward(input);

    EXPECT_EQ(output[0], 15);
    EXPECT_EQ(output[1], 31);
}