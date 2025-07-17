#pragma once

constexpr float ReLu(float x) {
    return x > 0.0f ? x : 0.0f;
}