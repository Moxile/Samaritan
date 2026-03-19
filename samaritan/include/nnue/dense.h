#pragma once

#include "nnue/simd.h"
#include <cassert>
#include <cmath>
#include <random>

class AccumulatorLayer {
    std::vector<int16_t, AlignedAllocator<int16_t, 64>> weights; // [Feature * HIDDEN_SIZE]
    AlignedArr16 biases;

    static constexpr int QA = 255;

public:
    AccumulatorLayer(size_t feature_count) : weights(feature_count * HIDDEN_SIZE, 0) {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int16_t> dist(-255, 255);
        for (auto& w : weights) w = dist(rng);
        for (auto& b : biases)  b = dist(rng);
    }

    void add(AlignedArr16& acc, int feature) {
        simd_add_i16(acc.data(), &weights[feature * HIDDEN_SIZE], HIDDEN_SIZE);
    }

    void rem(AlignedArr16& acc, int feature) {
        simd_sub_i16(acc.data(), &weights[feature * HIDDEN_SIZE], HIDDEN_SIZE);
    }

    void refresh(AlignedArr16& acc, const AlignedVec8& input) {
        for (size_t i = 0; i < HIDDEN_SIZE; i++)
            acc[i] = biases[i];
        for (size_t f = 0; f < input.size(); f++)
            if (input[f])
                add(acc, f);
    }

    void loadFromFloats(const float* w, const float* b) {
        for (size_t i = 0; i < weights.size(); i++)
            weights[i] = static_cast<int16_t>(std::round(w[i] * QA));
        for (size_t i = 0; i < HIDDEN_SIZE; i++)
            biases[i] = static_cast<int16_t>(std::round(b[i] * QA));
    }
};

class OutputLayer {
    AlignedArr16 weights;
    int32_t bias;

    static constexpr int QA = 255;
    static constexpr int QB = 64;

public:
    OutputLayer() : bias(0) {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int16_t> dist(-255, 255);
        for (auto& w : weights) w = dist(rng);
    }

    int32_t forward(const AlignedArr16& acc) {
        int32_t sum = bias;
        for (size_t i = 0; i < HIDDEN_SIZE; i++) {
            int32_t clipped = std::clamp((int32_t)acc[i], (int32_t)0, (int32_t)QA);
            sum += clipped * weights[i];
        }
        return sum / (QA * QB);
    }

    void loadFromFloats(const float* w, float b) {
        for (size_t i = 0; i < HIDDEN_SIZE; i++)
            weights[i] = static_cast<int16_t>(std::round(w[i] * QB));
        bias = static_cast<int32_t>(std::round(b * QA * QB));
    }
};
