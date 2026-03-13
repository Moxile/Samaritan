#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__AVX2__)
    #include <immintrin.h>
    #define USE_AVX2
#elif defined(__ARM_NEON)
    #include <arm_neon.h>
    #define USE_NEON
#endif

inline void simd_add_i16(int16_t* acc, const int16_t* weights, size_t hiddenSize) {
#if defined(USE_AVX2)
    for (size_t i = 0; i < hiddenSize; i += 16) {
        __m256i a = _mm256_loadu_si256((__m256i*)(acc + i));
        __m256i b = _mm256_loadu_si256((__m256i*)(weights + i));
        _mm256_storeu_si256((__m256i*)(acc + i), _mm256_add_epi16(a, b));
    }
#elif defined(USE_NEON)
    for (size_t i = 0; i < hiddenSize; i += 8) {
        int16x8_t a = vld1q_s16(acc + i);
        int16x8_t b = vld1q_s16(weights + i);
        vst1q_s16(acc + i, vaddq_s16(a, b));
    }
#else
    for (size_t i = 0; i < hiddenSize; i++)
        acc[i] += weights[i];
#endif
}

inline void simd_sub_i16(int16_t* acc, const int16_t* weights, size_t hiddenSize) {
#if defined(USE_AVX2)
    for (size_t i = 0; i < hiddenSize; i += 16) {
        __m256i a = _mm256_loadu_si256((__m256i*)(acc + i));
        __m256i b = _mm256_loadu_si256((__m256i*)(weights + i));
        _mm256_storeu_si256((__m256i*)(acc + i), _mm256_sub_epi16(a, b));
    }
#elif defined(USE_NEON)
    for (size_t i = 0; i < hiddenSize; i += 8) {
        int16x8_t a = vld1q_s16(acc + i);
        int16x8_t b = vld1q_s16(weights + i);
        vst1q_s16(acc + i, vsubq_s16(a, b));
    }
#else
    for (size_t i = 0; i < hiddenSize; i++)
        acc[i] -= weights[i];
#endif
}
