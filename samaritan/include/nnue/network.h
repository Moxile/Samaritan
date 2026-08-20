#pragma once

// Inference for the NNUE in ../NNUE4pc.
//
//   4 accumulators of 128 (one per player)
//     -> reordered so the side to move comes first:  [stm, stm+1, stm+2, stm+3]
//     -> clamp(0, 1), flattened to 512
//     -> ff1 Linear(512 -> 32), clamp(0, 1)
//     -> ff2 Linear(32 -> 1)
//     -> x 400   (centipawns)
//
// Weights are float32 on purpose. The trainer is float32, so this reproduces
// its arithmetic exactly and makes numerical parity testing a straight
// comparison. Quantising to int16 halves the memory and speeds up inference,
// but it introduces error that has to be budgeted for -- worth doing only once
// parity against PyTorch is established.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "nnue/features.h"

namespace nnue {

inline constexpr int ACC_SIZE = 128;
inline constexpr int FF_SIZE  = 32;
inline constexpr int CONCAT   = ACC_SIZE * 4;      // 512
inline constexpr int SCALING  = 400;

// Deterministic and fast: filling 65.5M weights with mt19937 is needlessly slow.
inline uint64_t splitmix64(uint64_t &x)
{
    uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

class Network
{
public:
    // [NUM_FEATURES][ACC_SIZE], row-major: row f is the accumulator contribution
    // of feature f. 512000 * 128 * 4 bytes = 262 MB, so it is process-wide and
    // allocated only on demand.
    std::vector<float> ftWeights;
    std::array<float, ACC_SIZE> ftBias{};

    std::array<float, FF_SIZE * CONCAT> ff1W{};
    std::array<float, FF_SIZE> ff1B{};
    std::array<float, FF_SIZE> ff2W{};
    float ff2B = 0.0f;

    bool ready() const { return !ftWeights.empty(); }
    bool isRandom() const { return random_; }
    const std::string &name() const { return name_; }

    // Bumped by every operation that changes what the weights are. Accumulators
    // record the generation they were built under, so a position that was left
    // with a stack from an older (or absent) network is detected and rebuilt
    // instead of being evaluated against weights it was never computed from.
    uint32_t generation() const { return generation_; }

    // Plumbing-only weights: exercises the real code path and the real memory
    // footprint, but says nothing about playing strength.
    void randomize(uint64_t seed = 42)
    {
        ftWeights.assign(static_cast<size_t>(NUM_FEATURES) * ACC_SIZE, 0.0f);

        // The trainer initialises the embedding uniform(-sigma, sigma) with
        // sigma = sqrt(1 / active_count); mirroring that keeps activations in a
        // sane range instead of saturating every clamp.
        const float sigma = 1.0f / 6.48f;          // sqrt(1/42)
        uint64_t s = seed;
        auto next = [&]() {
            return (static_cast<float>(splitmix64(s) >> 11) * (1.0f / 9007199254740992.0f)) * 2.0f - 1.0f;
        };

        for (auto &w : ftWeights) w = next() * sigma;
        for (auto &b : ftBias)    b = 0.0f;        // trainer zeroes this one

        const float k1 = 1.0f / 22.6f;             // ~sqrt(1/512), Linear default
        for (auto &w : ff1W) w = next() * k1;
        for (auto &b : ff1B) b = next() * k1;
        const float k2 = 1.0f / 5.66f;             // ~sqrt(1/32)
        for (auto &w : ff2W) w = next() * k2;
        ff2B = next() * k2;

        random_ = true;
        name_   = "random(seed " + std::to_string(seed) + ")";
        ++generation_;
    }

    void clear()
    {
        ftWeights.clear();
        ftWeights.shrink_to_fit();
        random_ = false;
        name_.clear();
        ++generation_;
    }

    // On-disk format (little-endian, float32 throughout), written by
    // bench/dump_network.cc and by the trainer's exporter:
    //
    //   char   magic[4]      "SNN1"
    //   int32  num_features
    //   int32  acc_size
    //   int32  ff_size
    //   float  ft_weights[num_features * acc_size]   row-major, row f = feature f
    //   float  ft_bias[acc_size]
    //   float  ff1_w[ff_size * acc_size * 4]         row-major, PyTorch Linear order
    //   float  ff1_b[ff_size]
    //   float  ff2_w[ff_size]
    //   float  ff2_b
    //
    // Every dimension is checked against the compiled-in architecture before a
    // single weight is read: a file from a differently-shaped trainer would
    // otherwise be read as a valid net and quietly evaluate nonsense. The load
    // is staged into locals and only committed once the whole file has been
    // read, so a truncated file leaves the previous network intact.
    bool load(const std::string &path, std::string *error = nullptr)
    {
        auto fail = [&](const std::string &msg) {
            if (error) *error = msg;
            return false;
        };

        std::ifstream f(path, std::ios::binary);
        if (!f) return fail("cannot open " + path);

        char magic[4] = {};
        f.read(magic, 4);
        if (!f || std::memcmp(magic, "SNN1", 4) != 0)
            return fail("not an SNN1 network file");

        int32_t nf = 0, as = 0, fs = 0;
        f.read(reinterpret_cast<char *>(&nf), 4);
        f.read(reinterpret_cast<char *>(&as), 4);
        f.read(reinterpret_cast<char *>(&fs), 4);
        if (!f) return fail("truncated header");
        if (nf != NUM_FEATURES || as != ACC_SIZE || fs != FF_SIZE)
            return fail("architecture mismatch: file is " + std::to_string(nf) + "x" +
                        std::to_string(as) + "x" + std::to_string(fs) + ", engine is " +
                        std::to_string(NUM_FEATURES) + "x" + std::to_string(ACC_SIZE) + "x" +
                        std::to_string(FF_SIZE));

        std::vector<float> ft(static_cast<size_t>(nf) * as);
        std::array<float, ACC_SIZE> ftB{};
        std::array<float, FF_SIZE * CONCAT> w1{};
        std::array<float, FF_SIZE> b1{}, w2{};
        float b2 = 0.0f;

        auto readInto = [&](void *dst, size_t floats) {
            f.read(static_cast<char *>(dst), static_cast<std::streamsize>(floats) * 4);
            return static_cast<bool>(f);
        };

        if (!readInto(ft.data(), ft.size()))  return fail("truncated feature weights");
        if (!readInto(ftB.data(), ftB.size())) return fail("truncated feature bias");
        if (!readInto(w1.data(), w1.size()))  return fail("truncated ff1 weights");
        if (!readInto(b1.data(), b1.size()))  return fail("truncated ff1 bias");
        if (!readInto(w2.data(), w2.size()))  return fail("truncated ff2 weights");
        if (!readInto(&b2, 1))                return fail("truncated ff2 bias");

        // Anything after ff2_b means the file was written by something that does
        // not agree with us about the format, even if the sizes lined up.
        char extra = 0;
        f.read(&extra, 1);
        if (f) return fail("trailing data after the network");

        ftWeights = std::move(ft);
        ftBias    = ftB;
        ff1W      = w1;
        ff1B      = b1;
        ff2W      = w2;
        ff2B      = b2;
        random_   = false;
        name_     = path;
        ++generation_;
        return true;
    }

    // The same format, so a randomised or loaded net can be handed to the
    // trainer-side parity checks.
    bool save(const std::string &path) const
    {
        if (!ready()) return false;
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        const int32_t nf = NUM_FEATURES, as = ACC_SIZE, fs = FF_SIZE;
        f.write("SNN1", 4);
        f.write(reinterpret_cast<const char *>(&nf), 4);
        f.write(reinterpret_cast<const char *>(&as), 4);
        f.write(reinterpret_cast<const char *>(&fs), 4);
        f.write(reinterpret_cast<const char *>(ftWeights.data()),
                static_cast<std::streamsize>(ftWeights.size()) * 4);
        f.write(reinterpret_cast<const char *>(ftBias.data()), as * 4);
        f.write(reinterpret_cast<const char *>(ff1W.data()),
                static_cast<std::streamsize>(ff1W.size()) * 4);
        f.write(reinterpret_cast<const char *>(ff1B.data()), fs * 4);
        f.write(reinterpret_cast<const char *>(ff2W.data()), fs * 4);
        f.write(reinterpret_cast<const char *>(&ff2B), 4);
        return static_cast<bool>(f);
    }

    const float *featureRow(int f) const
    {
        return ftWeights.data() + static_cast<size_t>(f) * ACC_SIZE;
    }

private:
    bool random_ = false;
    uint32_t generation_ = 0;
    std::string name_;
};

inline Network &network()
{
    static Network net;
    return net;
}

} // namespace nnue
