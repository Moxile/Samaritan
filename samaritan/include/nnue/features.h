#pragma once

// Feature encoding for the NNUE in ../NNUE4pc.
//
// Ported deliberately verbatim from NNUE4pc/loader.cpp (init_live_squares,
// rotate_square, init_rotation, emit_features). Trainer/engine feature drift is
// the classic NNUE bug, and copying rather than reimplementing removes it by
// construction. If the trainer's encoding changes, re-copy these four functions.
//
//   feature = (king_sq * 160 + sq) * 20 + rel * 5 + piece_type
//
//     king_sq, sq : LIVE_SQ[ROT[p][square]] -- board rotated into perspective
//                   p's frame, then compacted to 0..159
//     rel         : (colour - p + 4) % 4    -- colour relative to p
//     piece_type  : 0..4                    -- kings excluded (HalfKP style)

#include <array>
#include <cstdint>

#include "board.h"

namespace nnue {

inline constexpr int BOARD_DIM   = 14;
inline constexpr int CORNER      = 3;
inline constexpr int NUM_SQUARES = BOARD_DIM * BOARD_DIM;   // 196, corners included
inline constexpr int NUM_LIVE    = 160;
inline constexpr int PIECE_KINDS = 4 * 5;                   // relative colour x piece type
inline constexpr int NUM_FEATURES = NUM_LIVE * NUM_LIVE * PIECE_KINDS;   // 512000

static_assert(NUM_FEATURES == 512000, "feature count must match the trainer");

// The four 3x3 corners are not part of the board and can never hold a piece.
constexpr bool isDeadSquare(int row, int col)
{
    return (row < CORNER || row >= BOARD_DIM - CORNER) &&
           (col < CORNER || col >= BOARD_DIM - CORNER);
}

// Dense 14x14 square -> compacted live index 0..159, or -1 for a dead corner.
inline constexpr auto LIVE_SQ = [] {
    std::array<int16_t, NUM_SQUARES> t{};
    int n = 0;
    for (int sq = 0; sq < NUM_SQUARES; ++sq)
        t[sq] = isDeadSquare(sq / BOARD_DIM, sq % BOARD_DIM) ? int16_t(-1) : int16_t(n++);
    return t;
}();

// One 90-degree step is (row, col) -> (13 - col, row): it moves every seat one
// place around the table, so applying it p times brings player p to red's seat.
constexpr int rotateSquare(int sq, int p)
{
    int row = sq / BOARD_DIM;
    int col = sq % BOARD_DIM;
    for (int i = 0; i < p; ++i)
    {
        const int nrow = BOARD_DIM - 1 - col;
        const int ncol = row;
        row = nrow;
        col = ncol;
    }
    return row * BOARD_DIM + col;
}

inline constexpr auto ROT = [] {
    std::array<std::array<int16_t, NUM_SQUARES>, 4> t{};
    for (int p = 0; p < 4; ++p)
        for (int sq = 0; sq < NUM_SQUARES; ++sq)
            t[p][sq] = static_cast<int16_t>(rotateSquare(sq, p));
    return t;
}();

// --- bridge between Samaritan's 16-wide mailbox and the trainer's 14x14 board ---
//
// Samaritan: loc = 16 * row + col, col in 1..14, row in 0..13.
// Trainer:   sq  = 14 * row + (col - 1).
inline constexpr auto MAILBOX_TO_DENSE = [] {
    std::array<int16_t, 224> t{};
    for (int loc = 0; loc < 224; ++loc)
    {
        const int row = loc / 16;
        const int col = loc % 16;
        t[loc] = (row < BOARD_DIM && col >= 1 && col <= BOARD_DIM)
               ? static_cast<int16_t>(row * BOARD_DIM + (col - 1))
               : int16_t(-1);
    }
    return t;
}();

// Samaritan mailbox square -> live index in perspective p's frame, or -1.
constexpr int liveIndex(int mailboxLoc, int p)
{
    const int dense = MAILBOX_TO_DENSE[mailboxLoc];
    if (dense < 0) return -1;
    return LIVE_SQ[ROT[p][dense]];
}

// PieceType is 1..6 (PAWN..KING); the trainer uses 0..5 with 5 == king.
constexpr int trainerPieceType(PieceType pie) { return static_cast<int>(pie) - 1; }

constexpr int featureIndex(int kingLive, int pieceLive, int rel, int type)
{
    return (kingLive * NUM_LIVE + pieceLive) * PIECE_KINDS + rel * 5 + type;
}

// ---------------------------------------------------------------------------
// Appends one feature index per non-king piece, from perspective p.
// Mirrors emit_features() in NNUE4pc/loader.cpp.
// ---------------------------------------------------------------------------
template <typename Out>
inline void emitFeatures(const Board &board, int p, Out &&out)
{
    const int kingLoc = board.kingTracker[p];
    if (kingLoc < 0) return;                 // that king has been captured
    const int kingLive = liveIndex(kingLoc, p);
    if (kingLive < 0) return;

    for (int c = 0; c < 4; ++c)
        for (int i = 0; i < board.pieceCount[c]; ++i)
        {
            const int loc = board.pieceList[c][i];
            const int type = trainerPieceType(board.pieceMailbox[loc]);
            if (type == 5) continue;         // HalfKP: kings are not on the piece side

            // The + 4 matters: C++ '%' on a negative left operand is negative,
            // which would produce a negative feature index.
            const int rel = (c - p + 4) % 4;
            const int sq  = liveIndex(loc, p);
            out(featureIndex(kingLive, sq, rel, type));
        }
}

} // namespace nnue
