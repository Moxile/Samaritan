#pragma once

#include "chess.h"
#include "utility.h"
#include "movegen.h"

#include <cstring>
#include <tuple>
#include <vector>

static const std::string START_FEN =
    "R-0,0,0,0-1,1,1,1-1,1,1,1-0,0,0,0-0-"
    "3,yR,yN,yB,yK,yQ,yB,yN,yR,3/"
    "3,yP,yP,yP,yP,yP,yP,yP,yP,3/"
    "14/"
    "bR,bP,10,gP,gR/"
    "bN,bP,10,gP,gN/"
    "bB,bP,10,gP,gB/"
    "bQ,bP,10,gP,gK/"
    "bK,bP,10,gP,gQ/"
    "bB,bP,10,gP,gB/"
    "bN,bP,10,gP,gN/"
    "bR,bP,10,gP,gR/"
    "14/"
    "3,rP,rP,rP,rP,rP,rP,rP,rP,3/"
    "3,rR,rN,rB,rQ,rK,rB,rN,rR,3";

struct PieceSQ {
    int square;
    PieceType type;
    PieceColor color;
};

// Build a position from scratch without relying on FEN parsing.
// Requires initZobrist() to have been called.
inline void setupPosition(Position& pos, PieceColor turn,
                           const std::vector<PieceSQ>& pieces,
                           int castleRights = NO_CASTLING)
{
    std::memset(pos.board.mailbox, NONE_PIECE_COLOR, sizeof(pos.board.mailbox));
    std::fill(std::begin(pos.board.kingTracker), std::end(pos.board.kingTracker), Square::A1);
    std::fill(std::begin(pos.board.nonPawnPieceCount),
              std::end(pos.board.nonPawnPieceCount), 0);

    GameState state;
    state.curTurn = turn;
    state.castleRights = castleRights;
    state.zobristKey = 0;

    for (const auto& p : pieces) {
        pos.board.mailbox[p.square] = makePiece(p.type, p.color);
        Square psq = static_cast<Square>(p.square);
        if (p.type == KING)
            pos.board.kingTracker[p.color] = psq;
        if (p.type != PAWN)
            pos.board.nonPawnPieceCount[p.color]++;
        state.zobristKey ^= zobristPieces[board_table[p.square]]
                                         [p.type - 1]
                                         [(unsigned int)p.color];
    }

    state.zobristKey ^= zobristTurn[__builtin_ctz((unsigned int)turn)];
    for (int i = 0; i < 8; i++)
        if (castleRights & (1 << i))
            state.zobristKey ^= zobristCastle[i];

    pos.gameStates.clear();
    pos.gameStates.push_back(state);

    if (pos.useEval) {
        pos.refreshNNUE();
        pos.nnue.init_eval(turn);
    }
}

// Snapshot of the board arrays so we can compare before/after make+unmake.
struct BoardSnapshot {
    Piece mailbox[256];
    int   kingTracker[4];

    static BoardSnapshot capture(const Board& b) {
        BoardSnapshot s;
        std::memcpy(s.mailbox, b.mailbox, sizeof(s.mailbox));
        std::memcpy(s.kingTracker, b.kingTracker, sizeof(s.kingTracker));
        return s;
    }

    bool operator==(const BoardSnapshot& o) const {
        return std::memcmp(mailbox, o.mailbox, sizeof(mailbox)) == 0
            && std::memcmp(kingTracker, o.kingTracker, sizeof(kingTracker)) == 0;
    }
    bool operator!=(const BoardSnapshot& o) const { return !(*this == o); }
};

// Count moves originating from a specific square.
inline int countMovesFrom(const MoveList& moves, int square) {
    int n = 0;
    for (const auto& m : moves)
        if (m.from() == square) n++;
    return n;
}

// Check whether a move with the given origin+destination exists in the list.
inline bool hasMoveFromTo(const MoveList& moves, int from, int to) {
    for (const auto& m : moves)
        if (m.from() == from && m.to() == to) return true;
    return false;
}
