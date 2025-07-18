#pragma once

#include "nnue/activation/ReLu.h"
#include <stdexcept>

template <typename T>
class DenseLayer
{
private:
    size_t nodeCount_;
    size_t inputSize_;
    std::vector<T> weights_;
    std::vector<T> biases_;

public:
    DenseLayer(size_t nodeCount, size_t inputSize)
        : nodeCount_(nodeCount), inputSize_(inputSize), weights_(nodeCount * inputSize), biases_(inputSize)
    {
        if (nodeCount == 0 || inputSize == 0)
        {
            throw std::invalid_argument("NodeCount and InputSize must be greater than zero.");
        }

        std::fill(weights_.begin(), weights_.end(), 0);
        std::fill(biases_.begin(), biases_.end(), 0);
    }

    void setWeights(const std::vector<T> &weights)
    {
        if (weights.size() != nodeCount_ * inputSize_)
        {
            throw std::invalid_argument("Weights size does not match layer configuration.");
        }
        weights_ = weights;
    }

    void setBiases(const std::vector<T> &biases)
    {
        if (biases.size() != nodeCount_)
        {
            throw std::invalid_argument("Biases size does not match layer node count.");
        }
        biases_ = biases;
    }

    std::vector<T> forward(const std::vector<T> &input) const
    {
        if (input.size() != inputSize_)
        {
            throw std::invalid_argument("Input size does not match layer input size.");
        }

        std::vector<T> output(nodeCount_);

        for (size_t i = 0; i < nodeCount_; ++i)
        {
            size_t index = i * inputSize_;
            T sum = 0;
            for (size_t j = 0; j < inputSize_; ++j)
            {
                sum += weights_[index + j] * input[j];
            }
            output[i] = sum + biases_[i];
        }

        return output;
    }

    void single_forward_add(const std::vector<T> &output, int node)
    {
        for(size_t i = 0; i < output.size(); i++)
        {
            output[i] += weights_[i][node];
        }
    }

    void single_forward_rem(const std::vector<T> &output, int node)
    {
        for(size_t i = 0; i < output.size(); i++)
        {
            output[i] -= weights_[i][node];
        }
    }
};