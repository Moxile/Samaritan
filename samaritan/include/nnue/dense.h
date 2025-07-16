#pragma once
#include <stdexcept>

template<size_t InputSize, size_t NodeCount>
class DenseLayer {
public:
    int** weights;
    int* biases;

    void initializeWeightsAndBiases() {
        for (size_t i = 0; i < NodeCount; ++i) {
            for (size_t j = 0; j < InputSize; ++j) {
                weights[i][j] = 0;  // Initialize with proper values
            }
            biases[i] = 0;
        }
    }

    DenseLayer() {
        weights = new int*[NodeCount];
        for (size_t i = 0; i < NodeCount; ++i) {
            weights[i] = new int[InputSize];
        }
        biases = new int[NodeCount];
        initializeWeightsAndBiases();
    }

    ~DenseLayer() {
        for (size_t i = 0; i < NodeCount; ++i) {
            delete[] weights[i];
        }
        delete[] weights;
        delete[] biases;
    }

    int* forward(const int* input) const {
        int* output = new int[NodeCount];
        for (size_t i = 0; i < NodeCount; ++i) {
            int sum = 0;
            for (size_t j = 0; j < InputSize; ++j) {
                sum += weights[i][j] * input[j];
            }
            output[i] = sum + biases[i];
        }
        return output;
    }
};