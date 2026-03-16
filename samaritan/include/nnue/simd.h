#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <vector>

static constexpr int HIDDEN_SIZE = 16;

template <typename T, std::size_t Align>
struct AlignedAllocator {
    using value_type = T;
    T* allocate(std::size_t n) {
        std::size_t bytes = (n * sizeof(T) + Align - 1) & ~(Align - 1);
        void* p = std::aligned_alloc(Align, bytes);
        if (!p) throw std::bad_alloc();
        return static_cast<T*>(p);
    }
    void deallocate(T* p, std::size_t) noexcept { std::free(p); }
    template <typename U> struct rebind { using other = AlignedAllocator<U, Align>; };
    bool operator==(const AlignedAllocator&) const noexcept { return true; }
    bool operator!=(const AlignedAllocator&) const noexcept { return false; }
};

struct alignas(64) AlignedArr16 {
    std::array<int16_t, HIDDEN_SIZE> v{};
    int16_t*       data()       { return v.data(); }
    const int16_t* data() const { return v.data(); }
    int16_t&       operator[](size_t i)       { return v[i]; }
    const int16_t& operator[](size_t i) const { return v[i]; }
    static constexpr size_t size() { return HIDDEN_SIZE; }
    auto begin()       { return v.begin(); }
    auto end()         { return v.end(); }
    auto begin() const { return v.begin(); }
    auto end()   const { return v.end(); }
};

using AlignedVec8 = std::vector<uint8_t, AlignedAllocator<uint8_t, 64>>;

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
        __m256i a = _mm256_load_si256((__m256i*)(acc + i));
        __m256i b = _mm256_load_si256((__m256i*)(weights + i));
        _mm256_store_si256((__m256i*)(acc + i), _mm256_add_epi16(a, b));
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
        __m256i a = _mm256_load_si256((__m256i*)(acc + i));
        __m256i b = _mm256_load_si256((__m256i*)(weights + i));
        _mm256_store_si256((__m256i*)(acc + i), _mm256_sub_epi16(a, b));
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
