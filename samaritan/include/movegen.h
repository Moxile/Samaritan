#pragma once
#include <cstdint>
#include <algorithm>

#include "position.h"

// Search-ordering values (not evaluation values). Capturing a king is a
// terminal move and therefore has to sort ahead of every ordinary capture.
constexpr int piece_values[7] = {0, 100, 300, 400, 500, 1000, 100000};

const int MAX_MOVES = 512;

bool inCheck(const Position &pos, PieceColor color);

// --- draw rules -------------------------------------------------------------
// 4PC draws: threefold repetition, and 50 whole moves without a capture or a
// pawn move. A whole move is one turn for each of the four players, so the
// clock limit is 50 * 4 plies.
//
// NOTE: the reference engine uses 200 plies (a 50-move rule); we deliberately
// differ. This does not affect perft, which never consults draw state.
inline constexpr int MOVES_WITHOUT_PROGRESS = 50;
inline constexpr int HALFMOVE_LIMIT = MOVES_WITHOUT_PROGRESS * 4;

// Has the position at the top of the stack already occurred twice before?
// Only every fourth ply can match, because the key includes the side to move.
inline bool isRepetitionDraw(const Position &pos)
{
    const auto &states = pos.gameStates;
    if (states.size() < 9) return false;              // two full rounds minimum
    const auto &cur = states.back();

    const int last = static_cast<int>(states.size()) - 1;
    const int window = std::min(cur.halfmoveClock, cur.pliesFromNull);

    int seen = 0;
    for (int i = last - 4; i >= 0 && last - i <= window; i -= 4)
        if (states[i].zobristKey == cur.zobristKey && ++seen >= 2)
            return true;
    return false;
}

inline bool isDraw(const Position &pos)
{
    return pos.gameStates.back().halfmoveClock >= HALFMOVE_LIMIT
        || isRepetitionDraw(pos);
}
void computeCheckInfo(const Position &pos, PieceColor us, CheckInfo &info);

struct MoveList
{
    explicit MoveList(Position& pos);

    ExtMove *begin() { return moveList; }
    ExtMove *end() { return last; }
    const ExtMove *begin() const { return moveList; }
    const ExtMove *end() const { return last; }

    size_t size() const { return last - moveList; }

    bool contains(Move move) const { return std::find(begin(), end(), move) != end(); }

    void print() const;

    void sort()
    {
        std::sort(begin(), end(), [](ExtMove &a, ExtMove &b)
                  {
                      return a > b;
                  });
    }

private:
    ExtMove moveList[MAX_MOVES], *last;
};
