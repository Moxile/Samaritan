#pragma once
#include <vector>
#include <stdexcept>

template<size_t InputSize, size_t NodeCount>
class DenseLayer {
private:
    std::vector<std::vector<float>> weights;
    std::vector<float> biases;

    void initializeWeightsAndBiases() {
        weights.resize(NodeCount, std::vector<float>(InputSize));
        biases.resize(NodeCount, 0.0f);
        biases[0] = 10;
        biases[1] = 20;
        weights[0] = {1, 2, 3};
        weights[1] = {4, 5, 6};
    }

public:
    DenseLayer() {
        weights.resize(NodeCount, std::vector<float>(InputSize));
        biases.resize(NodeCount);
        initializeWeightsAndBiases();
    }

    std::vector<float> forward(const std::vector<float>& input) const {
        if (input.size() != InputSize) {
            throw std::invalid_argument("Input size does not match expected input_size");
        }

        std::vector<float> output(NodeCount, 0.0f);
        for (size_t i = 0; i < NodeCount; ++i) {
            float sum = 0.0f;
            for (size_t j = 0; j < InputSize; ++j) {
                sum += weights[i][j] * input[j];
            }
            output[i] = sum + biases[i];
        }
        return output;
    }
};