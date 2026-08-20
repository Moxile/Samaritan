#pragma once

#include "position.h"
#include "nnue/accumulator.h"

// Interim hand-crafted evaluation.
//
// This replaces the previous NNUE, whose weights were never loaded from disk
// (loadWeights was commented out), so the search was in fact being guided by
// mt19937(42) random numbers. Material is not much of an evaluation, but it is
// a real one, and it gives the incoming NNUE a baseline to be measured against.
//
// Score is returned from the side-to-move's *team* perspective, which is what
// negamax expects: the turn order R -> B -> Y -> G strictly alternates teams
// RY / BG, so flipping the sign every ply is correct.

constexpr int PIECE_VALUE[7] = {
    0,      // NONE
    100,    // PAWN
    300,    // KNIGHT
    400,    // BISHOP
    500,    // ROOK
    1000,   // QUEEN
    0,      // KING -- terminal states are scored by the search, not here
};

// Centipawns per pseudo-legal destination. Mobility is deliberately modest
// beside material, but weighted by the sliding piece's reach: freeing a queen
// matters most, a rook less, and a bishop least. Pawns, knights and kings stay
// at zero to keep this first HCE extension intentionally small.
constexpr int MOBILITY_WEIGHT[7] = {
    0,      // NONE
    0,      // PAWN
    0,      // KNIGHT
    1,      // BISHOP
    2,      // ROOK
    4,      // QUEEN
    0,      // KING
};

// Material only. Kept as the fallback and as a sanity baseline for the NNUE:
// if the net cannot beat counting material, something is wrong with it.
inline int evaluateMaterial(const Position &pos)
{
    const PieceColor myTeam = getTeam(pos.gameStates.back().curTurn);
    const Board &b = pos.board;

    int score = 0;
    for (int c = 0; c < 4; ++c)
    {
        const PieceColor col = static_cast<PieceColor>(1 << c);
        const int sign = isOnTeam(col, myTeam) ? 1 : -1;
        for (int i = 0; i < b.pieceCount[c]; ++i)
            score += sign * PIECE_VALUE[b.pieceMailbox[b.pieceList[c][i]]];
    }
    return score;
}

// Count pseudo-legal sliding destinations. This intentionally ignores pins and
// check: mobility is a positional signal, not a second legality generator.
// Empty squares count, an enemy-occupied square counts once as a possible
// capture, and either team's own piece stops the ray without counting.
inline int sliderMobility(const Board &board, int loc, PieceType piece,
                          PieceColor pieceTeam)
{
    if (piece != BISHOP && piece != ROOK && piece != QUEEN) return 0;

    const int offsetRow = static_cast<int>(piece) + 3; // B/R/Q -> offsets[6/7/8]
    int mobility = 0;
    for (int i = 0; i < offsetsNum[offsetRow]; ++i)
    {
        const int step = offsets[offsetRow][i];
        for (int to = loc + step; !isInvalidLocation(to); to += step)
        {
            if (board.isEmpty(to))
            {
                ++mobility;
                continue;
            }
            if (!isOnTeam(board.colorMailbox[to], pieceTeam)) ++mobility;
            break;
        }
    }
    return mobility;
}

inline int evaluateMobility(const Position &pos)
{
    const PieceColor myTeam = getTeam(pos.gameStates.back().curTurn);
    const Board &board = pos.board;
    int score = 0;

    for (int c = 0; c < 4; ++c)
    {
        const PieceColor colour = static_cast<PieceColor>(1 << c);
        const PieceColor team = getTeam(colour);
        const int sign = team == myTeam ? 1 : -1;
        for (int i = 0; i < board.pieceCount[c]; ++i)
        {
            const int loc = board.pieceList[c][i];
            const PieceType piece = board.pieceMailbox[loc];
            score += sign * MOBILITY_WEIGHT[piece]
                   * sliderMobility(board, loc, piece, team);
        }
    }
    return score;
}

inline int evaluateHCE(const Position &pos)
{
    return evaluateMaterial(pos) + evaluateMobility(pos);
}

// Dispatch: the NNUE when weights are present, material otherwise. Perft and
// the test suite never load weights, so they never pay for the network.
inline int evaluate(const Position &pos)
{
    if (!nnue::network().ready()) return evaluateHCE(pos);
    if (pos.accValid())
        return nnue::forward(pos.accStack.back(), pos.gameStates.back().curTurn);

    // No usable stack: either evaluation was switched on mid-game, or the
    // network was replaced under a stack built from the old weights. Rebuilding
    // is the only honest answer -- an accumulator from different weights is not
    // stale by a little, it is a different function.
    nnue::Accumulators a;
    a.refresh(pos.board);
    return nnue::forward(a, pos.gameStates.back().curTurn);
}

// Always-correct reference evaluation, used to test the incremental path.
inline int evaluateFullRefresh(const Position &pos)
{
    if (!nnue::network().ready()) return evaluateHCE(pos);
    nnue::Accumulators a;
    a.refresh(pos.board);
    return nnue::forward(a, pos.gameStates.back().curTurn);
}
