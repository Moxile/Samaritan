#include <gtest/gtest.h>
#include "nnue/dense.h"

TEST(TestDense, Forward)
{
    constexpr std::size_t inSize = 3;
    constexpr std::size_t outSize = 2;

    DenseLayer<inSize, outSize> dense;
    
    // Example initialization
    dense.biases[0] = 10;
    dense.biases[1] = 20;
    // Initialize weights properly
    dense.weights[0][0] = 1; dense.weights[0][1] = 2; dense.weights[0][2] = 3;
    dense.weights[1][0] = 4; dense.weights[1][1] = 5; dense.weights[1][2] = 6;

    int input[] = {1, 2, 3};
    int* output = dense.forward(input);

    EXPECT_EQ(output[0], 24);
    EXPECT_EQ(output[1], 52);

    delete[] output;  // Don't forget to free the memory
}