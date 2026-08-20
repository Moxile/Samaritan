#pragma once

// The four per-player accumulators and the forward pass.
//
// Currently a full refresh on every evaluation: each accumulator is rebuilt
// from the bias plus one weight row per non-king piece. That is ~40 rows of 128
// floats per perspective. Incremental updates (add/subtract the two rows a move
// changes, per perspective) are the obvious next step, but a full refresh is
// the correct-by-construction version and it doubles as the oracle the
// incremental path will be tested against.

#include <algorithm>
#include <array>
#include <cmath>

#include "nnue/network.h"
#include "board.h"

namespace nnue {

// One piece that moved, appeared or disappeared. Recorded by Position::move so
// the evaluator can update without re-reading the whole board.
//
// Kings are not on the piece side of a HalfKP feature, so a king *moving*
// changes no piece feature at all -- it only changes its own perspective's
// king square, which invalidates that one accumulator entirely. That is why
// king moves carry a refresh colour instead of a from/to pair.
struct DirtyPiece
{
    int from = -1;             // -1 when the piece appeared (nothing to remove)
    int to   = -1;             // -1 when the piece vanished (nothing to add)
    PieceType type = NONE_PIECE;
    PieceColor colour = NONE_COLOR;
};

struct DirtyState
{
    DirtyPiece pieces[4];      // mover, captured/en-passant victim, castling rook
    int count = 0;
    int refreshColour = -1;    // perspective needing a full rebuild, or -1

    void add(int from, int to, PieceType type, PieceColor colour)
    {
        pieces[count++] = DirtyPiece{from, to, type, colour};
    }
};

struct Accumulators
{
    std::array<std::array<float, ACC_SIZE>, 4> acc{};

    void refresh(const Board &board)
    {
        for (int p = 0; p < 4; ++p) refreshOne(board, p);
    }

    void refreshOne(const Board &board, int p)
    {
        const Network &net = network();
        std::copy(net.ftBias.begin(), net.ftBias.end(), acc[p].begin());
        emitFeatures(board, p, [&](int f) {
            const float *row = net.featureRow(f);
            for (int i = 0; i < ACC_SIZE; ++i) acc[p][i] += row[i];
        });
    }

    // These are elementwise, not reductions, so they vectorise freely.
    void addFeature(int p, int f)
    {
        const float *__restrict row = network().featureRow(f);
        float *__restrict a = acc[p].data();
        for (int i = 0; i < ACC_SIZE; ++i) a[i] += row[i];
    }

    void subFeature(int p, int f)
    {
        const float *__restrict row = network().featureRow(f);
        float *__restrict a = acc[p].data();
        for (int i = 0; i < ACC_SIZE; ++i) a[i] -= row[i];
    }

    // Build this accumulator from `prev` plus the changes `dirty` describes.
    // `board` must already reflect the move, since a king move rebuilds that
    // perspective straight from it.
    void applyFrom(const Accumulators &prev, const Board &board, const DirtyState &dirty)
    {
        acc = prev.acc;

        for (int p = 0; p < 4; ++p)
        {
            if (p == dirty.refreshColour) continue;      // rebuilt below

            // A perspective whose king has just been captured contributes no
            // features at all, so its accumulator collapses to the bias. Simply
            // skipping it would leave the pre-capture values in place, which is
            // where the incremental path diverged from a full refresh.
            const int kingLoc = board.kingTracker[p];
            if (kingLoc < 0) { refreshOne(board, p); continue; }
            const int kingLive = liveIndex(kingLoc, p);
            if (kingLive < 0) { refreshOne(board, p); continue; }

            for (int i = 0; i < dirty.count; ++i)
            {
                const DirtyPiece &d = dirty.pieces[i];
                const int type = trainerPieceType(d.type);
                if (type == 5) continue;                 // kings are not features
                const int c   = ctz(static_cast<unsigned int>(d.colour));
                const int rel = (c - p + 4) % 4;

                if (d.from >= 0) subFeature(p, featureIndex(kingLive, liveIndex(d.from, p), rel, type));
                if (d.to   >= 0) addFeature(p, featureIndex(kingLive, liveIndex(d.to,   p), rel, type));
            }
        }

        if (dirty.refreshColour >= 0) refreshOne(board, dirty.refreshColour);
    }
};

// Raw network output, before scaling to centipawns. Exposed so the parity test
// can compare against PyTorch without a rounding step in the way.
inline float forwardRaw(const Accumulators &a, PieceColor stm)
{
    const Network &net = network();
    const int s = ctz(static_cast<unsigned int>(stm));

    // Concatenate in turn order starting from the side to move, matching the
    // trainer's perm buffer: row j is player (stm + j) % 4.
    float x[CONCAT];
    for (int j = 0; j < 4; ++j)
    {
        const auto &src = a.acc[(s + j) & 3];
        for (int i = 0; i < ACC_SIZE; ++i)
            x[j * ACC_SIZE + i] = std::clamp(src[i], 0.0f, 1.0f);
    }

    // ff1 is 32 x 512 = 16384 multiply-accumulates. Written as one running sum
    // it is latency-bound: floating-point addition is not associative, so the
    // compiler may not reassociate the reduction and every add waits on the
    // previous one. Four independent partial sums break that dependency chain
    // and let the vectoriser work.
    //
    // This does change the summation order, and therefore the last bits of the
    // result -- which is why the PyTorch parity test compares to a tolerance
    // rather than for equality.
    float h[FF_SIZE];
    for (int o = 0; o < FF_SIZE; ++o)
    {
        const float *w = net.ff1W.data() + static_cast<size_t>(o) * CONCAT;
        float s[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0; i < CONCAT; i += 8)
            for (int k = 0; k < 8; ++k) s[k] += w[i + k] * x[i + k];
        const float sum = net.ff1B[o]
                        + (((s[0] + s[1]) + (s[2] + s[3])) + ((s[4] + s[5]) + (s[6] + s[7])));
        h[o] = std::clamp(sum, 0.0f, 1.0f);
    }

    float out = net.ff2B;
    for (int o = 0; o < FF_SIZE; ++o) out += net.ff2W[o] * h[o];

    return out * SCALING;
}

// Evaluation in centipawns, from the side-to-move's team perspective.
// Rounded, not truncated: truncation biases every score toward zero.
inline int forward(const Accumulators &a, PieceColor stm)
{
    return static_cast<int>(std::lround(forwardRaw(a, stm)));
}

} // namespace nnue
